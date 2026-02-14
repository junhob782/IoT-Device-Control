/**
 * @file    main.c
 * @brief   T-MAP Tactical Console (CLI & System Entry Point)
 * @details 실시간 사용자의 명령을 파싱하고 핵심 엔진 로직(Correlation, Insert, Search)을 통제합니다.
 */

#include "common.h"
#include <string.h>

/* --- External Linkage --- */
extern TacticalTrack* create_track(int track_id, int threat_level);
extern void add_history_node(TacticalTrack* track, double lat, double lon, int timestamp);
extern void intercept_track(TacticalTrack* track);

extern void insert_track(BTreeNode** root, TacticalTrack* track);
extern TacticalTrack* search_btree(BTreeNode* node, int track_id);
extern void print_btree(BTreeNode* node, int level);
extern void scan_high_threat(BTreeNode* node, int threshold);
extern void free_btree(BTreeNode* node);

/**
 * @brief 콘솔 UI 출력 함수
 */
void print_help() {
    printf("\n=== [ T-MAP Tactical Console Commands ] ===\n");
    printf(" 1. ADD <ID> <Threat>  : Detect/Update Target\n");
    printf(" 2. SEARCH <ID>        : Find Target info\n");
    printf(" 3. SCAN <Threat>      : Scan High-Threat Targets\n");
    printf(" 4. KILL <ID>          : Intercept Target & Reclaim Memory\n");
    printf(" 5. SHOW               : Visualize Tree Structure\n");
    printf(" 6. EXIT               : Shutdown System\n");
    printf("===========================================\n");
}

/**
 * @brief 입력 버퍼를 깔끔하게 비우는 방어적 헬퍼 함수
 */
void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int main() {
    // Windows 메모리 릭 디텍터 (개발 환경용)
#ifdef _WIN32
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    BTreeNode* root = NULL;
    char command[16];
    int id, threat, threshold;
    int current_time = 0; // 시뮬레이션 타임스탬프

    printf("\n[SYSTEM] T-MAP Engine Booted successfully.\n");
    print_help();

    while (1) {
        printf("\nT-MAP> "); 
        if (scanf("%15s", command) != 1) break; // EOF 발생 시 안전 종료

        /* ========================================================
           [명령어 처리 분기 (Command Dispatcher)]
        ======================================================== */
        if (strcmp(command, "ADD") == 0) {
            if (scanf("%d %d", &id, &threat) != 2) {
                printf("[ERROR] Usage: ADD <ID> <Threat>\n");
                clear_input_buffer(); 
                continue;
            }
            
            // 1. 상관 처리 (Correlation)
            TacticalTrack* existing = search_btree(root, id);
            
            // CASE A: 묘비(Tombstone) 상태인 표적이 재출현 -> 부활(Re-allocate)
            if (existing != NULL && existing->status == TRACK_STATUS_DESTROYED) {
                 printf("[SYSTEM] Re-detecting previously destroyed target (ID: %d)...\n", id);
                 existing->threat_level = threat;
                 existing->status = TRACK_STATUS_ACTIVE;
                 add_history_node(existing, 37.5, 127.0, current_time);
            }
            // CASE B: 현재 추적 중인 표적 -> 위치 업데이트 (O(1) 속도로)
            else if (existing != NULL) {
                printf("[SYSTEM] Target ID %d found! Updating position...\n", id);
                add_history_node(existing, 37.5 + (current_time*0.01), 127.0, current_time);
            } 
            // CASE C: 완전 신규 표적 포착 -> 메모리 할당 및 B-Tree 삽입
            else {
                TacticalTrack* new_track = create_track(id, threat);
                add_history_node(new_track, 37.5, 127.0, current_time);
                insert_track(&root, new_track);
                printf("[SYSTEM] New Target (ID: %d) successfully registered.\n", id);
            }
            current_time += 10;

        } else if (strcmp(command, "SEARCH") == 0) {
            if (scanf("%d", &id) != 1) {
                printf("[ERROR] Usage: SEARCH <ID>\n");
                clear_input_buffer(); 
                continue;
            }
            TacticalTrack* result = search_btree(root, id);
            if (result != NULL) {
                if (result->status == TRACK_STATUS_DESTROYED) {
                    printf("  => [DESTROYED] Target ID %d was intercepted and neutralized.\n", id);
                } else {
                    printf("  => [FOUND] ID: %d | Threat: %d | Nodes: %d (Active)\n", 
                           result->track_id, result->threat_level, result->history_count);
                }
            } else {
                printf("  => [NOT FOUND] Target ID %d does not exist in the airspace.\n", id);
            }

        } else if (strcmp(command, "SCAN") == 0) {
            if (scanf("%d", &threshold) != 1) {
                printf("[ERROR] Usage: SCAN <Threat>\n");
                clear_input_buffer(); 
                continue;
            }
            printf("\n--- [ HIGH THREAT SCAN: Level %d or above ] ---\n", threshold);
            if (root != NULL) scan_high_threat(root, threshold);
            printf("-----------------------------------------------\n");

        } else if (strcmp(command, "KILL") == 0) {
            if (scanf("%d", &id) != 1) {
                printf("[ERROR] Usage: KILL <ID>\n");
                clear_input_buffer(); 
                continue;
            }
            TacticalTrack* target = search_btree(root, id);
            if (target != NULL) {
                if (target->status == TRACK_STATUS_DESTROYED) {
                    printf("[SYSTEM] Target ID %d is already confirmed destroyed.\n", id);
                } else {
                    printf("\n[ALERT] Missile Launched at Target %d!\n", id);
                    intercept_track(target); 
                }
            } else {
                printf("[ERROR] Cannot intercept. Target ID %d not found.\n", id);
            }

        } else if (strcmp(command, "SHOW") == 0) {
            printf("\n--- Current B-Tree Structure ---\n");
            if (root == NULL) printf("(Empty Tree)\n");
            else print_btree(root, 0);
            printf("--------------------------------\n");

        } else if (strcmp(command, "EXIT") == 0) {
            printf("[SYSTEM] Initiating Graceful Shutdown...\n");
            free_btree(root);
            printf("[SYSTEM] All memory successfully reclaimed. Bye.\n");
            break; 
            
        } else if (strcmp(command, "HELP") == 0) {
            print_help();
        } else {
            printf("[ERROR] Unknown Command. Type 'HELP'.\n");
            clear_input_buffer(); 
        }
    }
    return 0;
}