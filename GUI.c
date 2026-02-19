#include "include/raylib.h"
#include "common.h"
#include <stdlib.h>
#include <time.h>

// --- [쿼드트리(Quadtree) 모듈 연동] ---
typedef struct QuadNode QuadNode; // 불완전 타입 선언
extern QuadNode* create_quad_node(Rectangle boundary);
extern void BuildQuadtreeFromBTree(BTreeNode* btree_node, QuadNode* quad_root);
extern void DrawQuadtree(QuadNode* node);
extern void FreeQuadtree(QuadNode* node);

// --- [저장/로드(Persistence) 모듈 연동] ---
extern void SaveSystem(BTreeNode* root);
extern void LoadSystem(BTreeNode** root);

// --- [기존 T-MAP 엔진 핵심 함수들 (Extern)] ---
extern TacticalTrack* create_track(int track_id, int threat_level);
extern void add_history_node(TacticalTrack* track, double lat, double lon, int timestamp);
extern void intercept_track(TacticalTrack* track);
extern void insert_track(BTreeNode** root, TacticalTrack* track);
extern void free_btree(BTreeNode* node);

// 1. [렌더링 엔진] B-Tree를 순회하며 궤적(선)과 표적(점)을 그리는 함수
void DrawRadarTargets(BTreeNode* node) {
    if (node == NULL) return;

    for (int i = 0; i < node->num_keys; i++) {
        // 왼쪽 자식 먼저 순회 (재귀)
        if (!node->is_leaf) DrawRadarTargets(node->children[i]);

        TacticalTrack* target = node->tracks[i];
        
        // 데이터가 있고, 궤적(Linked List)이 존재한다면 그리기 시작
        if (target != NULL && target->history_head != NULL) {
            
            // ---------------------------------------------------------
            // [STEP 1] 과거의 궤적(History)을 선으로 연결하여 그리기
            // ---------------------------------------------------------
            HistoryNode* current = target->history_head;
            while (current != NULL && current->next != NULL) {
                // 현재 노드와 다음 노드를 선으로 잇기
                Vector2 startPos = { (float)current->lon, (float)current->lat };
                Vector2 endPos = { (float)current->next->lon, (float)current->next->lat };
                
                // 표적 상태에 따라 궤적 색상 다르게 (파괴됨: 회색, 활성: 초록/빨강)
                Color trailColor = (target->status == TRACK_STATUS_DESTROYED) ? GRAY : Fade(GREEN, 0.5f);
                if (target->threat_level >= 8 && target->status == TRACK_STATUS_ACTIVE) {
                    trailColor = Fade(RED, 0.5f); // 고위협은 빨간 궤적
                }

                DrawLineV(startPos, endPos, trailColor);
                current = current->next;
            }

            // ---------------------------------------------------------
            // [STEP 2] 현재 위치(Head)에 표적 아이콘 그리기
            // ---------------------------------------------------------
            // 가장 최신 위치 (Tail Pointer 사용)
            if (target->history_tail != NULL) {
                float x = (float)target->history_tail->lon;
                float y = (float)target->history_tail->lat;

                if (target->status == TRACK_STATUS_DESTROYED) {
                    // 격추된 표적 (Tombstone)
                    DrawText("X", (int)x - 5, (int)y - 10, 20, GRAY);
                } else if (target->threat_level >= 8) {
                    // 고위협 표적
                    DrawCircle((int)x, (int)y, 6, RED);
                    DrawCircleLines((int)x, (int)y, 10, Fade(RED, 0.6f)); // 위협 반경
                    DrawText(TextFormat("ID:%d", target->track_id), (int)x + 10, (int)y - 10, 10, RED);
                } else {
                    // 일반 표적
                    DrawCircle((int)x, (int)y, 4, LIME);
                    DrawText(TextFormat("ID:%d", target->track_id), (int)x + 8, (int)y - 8, 10, LIME);
                }
            }
        }
    }
    // 오른쪽 자식 순회
    if (!node->is_leaf) DrawRadarTargets(node->children[node->num_keys]);
}

// 2. [물리 엔진] 모든 표적을 매 프레임마다 무작위로 움직이는 함수
void UpdateTargetsPosition(BTreeNode* node, int current_time) {
    if (node == NULL) return;
    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf) UpdateTargetsPosition(node->children[i], current_time);

        TacticalTrack* target = node->tracks[i];
        // 파괴되지 않은 표적만 O(1) 꼬리 포인터 알고리즘으로 궤적 추가
        if (target != NULL && target->status == TRACK_STATUS_ACTIVE && target->history_tail != NULL) {
            double new_lon = target->history_tail->lon + (GetRandomValue(-10, 10) * 0.15);
            double new_lat = target->history_tail->lat + (GetRandomValue(-10, 10) * 0.15);

            // 레이더 화면 밖으로 나가지 않게 가두기
            if(new_lon < 50) new_lon = 50; if(new_lon > 750) new_lon = 750;
            if(new_lat < 50) new_lat = 50; if(new_lat > 550) new_lat = 550;

            add_history_node(target, new_lat, new_lon, current_time);
        }
    }
    if (!node->is_leaf) UpdateTargetsPosition(node->children[node->num_keys], current_time);
}

// 3. [전술 엔진] 위협도 8 이상의 최우선 표적 1개를 찾아 요격(Tombstone)
bool InterceptFirstHighThreat(BTreeNode* node) {
    if (node == NULL) return false;
    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf) {
            if(InterceptFirstHighThreat(node->children[i])) return true;
        }

        TacticalTrack* target = node->tracks[i];
        if (target != NULL && target->status == TRACK_STATUS_ACTIVE && target->threat_level >= 8) {
            intercept_track(target); // 요격 수행
            return true; 
        }
    }
    if (!node->is_leaf) return InterceptFirstHighThreat(node->children[node->num_keys]);
    return false;
}

// 4. [메인 GUI 루프] Launcher에서 호출되는 진입점
int RunRadarGUI(void) {
    InitWindow(800, 600, "T-MAP Tactical Radar - Live Visualization");
    SetTargetFPS(60);
    srand(time(NULL));

    BTreeNode* root = NULL;

    // [시스템 부팅] 파일에서 데이터 복원 (Persistence)
    LoadSystem(&root);

    int current_time = 0;
    int next_id = 1000;
    
    // 로드된 데이터가 있다면 ID 충돌 방지를 위해 시작 번호 증가
    if (root != NULL) next_id += 100;

    while (!WindowShouldClose()) {
        // --- [ 입력 처리 ] ---
        // 'A': 표적 생성
        if (IsKeyPressed(KEY_A)) {
            int threat = GetRandomValue(1, 10);
            TacticalTrack* new_track = create_track(next_id++, threat);
            double spawn_x = GetRandomValue(100, 700);
            double spawn_y = GetRandomValue(100, 500);
            add_history_node(new_track, spawn_y, spawn_x, current_time);
            insert_track(&root, new_track);
        }

        // 'K': 고위협 표적 요격
        if (IsKeyPressed(KEY_K)) {
            InterceptFirstHighThreat(root);
        }

        // --- [ 물리 업데이트 ] ---
        current_time++;
        if (current_time % 6 == 0) { // 속도 조절
            UpdateTargetsPosition(root, current_time);
        }

        // --- [ 쿼드트리 업데이트 ] ---
        // 매 프레임마다 동적 객체들을 담을 쿼드트리를 생성
        QuadNode* q_root = create_quad_node((Rectangle){0, 0, 800, 600});
        BuildQuadtreeFromBTree(root, q_root);

        // --- [ 렌더링 ] ---
        BeginDrawing();
        ClearBackground(BLACK);

        // 1. 레이더 UI 및 배경
        DrawCircleLines(400, 300, 100, DARKGREEN);
        DrawCircleLines(400, 300, 200, DARKGREEN);
        DrawCircleLines(400, 300, 300, DARKGREEN);
        DrawLine(400, 0, 400, 600, DARKGREEN);
        DrawLine(0, 300, 800, 300, DARKGREEN);
        DrawText("AQUILA HQ", 405, 305, 10, GREEN);

        DrawText("[ T-MAP COMMAND CENTER ]", 10, 10, 20, GREEN);
        DrawText("- Press 'A' : ADD Target", 10, 40, 15, RAYWHITE);
        DrawText("- Press 'K' : KILL High-Threat", 10, 60, 15, RED);

        // 2. 쿼드트리 격자 시각화 (공간 분할 확인용)
        DrawQuadtree(q_root);

        // 3. 표적 및 궤적 그리기
        DrawRadarTargets(root);

        EndDrawing();

        // 사용한 쿼드트리는 즉시 해제 (매 프레임 리셋)
        FreeQuadtree(q_root);
    }

    // [시스템 종료] 데이터 저장 (Persistence)
    SaveSystem(root);

    free_btree(root);
    CloseWindow();
    return 0;
}