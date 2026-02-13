#include "common.h"

/* =================================================================
   1. Create New Track (Memory Allocation)
================================================================= */
TacticalTrack* create_track(int track_id, int threat_level) {
    // 1-1. Allocate memory for track
    TacticalTrack* new_track = (TacticalTrack*)malloc(sizeof(TacticalTrack));
    if (new_track == NULL) {
        printf("[ERROR] Memory allocation failed for Track ID: %d\n", track_id);
        return NULL;
    }
    
    // 1-2. Initialize
    new_track->track_id = track_id;
    new_track->threat_level = threat_level;
    new_track->history_head = NULL;
    new_track->history_count = 0;
    
    // 1-3. Log
    LOG_WAYPOINT("CREATE", track_id, "New track allocated.");
    
    return new_track;
}

/* =================================================================
   2. Add History Node (Linked List Insertion)
================================================================= */
void add_history_node(TacticalTrack* track, double lat, double lon, int timestamp) {
    if (track == NULL) return;

    // 2-1. Allocate memory for history node
    HistoryNode* new_node = (HistoryNode*)malloc(sizeof(HistoryNode));
    if (new_node == NULL) {
        printf("[ERROR] History allocation failed for Track ID: %d\n", track->track_id);
        return;
    }

    new_node->lat = lat;
    new_node->lon = lon;
    new_node->timestamp = timestamp;
    new_node->next = NULL;

    // 2-2. Append to Linked List
    if (track->history_head == NULL) {
        track->history_head = new_node;
    } else {
        HistoryNode* current = track->history_head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_node;
    }
    
    track->history_count++;
    
    // 2-3. Log
    char msg[100];
    sprintf(msg, "Pos Update (Lat: %.2f, Lon: %.2f)", lat, lon);
    LOG_WAYPOINT("UPDATE", track->track_id, msg);
}

/* =================================================================
   3. Free Track (Memory Cleanup - Prevent Leaks)
================================================================= */
void free_track(TacticalTrack* track) {
    if (track == NULL) return;

    HistoryNode* current = track->history_head;
    HistoryNode* next_node;

    // 3-1. Free Linked List (History)
    while (current != NULL) {
        next_node = current->next;
        free(current);
        current = next_node;
    }

    int t_id = track->track_id;
    
    // 3-2. Free Track Structure
    free(track);
    
    // 3-3. Log
    LOG_WAYPOINT("DESTROY", t_id, "Track & History memory released.");
}