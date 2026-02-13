#include "common.h"
#include <string.h>

// [External Functions]
extern TacticalTrack* create_track(int track_id, int threat_level);
extern void add_history_node(TacticalTrack* track, double lat, double lon, int timestamp);
extern void insert_track(BTreeNode** root, TacticalTrack* track);
extern TacticalTrack* search_btree(BTreeNode* node, int track_id);
extern void print_btree(BTreeNode* node, int level);
extern void free_btree(BTreeNode* node);

void print_help() {
    printf("\n=== [ T-MAP Tactical Console Commands ] ===\n");
    printf(" 1. ADD <ID> <Threat>  : Create & Insert Target\n");
    printf(" 2. SEARCH <ID>        : Find Target info\n");
    printf(" 3. SHOW               : Visualize Tree Structure\n");
    printf(" 4. EXIT               : Shutdown System\n");
    printf("===========================================\n");
}

int main() {
    // Windows Memory Leak Check
#ifdef _WIN32
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    BTreeNode* root = NULL;
    char command[10];
    int id, threat;

    printf("\n[SYSTEM] T-MAP Engine Booted successfully.\n");
    printf("[SYSTEM] Waiting for operator commands...\n");
    print_help();

    // Infinite Loop (The Console Loop)
    while (1) {
        printf("\nT-MAP> "); // Command Prompt
        scanf("%s", command);

        if (strcmp(command, "ADD") == 0) {
            // Usage: ADD 1001 5
            if (scanf("%d %d", &id, &threat) != 2) {
                printf("[ERROR] Invalid Format. Usage: ADD <ID> <Threat>\n");
                while(getchar() != '\n'); // Clear buffer
                continue;
            }
            TacticalTrack* new_track = create_track(id, threat);
            // Add dummy initial position for realism
            add_history_node(new_track, 37.5, 127.0, 0); 
            insert_track(&root, new_track);
            
        } else if (strcmp(command, "SEARCH") == 0) {
            // Usage: SEARCH 1001
            if (scanf("%d", &id) != 1) {
                printf("[ERROR] Invalid Format. Usage: SEARCH <ID>\n");
                while(getchar() != '\n'); 
                continue;
            }
            TacticalTrack* result = search_btree(root, id);
            if (result != NULL) {
                printf("  => [FOUND] ID: %d | Threat: %d | History: %d nodes\n", 
                       result->track_id, result->threat_level, result->history_count);
            } else {
                printf("  => [NOT FOUND] Target ID %d does not exist.\n", id);
            }

        } else if (strcmp(command, "SHOW") == 0) {
            // Usage: SHOW
            printf("\n--- Current B-Tree Structure ---\n");
            if (root == NULL) printf("(Empty Tree)\n");
            else print_btree(root, 0);
            printf("--------------------------------\n");

        } else if (strcmp(command, "EXIT") == 0) {
            printf("[SYSTEM] Shutting down... Clearing memory...\n");
            free_btree(root);
            printf("[SYSTEM] Bye.\n");
            break; // Break the infinite loop

        } else if (strcmp(command, "HELP") == 0) {
            print_help();
        } else {
            printf("[ERROR] Unknown Command. Type 'HELP'.\n");
            // Clear input buffer to prevent infinite loop on bad input
            while(getchar() != '\n'); 
        }
    }

    return 0;
}