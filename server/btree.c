#include "common.h"
#include "../common/packet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>       // SOCKET, sockaddr_in 정의를 위해 필요
#pragma comment(lib, "ws2_32.lib")

#ifndef MIN_DEGREE
#define MIN_DEGREE 3        // B-Tree의 최소 차수 (에러 해결)
#endif
/* --- 외부 함수 연결 --- */
extern void free_track(TacticalTrack* track);

/**
 * @brief 새로운 B-Tree 노드 생성
 */
BTreeNode* create_btree_node(bool is_leaf) {
    BTreeNode* node = (BTreeNode*)malloc(sizeof(BTreeNode));
    node->num_keys = 0;
    node->is_leaf = is_leaf;
    for (int i = 0; i < MAX_KEYS; i++) node->tracks[i] = NULL;
    for (int i = 0; i <= MAX_KEYS; i++) node->children[i] = NULL;
    return node;
}

/**
 * @brief 노드 분할 (Split Child)
 */
void split_child(BTreeNode* parent, int i, BTreeNode* full_node) {
    BTreeNode* new_node = create_btree_node(full_node->is_leaf);
    new_node->num_keys = MIN_DEGREE - 1;

    // 데이터를 새 노드로 복사
    for (int j = 0; j < MIN_DEGREE - 1; j++) {
        new_node->tracks[j] = full_node->tracks[j + MIN_DEGREE];
        full_node->tracks[j + MIN_DEGREE] = NULL;
    }

    // 자식 노드가 있다면 자식들도 복사
    if (!full_node->is_leaf) {
        for (int j = 0; j < MIN_DEGREE; j++) {
            new_node->children[j] = full_node->children[j + MIN_DEGREE];
            full_node->children[j + MIN_DEGREE] = NULL;
        }
    }

    full_node->num_keys = MIN_DEGREE - 1;

    // 부모 노드의 자식 포인터 밀어내고 새 노드 연결
    for (int j = parent->num_keys; j >= i + 1; j--) {
        parent->children[j + 1] = parent->children[j];
    }
    parent->children[i + 1] = new_node;

    // 부모 노드의 키(Track) 밀어내고 분할점 데이터 올리기
    for (int j = parent->num_keys - 1; j >= i; j--) {
        parent->tracks[j + 1] = parent->tracks[j];
    }
    parent->tracks[i] = full_node->tracks[MIN_DEGREE - 1];
    full_node->tracks[MIN_DEGREE - 1] = NULL;
    parent->num_keys++;
}

/**
 * @brief 꽉 차지 않은 노드에 삽입
 */
void insert_non_full(BTreeNode* node, TacticalTrack* track) {
    int i = node->num_keys - 1;

    if (node->is_leaf) {
        while (i >= 0 && node->tracks[i]->track_id > track->track_id) {
            node->tracks[i + 1] = node->tracks[i];
            i--;
        }
        node->tracks[i + 1] = track;
        node->num_keys++;
    } else {
        while (i >= 0 && node->tracks[i]->track_id > track->track_id) i--;
        if (node->children[i + 1]->num_keys == MAX_KEYS) {
            split_child(node, i + 1, node->children[i + 1]);
            if (node->tracks[i + 1]->track_id < track->track_id) i++;
        }
        insert_non_full(node->children[i + 1], track);
    }
}

/**
 * @brief 메인 삽입 함수
 */
void insert_track(BTreeNode** root, TacticalTrack* track) {
    if (*root == NULL) {
        *root = create_btree_node(true);
        (*root)->tracks[0] = track;
        (*root)->num_keys = 1;
        printf("[WAYPOINT] INSERT     | Target ID: %-4d | Root created & Track inserted.\n", track->track_id);
        return;
    }

    if ((*root)->num_keys == MAX_KEYS) {
        BTreeNode* new_root = create_btree_node(false);
        new_root->children[0] = *root;
        split_child(new_root, 0, *root);
        int i = (new_root->tracks[0]->track_id < track->track_id) ? 1 : 0;
        insert_non_full(new_root->children[i], track);
        *root = new_root;
        printf("[WAYPOINT] SPLIT      | B-Tree Height Increased.\n");
    } else {
        insert_non_full(*root, track);
    }
    printf("[WAYPOINT] INSERT     | Target ID: %-4d | Inserted into B-Tree Leaf.\n", track->track_id);
}

/**
 * @brief 트랙 ID로 검색
 */
TacticalTrack* search_btree(BTreeNode* node, int track_id) {
    if (node == NULL) return NULL;
    int i = 0;
    while (i < node->num_keys && track_id > node->tracks[i]->track_id) i++;
    if (i < node->num_keys && track_id == node->tracks[i]->track_id) return node->tracks[i];
    if (node->is_leaf) return NULL;
    return search_btree(node->children[i], track_id);
}

/**
 * @brief B-Tree 구조 출력 (시각화)
 */
void print_btree(BTreeNode* node, int level) {
    if (node == NULL) return;
    printf("Level %d: ", level);
    for (int i = 0; i < node->num_keys; i++) printf("[%d] ", node->tracks[i]->track_id);
    printf("\n");
    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++) print_btree(node->children[i], level + 1);
    }
}

/**
 * @brief 고위험 표적 스캔
 */
void scan_high_threat(BTreeNode* node, int threshold) {
    if (node == NULL) return;
    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf) scan_high_threat(node->children[i], threshold);
        if (node->tracks[i]->threat_level >= threshold && node->tracks[i]->status != 0) {
            printf("  [!] ALERT: Target %d (Threat: %d) detected!\n", 
                   node->tracks[i]->track_id, node->tracks[i]->threat_level);
        }
    }
    if (!node->is_leaf) scan_high_threat(node->children[node->num_keys], threshold);
}

/**
 * @brief 메모리 해제
 */
void free_btree(BTreeNode* node) {
    if (node == NULL) return;
    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf) free_btree(node->children[i]);
        free_track(node->tracks[i]);
    }
    if (!node->is_leaf) free_btree(node->children[node->num_keys]);
    free(node);
}

/* ========================================================
    [핵심 추가 함수] B-Tree 데이터를 UDP로 브로드캐스트
   ======================================================== */
void broadcast_btree(BTreeNode* node, SOCKET sock, struct sockaddr_in* addr) {
    if (node == NULL) return;

    for (int i = 0; i < node->num_keys; i++) {
        // 1. 왼쪽 자식 노드 탐색
        if (!node->is_leaf) {
            broadcast_btree(node->children[i], sock, addr);
        }

        // 2. 현재 노드의 표적 전송
        TacticalTrack* track = node->tracks[i];
        if (track != NULL && track->status == 1) { // 1: ACTIVE
            TargetPacket pkt;
            memset(&pkt, 0, sizeof(TargetPacket));

            pkt.id = track->track_id;
            pkt.threat_level = track->threat_level;
            pkt.status = (int)track->status;

            if (track->history_tail != NULL) {
                pkt.lat = (float)track->history_tail->lat;
                pkt.lon = (float)track->history_tail->lon;
            }

            sendto(sock, (const char*)&pkt, sizeof(TargetPacket), 0,
                   (struct sockaddr*)addr, sizeof(*addr));
        }
    }

    // 3. 마지막 오른쪽 자식 노드 탐색
    if (!node->is_leaf) {
        broadcast_btree(node->children[node->num_keys], sock, addr);
    }
}