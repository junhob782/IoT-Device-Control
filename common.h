#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Windows Memory Leak Detection (Plan B)
#ifdef _WIN32
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

// B-Tree Settings
#define BTREE_T 3
#define MAX_KEYS (2 * BTREE_T - 1)
#define MAX_CHILDREN (2 * BTREE_T)

/* =================================================================
   [1] History Node (Linked List) - "Trajectory Data"
================================================================= */
typedef struct HistoryNode {
    double lat;                 // Latitude
    double lon;                 // Longitude
    int timestamp;              // Time detected
    struct HistoryNode* next;   // Pointer to next history
} HistoryNode;

/* =================================================================
   [2] Tactical Track Data - "Target Information"
================================================================= */
typedef struct TacticalTrack {
    int track_id;               // Unique ID
    int threat_level;           // Threat (1-10)
    HistoryNode* history_head;  // Head of Linked List
    int history_count;          // Number of history nodes
} TacticalTrack;

/* =================================================================
   [3] B-Tree Node - "Indexing Container"
================================================================= */
typedef struct BTreeNode {
    int num_keys;
    TacticalTrack* tracks[MAX_KEYS];
    struct BTreeNode* children[MAX_CHILDREN];
    bool is_leaf;
} BTreeNode;

/* =================================================================
   [4] System Log Macro (REQ-07)
================================================================= */
// Unified Logging Format
#define LOG_WAYPOINT(action, target_id, msg) \
    printf("[WAYPOINT] %-10s | Target ID: %-5d | %s\n", action, target_id, msg)

#endif // COMMON_H