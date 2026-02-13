#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// 윈도우 환경 메모리 누수 탐지(CRT)를 위한 특수 매크로 (Plan B 적용)
#ifdef _WIN32
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

// B-Tree 설정 (t = 최소 차수)
#define BTREE_T 3
#define MAX_KEYS (2 * BTREE_T - 1) // 한 노드가 가질 수 있는 최대 표적 수 (5개)
#define MAX_CHILDREN (2 * BTREE_T) // 한 노드가 가질 수 있는 최대 자식 수 (6개)

/* =================================================================
   [1] 과거 궤적 노드 (Linked List) - "드론이 지나간 발자국"
================================================================= */
typedef struct HistoryNode {
    double lat;                 // 위도 (Latitude)
    double lon;                 // 경도 (Longitude)
    int timestamp;              // 탐지된 시간
    struct HistoryNode* next;   // 다음 발자국을 가리키는 밧줄(포인터)
} HistoryNode;

/* =================================================================
   [2] 전술 표적 데이터 (Target Track) - "드론 1대의 종합 정보"
================================================================= */
typedef struct TacticalTrack {
    int track_id;               // 표적 고유 ID (B-Tree에서 찾을 때 쓸 열쇠)
    int threat_level;           // 위협도 (1~10)
    HistoryNode* history_head;  // 이 드론의 발자국들이 묶여있는 첫 번째 매듭
    int history_count;          // 지금까지 몇 번 위치가 갱신되었는지 카운트
} TacticalTrack;

/* =================================================================
   [3] B-Tree 노드 - "표적들을 빠르게 찾기 위한 서류함의 칸"
================================================================= */
typedef struct BTreeNode {
    int num_keys;                          // 이 서류함 칸에 들어있는 표적의 개수
    TacticalTrack* tracks[MAX_KEYS];       // 실제 표적 데이터가 있는 곳의 주소(포인터) 배열
    struct BTreeNode* children[MAX_CHILDREN]; // 아래쪽 서류함(자식)들을 가리키는 주소 배열
    bool is_leaf;                          // 여기가 맨 밑바닥(리프) 칸인지 확인
} BTreeNode;

#endif // COMMON_H