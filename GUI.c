#include "include/raylib.h"
#include "common.h"
#include <stdlib.h>
#include <time.h>

// --- 기존 T-MAP 엔진의 핵심 함수들 연결 (Extern) ---
extern TacticalTrack* create_track(int track_id, int threat_level);
extern void add_history_node(TacticalTrack* track, double lat, double lon, int timestamp);
extern void intercept_track(TacticalTrack* track);
extern void insert_track(BTreeNode** root, TacticalTrack* track);
extern void free_btree(BTreeNode* node);

// 1. [렌더링 엔진] B-Tree를 순회하며 화면에 표적을 그리는 함수
void DrawRadarTargets(BTreeNode* node) {
    if (node == NULL) return;

    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf) DrawRadarTargets(node->children[i]);

        TacticalTrack* target = node->tracks[i];
        if (target != NULL && target->history_tail != NULL) {
            // 화면 픽셀 좌표계로 사용하기 위해 X=lon, Y=lat 로 치환
            float x = (float)target->history_tail->lon;
            float y = (float)target->history_tail->lat;

            if (target->status == TRACK_STATUS_DESTROYED) {
                // 요격된 표적 (Tombstone): 회색 X 기호
                DrawText("X", (int)x, (int)y, 20, GRAY);
            } else if (target->threat_level >= 8) {
                // 고위협 표적: 빨간색 원형 레이더 점
                DrawCircle((int)x, (int)y, 8, RED);
                DrawText(TextFormat("ID:%d", target->track_id), (int)x + 12, (int)y - 10, 15, RED);
            } else {
                // 일반 표적: 초록색 레이더 점
                DrawCircle((int)x, (int)y, 5, LIME);
                DrawText(TextFormat("ID:%d", target->track_id), (int)x + 10, (int)y - 10, 10, LIME);
            }
        }
    }
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
            intercept_track(target); // Day 1~10에 만든 그 함수 호출!
            return true; 
        }
    }
    if (!node->is_leaf) return InterceptFirstHighThreat(node->children[node->num_keys]);
    return false;
}

int main(void) {
    InitWindow(800, 600, "T-MAP Tactical Radar - Live Visualization");
    SetTargetFPS(60);
    srand(time(NULL));

    BTreeNode* root = NULL;
    int current_time = 0;
    int next_id = 1000;

    while (!WindowShouldClose()) {
        // --- [ 컨트롤 (입력) 로직 ] ---
        // 'A' 키를 누르면 레이더망 모서리에 무작위 표적 스폰
        if (IsKeyPressed(KEY_A)) {
            int threat = GetRandomValue(1, 10);
            TacticalTrack* new_track = create_track(next_id++, threat);
            double spawn_x = GetRandomValue(100, 700);
            double spawn_y = GetRandomValue(100, 500);
            add_history_node(new_track, spawn_y, spawn_x, current_time);
            insert_track(&root, new_track);
        }

        // 'K' 키를 누르면 고위협 표적 즉시 요격
        if (IsKeyPressed(KEY_K)) {
            InterceptFirstHighThreat(root);
        }

        // 약간의 딜레이를 주어 표적 이동 (1초에 약 10번 업데이트)
        current_time++;
        if (current_time % 6 == 0) {
            UpdateTargetsPosition(root, current_time);
        }

        // --- [ 렌더링 (그리기) 로직 ] ---
        BeginDrawing();
        ClearBackground(BLACK);

        // 1. 레이더 배경선 그리기 (동심원과 십자선)
        DrawCircleLines(400, 300, 100, DARKGREEN);
        DrawCircleLines(400, 300, 200, DARKGREEN);
        DrawCircleLines(400, 300, 300, DARKGREEN);
        DrawLine(400, 0, 400, 600, DARKGREEN);
        DrawLine(0, 300, 800, 300, DARKGREEN);
        DrawText("AQUILA HQ", 405, 305, 10, GREEN); // 중앙 기지

        // 2. 컨트롤 가이드 UI
        DrawText("[ T-MAP COMMAND CENTER ]", 10, 10, 20, GREEN);
        DrawText("- Press 'A' : ADD Target (Random)", 10, 40, 15, RAYWHITE);
        DrawText("- Press 'K' : KILL High-Threat (Threat >= 8)", 10, 60, 15, RED);

        // 3. 엔진 순회 및 점 찍기
        DrawRadarTargets(root);

        EndDrawing();
    }

    free_btree(root); // 안전한 종료
    CloseWindow();
    return 0;
}