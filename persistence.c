#include<stdio.h>
#include "common.h"

extern TacticalTrack* create_track(int track_id, int threat_level);
extern void add_history_node(TacticalTrack* track, double lat, double lon, int timestamp);
extern void insert_track(BTreeNode** root, TacticalTrack* track);

void save_single_track(TacticalTrack* track, FILE* fp) {
    if (track == NULL) return;

    // 1. 표적의 핵심 정보(Header) 저장
    // 주의: 포인터(history_head/tail)는 저장하면 안 됨! (주소는 매번 바뀌니까)
    fwrite(&track->track_id, sizeof(int), 1, fp);
    fwrite(&track->threat_level, sizeof(int), 1, fp);
    fwrite(&track->status, sizeof(int), 1, fp);
    fwrite(&track->history_count, sizeof(int), 1, fp);

    // 2. 표적의 궤적(Linked List) 순회 및 저장
    HistoryNode* current = track->history_head;
    while (current != NULL) {
        // 좌표와 시간 데이터만 쏙 빼서 저장
        fwrite(&current->lat, sizeof(double), 1, fp);
        fwrite(&current->lon, sizeof(double), 1, fp);
        fwrite(&current->timestamp, sizeof(int), 1, fp);
        current = current->next;
    }
}

// B-Tree를 재귀적으로 돌면서 모든 표적을 찾아 저장하는 함수
void traverse_and_save(BTreeNode* node, FILE* fp) {
    if (node == NULL) return;

    // 1. 현재 노드의 표적들 저장
    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf) traverse_and_save(node->children[i], fp); // 왼쪽 자식 먼저
        save_single_track(node->tracks[i], fp);
    }
    // 2. 마지막 자식 저장
    if (!node->is_leaf) traverse_and_save(node->children[node->num_keys], fp);
}

// [메인 저장 함수] 외부에서 호출하는 함수
void SaveSystem(BTreeNode* root) {
    FILE* fp = fopen("tmap_data.dat", "wb"); // Binary Write 모드
    if (fp == NULL) {
        printf("[ERROR] Failed to open file for saving.\n");
        return;
    }

    printf("[SYSTEM] Saving data to 'tmap_data.dat'...\n");
    if (root != NULL) {
        traverse_and_save(root, fp);
    }
    
    fclose(fp);
    printf("[SYSTEM] Save Complete.\n");
}

/* =================================================================
   [2] LOAD SYSTEM (Deserialization)
   - 파일의 데이터를 읽어 다시 malloc으로 메모리에 재구축
================================================================= */

void LoadSystem(BTreeNode** root) {
    FILE* fp = fopen("tmap_data.dat", "rb"); // Binary Read 모드
    if (fp == NULL) {
        printf("[SYSTEM] No previous data found. Starting fresh.\n");
        return;
    }

    printf("[SYSTEM] Loading data from 'tmap_data.dat'...\n");

    int id, threat, status, count;
    int loaded_tracks = 0;

    // 파일 끝(EOF)에 도달할 때까지 계속 읽음
    while (fread(&id, sizeof(int), 1, fp) == 1) {
        // 1. 헤더 정보 읽기
        fread(&threat, sizeof(int), 1, fp);
        fread(&status, sizeof(int), 1, fp);
        fread(&count, sizeof(int), 1, fp);

        // 2. 표적 객체 메모리 할당 (복원)
        TacticalTrack* new_track = create_track(id, threat);
        new_track->status = status; // 저장된 상태(파괴됨/생존함) 복구

        // 3. 궤적 데이터(History) 복원
        for (int i = 0; i < count; i++) {
            double lat, lon;
            int time;
            fread(&lat, sizeof(double), 1, fp);
            fread(&lon, sizeof(double), 1, fp);
            fread(&time, sizeof(int), 1, fp);
            
            // O(1) 꼬리 포인터를 이용해 궤적 재연결
            add_history_node(new_track, lat, lon, time);
        }

        // 4. B-Tree에 삽입 (인덱싱 복구)
        insert_track(root, new_track);
        loaded_tracks++;
    }

    fclose(fp);
    printf("[SYSTEM] Load Complete. %d targets restored.\n", loaded_tracks);
}