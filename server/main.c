#include "common.h"
#include "../common/packet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>      
#include <windows.h>    
#include <stdbool.h>
#include <math.h>
// ============================================================================
// [수정 완료] 통신 포트 완벽 분리 (충돌 방지)
// ============================================================================
#define SERVER_PORT 8080 // 서버 수신용 포트
#define CLIENT_PORT 9090 // 클라이언트 송신용 포트
#define TICK_RATE_MS 100
#define BASE_LAT 37.500000
#define BASE_LON 127.000000

#define MIN_LAT 37.450000
#define MAX_LAT 37.550000
#define MIN_LON 126.930000
#define MAX_LON 127.070000

#define MAX_ID_BUFFER 10000

BTreeNode* btree_root = NULL;
bool server_running = true;

int dir_lat[MAX_ID_BUFFER];
int dir_lon[MAX_ID_BUFFER];

extern void insert_track(BTreeNode** root, TacticalTrack* track);
extern TacticalTrack* create_track(int track_id, int threat_level);
extern void add_history_node(TacticalTrack* track, double lat, double lon, int timestamp);
extern void broadcast_btree(BTreeNode* node, SOCKET sock, struct sockaddr_in* addr);

void save_node_to_binary(BTreeNode* node, FILE* fp) {
    if (node == NULL) return;
    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf) save_node_to_binary(node->children[i], fp);
        TacticalTrack* track = node->tracks[i];
        if (track != NULL) {
            fwrite(&track->track_id, sizeof(int), 1, fp);
            fwrite(&track->threat_level, sizeof(int), 1, fp);
            fwrite(&track->status, sizeof(int), 1, fp);
            
            int h_count = 0;
            HistoryNode* curr = track->history_head;
            while (curr != NULL) { h_count++; curr = curr->next; }
            fwrite(&h_count, sizeof(int), 1, fp);
            
            curr = track->history_head;
            while (curr != NULL) {
                fwrite(&curr->lat, sizeof(double), 1, fp);
                fwrite(&curr->lon, sizeof(double), 1, fp);
                int time_dummy = 0; 
                fwrite(&time_dummy, sizeof(int), 1, fp);
                curr = curr->next;
            }
        }
    }
    if (!node->is_leaf) save_node_to_binary(node->children[node->num_keys], fp);
}

void load_system_state(BTreeNode** root) {
    FILE* fp = fopen("tmap_data.dat", "rb");
    if (fp == NULL) {
        printf("[SYSTEM] No previous database found. Booting fresh instance.\n");
        return;
    }
    printf("[SYSTEM] Loading tactical database from 'tmap_data.dat'...\n");
    
    int id, threat, status, count;
    while (fread(&id, sizeof(int), 1, fp) == 1) {
        fread(&threat, sizeof(int), 1, fp);
        fread(&status, sizeof(int), 1, fp);
        fread(&count, sizeof(int), 1, fp);
        
        TacticalTrack* new_track = create_track(id, threat);
        new_track->status = status;
        for (int i = 0; i < count; i++) {
            double lat, lon; int t;
            fread(&lat, sizeof(double), 1, fp);
            fread(&lon, sizeof(double), 1, fp);
            fread(&t, sizeof(int), 1, fp);
            add_history_node(new_track, lat, lon, t);
        }
        insert_track(root, new_track);
        printf(" -> [RESTORED] Track ID: %04d (Threat: %d)\n", id, threat);
    }
    fclose(fp);
    printf("[SYSTEM] Database load complete.\n");
}

void free_system_postorder(BTreeNode* node) {
    if (node == NULL) return;
    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++) free_system_postorder(node->children[i]);
    }
    for (int i = 0; i < node->num_keys; i++) {
        if (node->tracks[i] != NULL) {
            HistoryNode* curr = node->tracks[i]->history_head;
            while (curr != NULL) {
                HistoryNode* temp = curr;
                curr = curr->next;
                free(temp); 
            }
            node->tracks[i]->history_head = NULL; 
            free(node->tracks[i]); 
        }
    }
    free(node); 
}

bool kill_target(BTreeNode* node, int target_id) {
    if (node == NULL) return false;
    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf && kill_target(node->children[i], target_id)) return true;
        TacticalTrack* track = node->tracks[i];
        if (track != NULL && track->track_id == target_id) {
            track->status = 0; 
            return true;
        }
    }
    if (!node->is_leaf) return kill_target(node->children[node->num_keys], target_id);
    return false;
}

void simulate_flight(BTreeNode* node) {
    if (node == NULL) return;
    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf) simulate_flight(node->children[i]);
        TacticalTrack* track = node->tracks[i];
        
        if (track != NULL && track->status == 1 && track->history_tail != NULL) {
            int safe_id = track->track_id % MAX_ID_BUFFER;
            double base_s_lat = ((track->track_id % 5) - 2) * 0.00008;
            double base_s_lon = ((track->track_id % 7) - 3) * 0.00008;
            if (base_s_lat == 0 && base_s_lon == 0) { base_s_lat = 0.00006; base_s_lon = 0.00006; }

            // --------------------------------------------------------------
            // [신규] 1. 사인(Sine) 파동을 이용한 부드러운 가속/감속 엔진
            // 시간이 지남에 따라 속도가 원래 속도의 0.5배 ~ 1.5배 사이로 변동합니다.
            // --------------------------------------------------------------
            double speed_modifier = 5.5 + 4.5 * sin(track->history_count * 0.275);
            
            // --------------------------------------------------------------
            // [신규] 2. 난기류 및 회피 기동 (미세한 좌표 흔들림)
            // 매 프레임마다 무작위로 미세하게 경로가 틀어집니다.
            // --------------------------------------------------------------
            double noise_lat = ((rand() % 100) / 100.0 - 0.5) * 0.00025;
            double noise_lon = ((rand() % 100) / 100.0 - 0.5) * 0.00025;

            // 최종 이동량 계산 (기본속도 * 변속기어 * 방향) + 노이즈
            double move_lat = (base_s_lat * speed_modifier) * dir_lat[safe_id] + noise_lat;
            double move_lon = (base_s_lon * speed_modifier) * dir_lon[safe_id] + noise_lon;

            double next_lat = track->history_tail->lat + move_lat;
            double next_lon = track->history_tail->lon + move_lon;

            // 바운싱(화면 이탈 방지) 로직
            if (next_lat > MAX_LAT || next_lat < MIN_LAT) dir_lat[safe_id] *= -1;
            if (next_lon > MAX_LON || next_lon < MIN_LON) dir_lon[safe_id] *= -1;

            // 새 좌표 기록
            add_history_node(track, track->history_tail->lat + move_lat, 
                                    track->history_tail->lon + move_lon, 0);
        }
    }
    if (!node->is_leaf) simulate_flight(node->children[node->num_keys]);
}

int main() {
    printf("====================================================\n");
    printf("  T-MAP COMMAND CENTER CORE ENGINE [v10.0 FINAL]\n");
    printf("====================================================\n");

    for(int i = 0; i < MAX_ID_BUFFER; i++) { dir_lat[i] = 1; dir_lon[i] = 1; }
    load_system_state(&btree_root);

    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET server_socket = socket(AF_INET, SOCK_DGRAM, 0);

    // [수정] 서버는 자신의 수신 포트인 8080(SERVER_PORT)을 열고 기다립니다.
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY; 
    bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));

    u_long mode = 1; ioctlsocket(server_socket, FIONBIO, &mode);

    // [수정] 서버가 데이터를 쏠 목적지는 클라이언트의 9090(CLIENT_PORT) 입니다.
    struct sockaddr_in client_dest;
    client_dest.sin_family = AF_INET;
    client_dest.sin_port = htons(CLIENT_PORT);
    client_dest.sin_addr.s_addr = inet_addr("127.0.0.1");

    char cmd_buf[256]; int ptr = 0; memset(cmd_buf, 0, 256);
    printf("\nT-MAP> ");

    while (server_running) {
        int target_to_kill;
        struct sockaddr_in from; int flen = sizeof(from);
        if (recvfrom(server_socket, (char*)&target_to_kill, sizeof(int), 0, (struct sockaddr*)&from, &flen) > 0) {
            if (kill_target(btree_root, target_to_kill)) {
                printf("\n[C2 LINK] Target #%04d Destroyed by Client Command!\nT-MAP> ", target_to_kill);
            }
        }

        while (_kbhit()) {
            char ch = _getch();
            if (ch == '\r') {
                cmd_buf[ptr] = '\0';
                int id, threat;
                if (sscanf(cmd_buf, "ADD %d %d", &id, &threat) == 2) {
                    TacticalTrack* nt = create_track(id, threat);
                    add_history_node(nt, BASE_LAT, BASE_LON, 0);
                    insert_track(&btree_root, nt);
                    printf("\n[SYSTEM] Target #%04d Deployed (Threat: %d).\nT-MAP> ", id, threat);
                } else if (strcmp(cmd_buf, "EXIT") == 0) {
                    server_running = false;
                }
                ptr = 0; memset(cmd_buf, 0, 256);
            } else if (ch == '\b' && ptr > 0) { 
                ptr--; printf("\b \b"); 
            } else if (ptr < 254) { 
                cmd_buf[ptr++] = ch; printf("%c", ch); 
            }
        }

        simulate_flight(btree_root);
        // 서버의 최신 데이터를 9090 포트로 쏩니다.
        broadcast_btree(btree_root, server_socket, &client_dest);
        
        Sleep(TICK_RATE_MS);
    }

    printf("\n[SYSTEM] Saving session to 'tmap_data.dat'...\n");
    FILE* save_fp = fopen("tmap_data.dat", "wb");
    if (save_fp != NULL) { save_node_to_binary(btree_root, save_fp); fclose(save_fp); }
    printf("[SYSTEM] Emptying B-Tree (Post-order GC)...\n");
    free_system_postorder(btree_root);
    closesocket(server_socket); WSACleanup();
    printf("[SYSTEM] Engine Offline. Goodbye.\n");
    return 0;
}