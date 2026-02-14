/**
 * @file    common.h
 * @brief   T-MAP Tactical Engine Core Data Structures & Macros
 * @details Mission-Critical 실시간 전술 표적 추적을 위한 하이브리드 인메모리 구조체 정의.
 * B-Tree와 O(1) Tail-Pointer Linked List를 결합하여 설계됨.
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* =================================================================
   [1] System Configurations & Debugging
================================================================= */
#ifdef _WIN32
    #define _CRTDBG_MAP_ALLOC
    #include <crtdbg.h>
#endif

// B-Tree Tuning Parameters
#define BTREE_T 3                       // B-Tree의 최소 차수 (t)
#define MAX_KEYS (2 * BTREE_T - 1)      // 노드당 최대 키 개수 (5)
#define MAX_CHILDREN (2 * BTREE_T)      // 노드당 최대 자식 개수 (6)

/* =================================================================
   [2] Target Status Constants (매직 넘버 제거)
================================================================= */
#define TRACK_STATUS_ACTIVE     1       // 활성화된 정상 표적
#define TRACK_STATUS_DESTROYED -1       // 요격 완료된 표적 (Tombstone)

/* =================================================================
   [3] Core Data Structures
================================================================= */

/**
 * @brief Trajectory Node (단일 궤적 노드)
 * @note  표적의 특정 시간대 위치 정보를 담는 Linked List의 단위체
 */
typedef struct HistoryNode {
    double              lat;            // 위도 (Latitude)
    double              lon;            // 경도 (Longitude)
    int                 timestamp;      // 탐지 시간 (System Time)
    struct HistoryNode* next;           // 다음 궤적을 가리키는 포인터
} HistoryNode;

/**
 * @brief Tactical Target Data (전술 표적 본체)
 * @note  O(1) 삽입 성능을 위해 history_tail 포인터를 유지하는 것이 핵심 아키텍처
 */
typedef struct TacticalTrack {
    int                 track_id;       // 고유 표적 식별자 (Unique ID)
    int                 threat_level;   // 위협도 (1~10)
    int                 status;         // 표적 상태 (ACTIVE or DESTROYED)
    int                 history_count;  // 누적 궤적 데이터 개수
    
    HistoryNode* history_head;   // 궤적 리스트의 시작점 (순회 및 메모리 해제용)
    HistoryNode* history_tail;   // 궤적 리스트의 끝점 (O(1) 빠른 삽입용)
} TacticalTrack;

/**
 * @brief B-Tree Node (고속 인덱싱 노드)
 * @note  최대 MAX_KEYS 개의 표적 포인터를 품고 있는 트리의 마디
 */
typedef struct BTreeNode {
    int                 num_keys;                 // 현재 저장된 키의 개수
    bool                is_leaf;                  // 단말(Leaf) 노드 여부
    TacticalTrack* tracks[MAX_KEYS];         // 표적 데이터 포인터 배열
    struct BTreeNode* children[MAX_CHILDREN];   // 자식 노드 포인터 배열
} BTreeNode;

/* =================================================================
   [4] Logging Macros
================================================================= */
#define LOG_WAYPOINT(action, target_id, msg) \
    printf("[WAYPOINT] %-10s | Target ID: %-5d | %s\n", action, target_id, msg)

#endif // COMMON_H