#include "common.h"
#include "../common/packet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>      // _kbhit(), _getch() 논블로킹 키보드 입력용
#include <windows.h>    // Sleep() 용도
#include <stdbool.h>

// ============================================================================
// 엔진 설정 상수 (매직 넘버 제거)
// ============================================================================
#define PORT 8080
#define TICK_RATE_MS 100
#define BASE_LAT 37.50
#define BASE_LON 127.00
#define SPEED_MULTIPLIER 0.00003

BTreeNode* btree_root = NULL;
bool server_running = true;

// 외부 함수 선언 (btree.c, track.c에 정의됨)
extern void insert_track(BTreeNode** root, TacticalTrack* track);
extern TacticalTrack* create_track(int track_id, int threat_level);
extern void add_history_node(TacticalTrack* track, double lat, double lon, int timestamp);
extern void broadcast_btree(BTreeNode* node, SOCKET sock, struct sockaddr_in* addr);

// ============================================================================
// [기능 1] 표적 파괴 (상태 변경) 탐색 함수
// B-Tree를 순회하며 특정 ID를 찾아 status를 0(파괴됨)으로 변경합니다.
// ============================================================================
bool kill_target(BTreeNode* node, int target_id) {
    if (node == NULL) return false;

    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf && kill_target(node->children[i], target_id)) return true;

        TacticalTrack* track = node->tracks[i];
        if (track != NULL && track->track_id == target_id) {
            track->status = 0; // 격추됨!
            return true;
        }
    }
    if (!node->is_leaf) return kill_target(node->children[node->num_keys], target_id);
    
    return false;
}

// ============================================================================
// [기능 2] 자동 비행 시뮬레이션 엔진
// ============================================================================
void simulate_flight(BTreeNode* node) {
    if (node == NULL) return;

    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf) simulate_flight(node->children[i]);

        TacticalTrack* track = node->tracks[i];
        // 살아있는(status == 1) 표적만 이동시킵니다.
        if (track != NULL && track->status == 1 && track->history_tail != NULL) {
            
            // ID 기반 의사 난수 속도 생성
            double speed_lat = ((track->track_id % 5) - 2) * SPEED_MULTIPLIER;
            double speed_lon = ((track->track_id % 7) - 3) * SPEED_MULTIPLIER;

            if (speed_lat == 0 && speed_lon == 0) {
                speed_lat = 0.00005;
                speed_lon = 0.00005;
            }

            track->history_tail->lat += speed_lat;
            track->history_tail->lon += speed_lon;
        }
    }
    if (!node->is_leaf) simulate_flight(node->children[node->num_keys]);
}

// ============================================================================
// [기능 3] 명령어 처리기 (Command Dispatcher)
// ============================================================================
void process_command(char* cmd) {
    int id, threat;

    if (sscanf(cmd, "ADD %d %d", &id, &threat) == 2) {
        TacticalTrack* new_track = create_track(id, threat);
        add_history_node(new_track, BASE_LAT, BASE_LON, 0);
        insert_track(&btree_root, new_track);
        printf("\n[SYSTEM] Target %d Launched (Threat: %d)!\n", id, threat);
    } 
    else if (sscanf(cmd, "KILL %d", &id) == 1) {
        if (kill_target(btree_root, id)) {
            printf("\n[SYSTEM] Target %d Intercepted and Destroyed!\n", id);
        } else {
            printf("\n[SYSTEM] Target %d not found or already destroyed.\n", id);
        }
    }
    else if (strcmp(cmd, "EXIT") == 0 || strcmp(cmd, "QUIT") == 0) {
        printf("\n[SYSTEM] Shutting down T-MAP Engine...\n");
        server_running = false;
    }
    else {
        printf("\n[ERROR] Unknown command. Use: ADD <id> <threat>, KILL <id>, EXIT\n");
    }
    printf("T-MAP> "); // 다음 명령어를 위한 프롬프트 출력
}

// ============================================================================
// 메인 루프
// ============================================================================
int main() {
    printf("====================================================\n");
    printf("  T-MAP Tactical Engine & Broadcaster [ONLINE]\n");
    printf("====================================================\n");
    printf("Commands:\n");
    printf("  - ADD <id> <threat> : Spawn a new target\n");
    printf("  - KILL <id>         : Destroy a target\n");
    printf("  - EXIT              : Shutdown server\n");
    printf("----------------------------------------------------\n");

    // 네트워크 초기화
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET server_socket = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(PORT);
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // 비동기 입력을 위한 버퍼 변수
    char input_buffer[256];
    int input_pos = 0;
    memset(input_buffer, 0, sizeof(input_buffer));

    printf("T-MAP> ");

    // 메인 엔진 루프
    while (server_running) {
        
        // 1. 비동기 키보드 입력 처리 (루프를 멈추지 않음!)
        while (_kbhit()) {
            char ch = _getch(); // 한 글자씩 가져옴
            
            if (ch == '\r') { // 엔터키를 누른 경우
                input_buffer[input_pos] = '\0';
                process_command(input_buffer); // 명령어 실행
                
                // 버퍼 초기화
                input_pos = 0;
                memset(input_buffer, 0, sizeof(input_buffer));
            } 
            else if (ch == '\b') { // 백스페이스를 누른 경우
                if (input_pos > 0) {
                    input_pos--;
                    input_buffer[input_pos] = '\0';
                    printf("\b \b"); // 화면에서 한 글자 지우기
                }
            } 
            else if (input_pos < 254) { // 일반 글자 타이핑
                input_buffer[input_pos++] = ch;
                printf("%c", ch); // 화면에 글자 표시
            }
        }

        // 2. 물리 엔진 업데이트 (100ms 마다 위치 갱신)
        simulate_flight(btree_root);

        // 3. 브로드캐스팅 (클라이언트 화면 갱신)
        broadcast_btree(btree_root, server_socket, &client_addr);

        // 4. 시스템 휴식 (CPU 점유율 방지 및 10FPS 유지)
        Sleep(TICK_RATE_MS);
    }

    // 시스템 안전 종료
    closesocket(server_socket);
    WSACleanup();
    printf("[SYSTEM] Engine Offline.\n");
    return 0;
}