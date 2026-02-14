/**
 * @file    btree.c
 * @brief   In-Memory B-Tree Indexing Engine
 * @details 표적 ID를 기준으로 O(log N) 검색, 삽입, 분할(Split) 및 가비지 컬렉션을 수행합니다.
 */

#include "common.h"

/* =================================================================
   PART 1: Node Creation & Search
================================================================= */

/**
 * @brief   새로운 B-Tree 노드를 메모리에 안전하게 할당합니다.
 * @param   is_leaf 단말(Leaf) 노드 여부 (true/false)
 * @return  초기화된 B-Tree 노드 포인터
 */
BTreeNode* create_btree_node(bool is_leaf) {
    BTreeNode* node = (BTreeNode*)malloc(sizeof(BTreeNode));
    if (node == NULL) {
        printf("[FATAL ERROR] B-Tree Node memory allocation failed.\n");
        exit(EXIT_FAILURE); // 엔진의 근간이 흔들리므로 즉시 시스템 셧다운
    }
    
    node->is_leaf = is_leaf;
    node->num_keys = 0;
    
    // [방어적 프로그래밍] 쓰레기값 참조를 막기 위한 NULL 초기화
    for (int i = 0; i < MAX_CHILDREN; i++) node->children[i] = NULL;
    for (int i = 0; i < MAX_KEYS; i++) node->tracks[i] = NULL;

    return node;
}

/**
 * @brief   B-Tree를 하향식(Top-Down)으로 탐색하여 표적을 찾습니다.
 * @param   node     현재 탐색 중인 트리의 루트/서브 노드
 * @param   track_id 찾고자 하는 표적 ID
 * @return  표적의 포인터 (O(log N)). 없으면 NULL 반환.
 */
TacticalTrack* search_btree(BTreeNode* node, int track_id) {
    if (node == NULL) return NULL;

    int i = 0;
    // 이진 탐색 대신 선형 탐색 사용 (B-Tree의 차수가 작으므로 CPU 캐시 히트율에 유리)
    while (i < node->num_keys && track_id > node->tracks[i]->track_id) i++;

    // 일치하는 ID 발견 시 반환 (Tombstone 여부는 호출자가 판단하도록 위임)
    if (i < node->num_keys && node->tracks[i]->track_id == track_id) {
        return node->tracks[i];
    }

    // Leaf 노드까지 내려왔는데도 없다면, 해당 데이터는 없는 것임
    if (node->is_leaf) return NULL;

    // 자식 노드로 재귀 탐색
    return search_btree(node->children[i], track_id);
}

/* =================================================================
   PART 2: Core Insertion Engine (Split Logic)
================================================================= */

void split_child(BTreeNode* parent, int i, BTreeNode* child_full) {
    BTreeNode* z = create_btree_node(child_full->is_leaf);
    z->num_keys = BTREE_T - 1;

    // 가득 찬 노드의 우측 절반을 새 노드(z)로 이동
    for (int j = 0; j < BTREE_T - 1; j++) {
        z->tracks[j] = child_full->tracks[j + BTREE_T];
    }

    if (!child_full->is_leaf) {
        for (int j = 0; j < BTREE_T; j++) {
            z->children[j] = child_full->children[j + BTREE_T];
        }
    }
    child_full->num_keys = BTREE_T - 1;

    // 부모 노드의 공간 확보 및 병합
    for (int j = parent->num_keys; j >= i + 1; j--) {
        parent->children[j + 1] = parent->children[j];
    }
    parent->children[i + 1] = z;

    for (int j = parent->num_keys - 1; j >= i; j--) {
        parent->tracks[j + 1] = parent->tracks[j];
    }

    parent->tracks[i] = child_full->tracks[BTREE_T - 1];
    parent->num_keys++;
}

void insert_non_full(BTreeNode* node, TacticalTrack* track) {
    int i = node->num_keys - 1;

    if (node->is_leaf) {
        // ID 기준 오름차순 정렬 삽입
        while (i >= 0 && track->track_id < node->tracks[i]->track_id) {
            node->tracks[i + 1] = node->tracks[i];
            i--;
        }
        node->tracks[i + 1] = track;
        node->num_keys++;
        LOG_WAYPOINT("INSERT", track->track_id, "Inserted into B-Tree Leaf.");
    } else {
        while (i >= 0 && track->track_id < node->tracks[i]->track_id) i--;
        i++;

        if (node->children[i]->num_keys == MAX_KEYS) {
            split_child(node, i, node->children[i]);
            if (track->track_id > node->tracks[i]->track_id) i++;
        }
        insert_non_full(node->children[i], track);
    }
}

void insert_track(BTreeNode** root, TacticalTrack* track) {
    BTreeNode* r = *root;

    if (r == NULL) {
        *root = create_btree_node(true);
        (*root)->tracks[0] = track;
        (*root)->num_keys = 1;
        LOG_WAYPOINT("INSERT", track->track_id, "Root created & Track inserted.");
        return;
    }

    if (r->num_keys == MAX_KEYS) {
        BTreeNode* s = create_btree_node(false);
        *root = s;
        s->children[0] = r;
        split_child(s, 0, r);
        insert_non_full(s, track);
        LOG_WAYPOINT("SPLIT", track->track_id, "Root split occurred (Tree Height +1).");
    } else {
        insert_non_full(r, track);
    }
}

/* =================================================================
   PART 3: Advanced Scanning & Memory Reclamation
================================================================= */

/**
 * @brief   In-order Traversal을 통한 고위협 표적 색출 (Tombstone 필터링 적용)
 * @param   node      시작 노드
 * @param   threshold 색출할 최소 위협도 임계치
 */
void scan_high_threat(BTreeNode* node, int threshold) {
    if (node == NULL) return;

    int i;
    for (i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf) scan_high_threat(node->children[i], threshold);
        
        // [핵심] 상태가 ACTIVE이고, 위협도가 threshold 이상인 놈만 철저히 걸러냄
        if (node->tracks[i]->status == TRACK_STATUS_ACTIVE && 
            node->tracks[i]->threat_level >= threshold) {
            
            printf("  [ALERT] ID: %-5d | Threat: %-2d | Updates: %d\n", 
                   node->tracks[i]->track_id, 
                   node->tracks[i]->threat_level, 
                   node->tracks[i]->history_count);
        }
    }
    if (!node->is_leaf) scan_high_threat(node->children[i], threshold);
}

void print_btree(BTreeNode* node, int level) {
    if (node == NULL) return;
    printf("  [Lv.%d] ", level);
    for (int i = 0; i < node->num_keys; i++) {
        // 파괴된 표적은 (D) 기호로 시각화
        if(node->tracks[i]->status == TRACK_STATUS_DESTROYED)
            printf("[%d(D)] ", node->tracks[i]->track_id);
        else
            printf("[%d] ", node->tracks[i]->track_id);
    }
    printf("\n");
    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++) print_btree(node->children[i], level + 1);
    }
}

/**
 * @brief   시스템 종료 시 발생하는 완벽한 가비지 컬렉션 (Recursive Free)
 * @note    모든 노드와 표적 본체, 잔류 궤적까지 0 bytes 누수를 목표로 해제함.
 */
void free_btree(BTreeNode* node) {
    if (node == NULL) return;

    // 1. 자식 노드들 먼저 재귀적으로 해제 (Post-order Traversal)
    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++) {
            free_btree(node->children[i]);
        }
    }

    // 2. 현재 노드가 품고 있는 '표적 본체' 소각
    for (int i = 0; i < node->num_keys; i++) {
        TacticalTrack* target = node->tracks[i];
        if (target != NULL) {
            // 아직 요격되지 않고 살아있던 표적이라면 궤적까지 여기서 해제
            HistoryNode* current = target->history_head;
            while (current != NULL) {
                HistoryNode* next = current->next;
                free(current);
                current = next;
            }
            // 최종적으로 표적 구조체 자체를 메모리에서 날려버림
            free(target);
        }
    }

    // 3. 껍데기가 된 B-Tree 노드 자신을 해제
    free(node);
}