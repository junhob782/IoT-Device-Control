/**
 * @file    track.c
 * @brief   Target Lifecycle & Trajectory Management Module
 * @details 표적의 생성, 궤적의 O(1) 고속 병합, 소프트 삭제(Tombstone) 로직을 담당.
 */

#include "common.h"

/**
 * @brief   메모리 풀에서 새로운 전술 표적 객체를 안전하게 할당하고 초기화합니다.
 * @param   track_id      표적의 고유 ID
 * @param   threat_level  초기 위협도 (1~10)
 * @return  할당된 표적의 포인터. 메모리 부족 시 NULL 반환.
 */
TacticalTrack* create_track(int track_id, int threat_level) {
    TacticalTrack* new_track = (TacticalTrack*)malloc(sizeof(TacticalTrack));
    if (new_track == NULL) {
        printf("[FATAL ERROR] Memory allocation failed for Target ID: %d. Out of RAM.\n", track_id);
        return NULL;
    }
    
    // [보안] 더미 데이터가 남지 않도록 모든 멤버를 명시적으로 초기화 (Zeroing)
    new_track->track_id      = track_id;
    new_track->threat_level  = threat_level;
    new_track->status        = TRACK_STATUS_ACTIVE;
    new_track->history_count = 0;
    
    // Head와 Tail 모두 NULL로 초기화
    new_track->history_head  = NULL;
    new_track->history_tail  = NULL; 
    
    LOG_WAYPOINT("CREATE", track_id, "Memory securely allocated & initialized.");
    return new_track;
}

/**
 * @brief   표적의 새로운 위치(궤적)를 기록합니다. [최적화: O(1) 시간 복잡도 달성]
 * @param   track      위치가 갱신될 대상 표적 포인터
 * @param   lat        새로운 위도
 * @param   lon        새로운 경도
 * @param   timestamp  데이터 수신 시간
 * @warning 파괴된(DESTROYED) 표적에는 궤적을 추가할 수 없습니다.
 */
void add_history_node(TacticalTrack* track, double lat, double lon, int timestamp) {
    // [방어적 프로그래밍] Null 체크 및 상태(Tombstone) 검증
    if (track == NULL) return;
    if (track->status == TRACK_STATUS_DESTROYED) {
        printf("[WARN] Attempted to update a destroyed target (ID: %d).\n", track->track_id);
        return;
    }

    // 1. 새로운 궤적 노드 메모리 할당
    HistoryNode* new_node = (HistoryNode*)malloc(sizeof(HistoryNode));
    if (new_node == NULL) {
        printf("[FATAL ERROR] History allocation failed for Target ID: %d.\n", track->track_id);
        return;
    }

    new_node->lat       = lat;
    new_node->lon       = lon;
    new_node->timestamp = timestamp;
    new_node->next      = NULL;

    // 2. O(1) 고속 삽입 로직 (꼬리 포인터 활용)
    if (track->history_head == NULL) {
        // 리스트가 비어있을 때: 첫 노드가 Head이자 Tail이 됨
        track->history_head = new_node;
        track->history_tail = new_node;
    } else {
        // 이미 노드가 있을 때: Tail 뒤에 바로 붙이고, Tail을 갱신 (while문 불필요!)
        track->history_tail->next = new_node;
        track->history_tail = new_node;
    }
    
    track->history_count++;
    
    char msg[100];
    sprintf(msg, "Pos Update [O(1)] (Lat: %.2f, Lon: %.2f)", lat, lon);
    LOG_WAYPOINT("UPDATE", track->track_id, msg);
}

/**
 * @brief   표적 요격 및 메모리 선택적 반환 (Tombstone Deletion 적용)
 * @param   track  격추할 대상 표적 포인터
 * @note    B-Tree의 균형 유지(Rebalancing) 오버헤드를 피하기 위해 Soft Delete를 수행하되,
 * 메모리 누수를 막기 위해 무거운 궤적 데이터(Linked List)만 완벽하게 free() 처리합니다.
 */
void intercept_track(TacticalTrack* track) {
    if (track == NULL || track->status == TRACK_STATUS_DESTROYED) return;

    // 1. 가장 큰 메모리를 차지하는 '궤적 리스트' 순회 및 완벽 소각
    HistoryNode* current = track->history_head;
    HistoryNode* next_node;

    while (current != NULL) {
        next_node = current->next;
        free(current); // 단일 노드 메모리 운영체제로 반환
        current = next_node;
    }

    // 2. 댕글링 포인터(Dangling Pointer) 방지를 위한 Null 처리
    track->history_head  = NULL;
    track->history_tail  = NULL; 
    track->history_count = 0;

    // 3. 상태값을 '파괴됨'으로 변경 (B-Tree 구조는 유지 - Tombstone)
    track->status = TRACK_STATUS_DESTROYED;

    LOG_WAYPOINT("KILL", track->track_id, "Target neutralized. Trajectory memory 100% reclaimed.");
}