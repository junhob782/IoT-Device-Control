/**
 * @file    track.c
 * @brief   Target Lifecycle & Trajectory Management Module
 * @details 표적의 생성, 궤적의 O(1) 고속 병합, 소프트 삭제 및 메모리 완전 해제 로직 담당.
 */

#include "common.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief   메모리에서 새로운 전술 표적 객체를 할당하고 초기화합니다.
 */
TacticalTrack* create_track(int track_id, int threat_level) {
    TacticalTrack* new_track = (TacticalTrack*)malloc(sizeof(TacticalTrack));
    if (new_track == NULL) {
        printf("[FATAL ERROR] Memory allocation failed for Target ID: %d.\n", track_id);
        return NULL;
    }
    
    new_track->track_id      = track_id;
    new_track->threat_level  = threat_level;
    new_track->status        = TRACK_STATUS_ACTIVE;
    new_track->history_count = 0;
    new_track->history_head  = NULL;
    new_track->history_tail  = NULL; 
    
    LOG_WAYPOINT("CREATE", track_id, "Memory securely allocated & initialized.");
    return new_track;
}

/**
 * @brief   표적의 새로운 위치(궤적)를 기록합니다. [O(1)]
 */
void add_history_node(TacticalTrack* track, double lat, double lon, int timestamp) {
    if (track == NULL || track->status == TRACK_STATUS_DESTROYED) return;

    HistoryNode* new_node = (HistoryNode*)malloc(sizeof(HistoryNode));
    if (new_node == NULL) return;

    new_node->lat       = lat;
    new_node->lon       = lon;
    new_node->timestamp = timestamp;
    new_node->next      = NULL;

    if (track->history_head == NULL) {
        track->history_head = new_node;
        track->history_tail = new_node;
    } else {
        track->history_tail->next = new_node;
        track->history_tail = new_node;
    }
    
    track->history_count++;
    
    // ======== 이 부분을 주석 처리합니다! ========
    // char msg[100];
    // sprintf(msg, "Pos Update [O(1)] (Lat: %.2f, Lon: %.2f)", lat, lon);
    // LOG_WAYPOINT("UPDATE", track->track_id, msg);
    // ============================================
}

/**
 * @brief   표적의 궤적(Linked List) 메모리만 선택적으로 해제합니다.
 */
void clear_track_history(TacticalTrack* track) {
    if (track == NULL) return;

    HistoryNode* current = track->history_head;
    while (current != NULL) {
        HistoryNode* next_node = current->next;
        free(current);
        current = next_node;
    }
    track->history_head = NULL;
    track->history_tail = NULL;
    track->history_count = 0;
}

/**
 * @brief   표적 요격 (Soft Delete: 궤적 메모리만 반환하고 상태 변경)
 */
void intercept_track(TacticalTrack* track) {
    if (track == NULL || track->status == TRACK_STATUS_DESTROYED) return;

    // 궤적 리스트만 소각
    clear_track_history(track);

    // 상태값을 '파괴됨'으로 변경
    track->status = TRACK_STATUS_DESTROYED;

    LOG_WAYPOINT("KILL", track->track_id, "Target neutralized. Trajectory memory reclaimed.");
}

/**
 * @brief   [추가됨] 표적 객체 자체를 메모리에서 완전히 삭제합니다.
 * @details B-Tree가 해제될 때 각 노드에 담긴 Track을 완전히 없애기 위해 사용됩니다.
 */
void free_track(TacticalTrack* track) {
    if (track == NULL) return;

    // 1. 남아있는 궤적 메모리 모두 해 de
    clear_track_history(track);

    // 2. 표적 구조체 본체 해제
    int id = track->track_id;
    free(track);

    // printf("[DEBUG] Track %d fully freed from memory.\n", id);
}