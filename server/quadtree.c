/**
 * @file    quadtree.c
 * @brief   Spatial Partitioning Engine
 * @details 화면을 4분면으로 재귀적으로 분할하여 충돌 감지 및 범위 검색 속도를 최적화합니다.
 */

#include "common.h"  // <--- TargetRect, Vector2, CheckCollisionPointRect 정의가 들어있음
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// [삭제됨] 여기서 TargetRect를 다시 정의하면 common.h와 충돌하여 에러가 발생합니다.

#define MAX_CAPACITY 4 // 한 구역(상자)에 들어갈 수 있는 최대 드론 수

// 쿼드트리 노드 구조체 (여기서만 씀)
typedef struct QuadNode {
    TargetRect boundary;                 // 현재 구역의 위치와 크기 (x, y, width, height)
    TacticalTrack* points[MAX_CAPACITY]; // 이 구역에 있는 드론들 포인터
    int count;                           // 현재 저장된 드론 수
    
    // 자식 노드 4개 (북서, 북동, 남서, 남동)
    struct QuadNode *nw, *ne, *sw, *se;
    bool divided;                        // 쪼개졌는지 여부
} QuadNode;

// 1. 쿼드트리 노드 생성
QuadNode* create_quad_node(TargetRect boundary) {
    QuadNode* node = (QuadNode*)malloc(sizeof(QuadNode));
    node->boundary = boundary;
    node->count = 0;
    node->divided = false;
    node->nw = node->ne = node->sw = node->se = NULL;
    for(int i=0; i<MAX_CAPACITY; i++) node->points[i] = NULL;
    return node;
}

// 2. 구역 4등분 (Subdivide)
void subdivide(QuadNode* node) {
    float x = node->boundary.x;
    float y = node->boundary.y;
    float w = node->boundary.width / 2;
    float h = node->boundary.height / 2;

    node->nw = create_quad_node((TargetRect){x, y, w, h});         // 왼쪽 위
    node->ne = create_quad_node((TargetRect){x + w, y, w, h});     // 오른쪽 위
    node->sw = create_quad_node((TargetRect){x, y + h, w, h});     // 왼쪽 아래
    node->se = create_quad_node((TargetRect){x + w, y + h, w, h}); // 오른쪽 아래
    
    node->divided = true;
}

// 3. 쿼드트리에 드론 위치 삽입 (Insert)
bool insert_quad(QuadNode* node, TacticalTrack* track) {
    if (track == NULL || track->history_tail == NULL) return false;

    // 현재 드론의 위치
    Vector2 point = { (float)track->history_tail->lon, (float)track->history_tail->lat };

    // 1. 내 구역 범위 밖이면 무시 (common.h에 정의된 함수 사용)
    if (!CheckCollisionPointRect(point, node->boundary)) return false;

    // 2. 자리가 남고, 아직 안 쪼개졌으면 -> 그냥 넣음
    if (node->count < MAX_CAPACITY && !node->divided) {
        node->points[node->count++] = track;
        return true;
    }

    // 3. 꽉 찼으면 -> 쪼갠다(Subdivide)
    if (!node->divided) {
        subdivide(node);
        // 기존에 있던 애들도 자식들한테 이사 보냄 (Re-distribute)
        for (int i = 0; i < node->count; i++) {
            insert_quad(node->nw, node->points[i]);
            insert_quad(node->ne, node->points[i]);
            insert_quad(node->sw, node->points[i]);
            insert_quad(node->se, node->points[i]);
        }
        node->count = 0; // 이사는 끝났으니 카운트 초기화
    }

    // 4. 자식들 중 맞는 구역에 넣음 (재귀)
    return insert_quad(node->nw, track) ||
           insert_quad(node->ne, track) ||
           insert_quad(node->sw, track) ||
           insert_quad(node->se, track);
}

// 4. 시각화 (서버용이므로 주석 처리 유지)
/*
void DrawQuadtree(QuadNode* node) {
    // ... 생략 ...
}
*/

// 5. 메모리 해제
void FreeQuadtree(QuadNode* node) {
    if (node == NULL) return;
    if (node->divided) {
        FreeQuadtree(node->nw);
        FreeQuadtree(node->ne);
        FreeQuadtree(node->sw);
        FreeQuadtree(node->se);
    }
    free(node);
}

// 6. [헬퍼] B-Tree에 있는 모든 드론을 쿼드트리에 넣기
void BuildQuadtreeFromBTree(BTreeNode* btree_node, QuadNode* quad_root) {
    if (btree_node == NULL) return;

    for (int i = 0; i < btree_node->num_keys; i++) {
        if (!btree_node->is_leaf) BuildQuadtreeFromBTree(btree_node->children[i], quad_root);
        
        // 살아있는 드론만 쿼드트리에 등록
        if (btree_node->tracks[i]->status == 1) { // 1: ACTIVE (보통 common.h에 정의됨)
            insert_quad(quad_root, btree_node->tracks[i]);
        }
    }
    if (!btree_node->is_leaf) BuildQuadtreeFromBTree(btree_node->children[btree_node->num_keys], quad_root);
}