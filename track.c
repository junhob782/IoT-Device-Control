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

/* =================================================================
   4. 표적 요격 및 메모리 반환 (Tombstone / Soft Delete 기법)
================================================================= */
void intercept_track(TacticalTrack* track) {
    if (track == NULL || track->threat_level == -1) return;

    // 1. 가장 메모리를 많이 먹는 '과거 궤적(Linked List)'을 하나씩 완벽하게 소각(free)
    HistoryNode* current = track->history_head;
    HistoryNode* next_node;

    while (current != NULL) {
        next_node = current->next;
        free(current); // 발자국 하나하나 메모리 반환
        current = next_node;
    }

    // 2. 표적 상태를 '파괴됨(Destroyed)'으로 변경 (Tombstone 처리)
    track->history_head = NULL;
    track->history_count = 0;
    track->threat_level = -1; // -1은 요격 완료(죽음)를 의미

    // 3. 요격 성공 로그 출력
    LOG_WAYPOINT("KILL", track->track_id, "Target Destroyed. Trajectory memory 100% reclaimed.");
}