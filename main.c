#include "common.h"
#include <string.h>

// [External Functions]
extern TacticalTrack* create_track(int track_id, int threat_level);
extern void add_history_node(TacticalTrack* track, double lat, double lon, int timestamp);
extern void insert_track(BTreeNode** root, TacticalTrack* track);
extern TacticalTrack* search_btree(BTreeNode* node, int track_id);
extern void print_btree(BTreeNode* node, int level);
extern void free_btree(BTreeNode* node);
extern void scan_high_threat(BTreeNode* node, int threshold);
// [새로 추가된 요격 함수]
extern void intercept_track(TacticalTrack* track);

void print_help() {
    printf("\n=== [ T-MAP Tactical Console Commands ] ===\n");
    printf(" 1. ADD <ID> <Threat>  : Detect/Update Target\n");
    printf(" 2. SEARCH <ID>        : Find Target info\n");
    printf(" 3. SCAN <Threat>      : Scan High-Threat Targets\n");
    printf(" 4. KILL <ID>          : [NEW] Intercept Target & Reclaim Memory\n");
    printf(" 5. SHOW               : Visualize Tree Structure\n");
    printf(" 6. EXIT               : Shutdown System\n");
    printf("===========================================\n");
}

int main() {
#ifdef _WIN32
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    BTreeNode* root = NULL;
    char command[10];
    int id, threat, threshold;
    int current_time = 0;

    printf("\n[SYSTEM] T-MAP Engine Booted successfully.\n");
    print_help();

    while (1) {
        printf("\nT-MAP> "); 
        scanf("%s", command);

        if (strcmp(command, "ADD") == 0) {
            if (scanf("%d %d", &id, &threat) != 2) {
                printf("[ERROR] Usage: ADD <ID> <Threat>\n");
                while(getchar() != '\n'); continue;
            }
            TacticalTrack* existing = search_btree(root, id);
            
            // 만약 이미 격추된 표적 번호가 다시 들어온다면? (새로 부활)
            if (existing != NULL && existing->threat_level == -1) {
                 printf("[SYSTEM] Re-detecting target on ID %d...\n", id);
                 existing->threat_level = threat;
                 add_history_node(existing, 37.5, 127.0, current_time);
            }
            // 기존 정상 표적 이동 업데이트
            else if (existing != NULL) {
                printf("[SYSTEM] Target ID %d found! Updating...\n", id);
                add_history_node(existing, 37.5 + (current_time*0.01), 127.0, current_time);
            } 
            // 아예 새로운 표적
            else {
                TacticalTrack* new_track = create_track(id, threat);
                add_history_node(new_track, 37.5, 127.0, current_time);
                insert_track(&root, new_track);
                printf("[SYSTEM] New Target (ID: %d) Inserted.\n", id);
            }
            current_time += 10;

        } else if (strcmp(command, "SEARCH") == 0) {
            if (scanf("%d", &id) != 1) {
                printf("[ERROR] Usage: SEARCH <ID>\n");
                while(getchar() != '\n'); continue;
            }
            TacticalTrack* result = search_btree(root, id);
            if (result != NULL) {
                // 파괴된 표적(Tombstone)인지 확인
                if (result->threat_level == -1) {
                    printf("  => [DESTROYED] Target ID %d was intercepted and neutralized.\n", id);
                } else {
                    printf("  => [FOUND] ID: %d | Threat: %d | Nodes: %d\n", 
                           result->track_id, result->threat_level, result->history_count);
                }
            } else {
                printf("  => [NOT FOUND] ID %d does not exist.\n", id);
            }

        } else if (strcmp(command, "SCAN") == 0) {
            if (scanf("%d", &threshold) != 1) {
                printf("[ERROR] Usage: SCAN <Threat>\n");
                while(getchar() != '\n'); continue;
            }
            printf("\n--- [ HIGH THREAT SCAN: Level %d or above ] ---\n", threshold);
            if (root != NULL) scan_high_threat(root, threshold);
            printf("-----------------------------------------------\n");

        // =========================================================
        // [새로 추가된 KILL(요격) 명령어 구역]
        // =========================================================
        } else if (strcmp(command, "KILL") == 0) {
            if (scanf("%d", &id) != 1) {
                printf("[ERROR] Usage: KILL <ID>\n");
                while(getchar() != '\n'); continue;
            }
            TacticalTrack* target = search_btree(root, id);
            if (target != NULL) {
                if (target->threat_level == -1) {
                    printf("[SYSTEM] Target ID %d is already destroyed.\n", id);
                } else {
                    printf("\n[ALERT] Missile Launched at Target %d!\n", id);
                    intercept_track(target); // 요격 및 메모리 반환 함수 호출!
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
            printf("[SYSTEM] Shutting down...\n");
            free_btree(root);
            break; 
        } else if (strcmp(command, "HELP") == 0) {
            print_help();
        } else {
            printf("[ERROR] Unknown Command. Type 'HELP'.\n");
            while(getchar() != '\n'); 
        }
    }
    return 0;
}