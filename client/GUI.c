#define SCREEN_W 1920
#define SCREEN_H 1080
#define CENTER_X (SCREEN_W / 2)
#define CENTER_Y (SCREEN_H / 2)


#ifndef NOGDI
    #define NOGDI
#endif
#ifndef NOUSER
    #define NOUSER
#endif

#include "raylib.h"
#include "common.h"
#include <stdlib.h>
#include <time.h>

// --- [해상도 설정: 16인치 모니터용 FHD 모드] ---
// 화면의 절반 정도를 꽉 채우는 1920 x 1080 사이즈입니다.


// --- [모듈 연동 선언] ---
typedef struct QuadNode QuadNode;
extern QuadNode* create_quad_node(Rectangle boundary);
extern void BuildQuadtreeFromBTree(BTreeNode* btree_node, QuadNode* quad_root);
extern void DrawQuadtree(QuadNode* node);
extern void FreeQuadtree(QuadNode* node);

extern void SaveSystem(BTreeNode* root);
extern void LoadSystem(BTreeNode** root);

extern TacticalTrack* create_track(int track_id, int threat_level);
extern void add_history_node(TacticalTrack* track, double lat, double lon, int timestamp);
extern void intercept_track(TacticalTrack* track);
extern void insert_track(BTreeNode** root, TacticalTrack* track);
extern void free_btree(BTreeNode* node);

// 1. [렌더링] 궤적 그리기
void DrawRadarTargets(BTreeNode* node) {
    if (node == NULL) return;

    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf) DrawRadarTargets(node->children[i]);

        TacticalTrack* target = node->tracks[i];
        if (target != NULL && target->history_head != NULL) {
            // 궤적(선)
            HistoryNode* current = target->history_head;
            while (current != NULL && current->next != NULL) {
                Vector2 startPos = { (float)current->lon, (float)current->lat };
                Vector2 endPos = { (float)current->next->lon, (float)current->next->lat };
                Color trailColor = (target->status == TRACK_STATUS_DESTROYED) ? GRAY : Fade(GREEN, 0.5f);
                if (target->threat_level >= 8 && target->status == TRACK_STATUS_ACTIVE) trailColor = Fade(RED, 0.5f);
                DrawLineV(startPos, endPos, trailColor);
                current = current->next;
            }
            // 표적(아이콘)
            if (target->history_tail != NULL) {
                float x = (float)target->history_tail->lon;
                float y = (float)target->history_tail->lat;
                if (target->status == TRACK_STATUS_DESTROYED) {
                    DrawText("X", (int)x - 5, (int)y - 10, 20, GRAY);
                } else if (target->threat_level >= 8) {
                    DrawCircle((int)x, (int)y, 6, RED);
                    DrawCircleLines((int)x, (int)y, 10, Fade(RED, 0.6f));
                    DrawText(TextFormat("ID:%d", target->track_id), (int)x + 10, (int)y - 10, 10, RED);
                } else {
                    DrawCircle((int)x, (int)y, 4, LIME);
                    DrawText(TextFormat("ID:%d", target->track_id), (int)x + 8, (int)y - 8, 10, LIME);
                }
            }
        }
    }
    if (!node->is_leaf) DrawRadarTargets(node->children[node->num_keys]);
}

// 2. [물리] 이동 범위 제한 (화면 크기에 맞춰 자동 조정)
void UpdateTargetsPosition(BTreeNode* node, int current_time) {
    if (node == NULL) return;
    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf) UpdateTargetsPosition(node->children[i], current_time);

        TacticalTrack* target = node->tracks[i];
        if (target != NULL && target->status == TRACK_STATUS_ACTIVE && target->history_tail != NULL) {
            double new_lon = target->history_tail->lon + (GetRandomValue(-10, 10) * 0.15);
            double new_lat = target->history_tail->lat + (GetRandomValue(-10, 10) * 0.15);

            // 넓어진 해상도에 맞춰 가두기
            if(new_lon < 50) new_lon = 50; 
            if(new_lon > SCREEN_W - 50) new_lon = SCREEN_W - 50;
            
            if(new_lat < 50) new_lat = 50; 
            if(new_lat > SCREEN_H - 50) new_lat = SCREEN_H - 50;

            add_history_node(target, new_lat, new_lon, current_time);
        }
    }
    if (!node->is_leaf) UpdateTargetsPosition(node->children[node->num_keys], current_time);
}

// 3. [전술] 요격
bool InterceptFirstHighThreat(BTreeNode* node) {
    if (node == NULL) return false;
    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf) {
            if(InterceptFirstHighThreat(node->children[i])) return true;
        }
        TacticalTrack* target = node->tracks[i];
        if (target != NULL && target->status == TRACK_STATUS_ACTIVE && target->threat_level >= 8) {
            intercept_track(target);
            return true; 
        }
    }
    if (!node->is_leaf) return InterceptFirstHighThreat(node->children[node->num_keys]);
    return false;
}

// 4. [메인] GUI 실행
int RunRadarGUI(void) {
    InitWindow(SCREEN_W, SCREEN_H, "T-MAP Tactical Radar - Full HD Visualizer");
    SetTargetFPS(60);
    srand(time(NULL));

    BTreeNode* root = NULL;
    LoadSystem(&root);

    int current_time = 0;
    int next_id = 1000;
    if (root != NULL) next_id += 100;

    while (!WindowShouldClose()) {
        // [입력] 'A' 키: 표적 생성 (넓은 화면 전체 활용)
        if (IsKeyPressed(KEY_A)) {
            int threat = GetRandomValue(1, 10);
            TacticalTrack* new_track = create_track(next_id++, threat);
            
            // 생성 위치도 1920x1080 범위 내 랜덤
            double spawn_x = GetRandomValue(100, SCREEN_W - 100);
            double spawn_y = GetRandomValue(100, SCREEN_H - 100);
            add_history_node(new_track, spawn_y, spawn_x, current_time);
            insert_track(&root, new_track);
        }

        if (IsKeyPressed(KEY_K)) InterceptFirstHighThreat(root);

        // [물리] 업데이트
        current_time++;
        if (current_time % 6 == 0) UpdateTargetsPosition(root, current_time);

        // [쿼드트리] 업데이트
        QuadNode* q_root = create_quad_node((Rectangle){0, 0, SCREEN_W, SCREEN_H});
        BuildQuadtreeFromBTree(root, q_root);

        // [렌더링]
        BeginDrawing();
        ClearBackground(BLACK);

        // 레이더 동심원 (화면 중앙 기준, 더 크게 그림)
        DrawCircleLines(CENTER_X, CENTER_Y, 200, DARKGREEN);
        DrawCircleLines(CENTER_X, CENTER_Y, 400, DARKGREEN);
        DrawCircleLines(CENTER_X, CENTER_Y, 500, DARKGREEN); // 1080p에 맞게 더 큰 원 추가
        DrawLine(CENTER_X, 0, CENTER_X, SCREEN_H, DARKGREEN);
        DrawLine(0, CENTER_Y, SCREEN_W, CENTER_Y, DARKGREEN);
        DrawText("AQUILA HQ", CENTER_X + 5, CENTER_Y + 5, 10, GREEN);

        DrawText("[ T-MAP COMMAND CENTER ]", 20, 20, 30, GREEN);
        DrawText("- Press 'A' : ADD Target", 20, 60, 20, RAYWHITE);
        DrawText("- Press 'K' : KILL Target", 20, 90, 20, RED);

        DrawQuadtree(q_root);
        DrawRadarTargets(root);

        EndDrawing();
        FreeQuadtree(q_root);
    }

    SaveSystem(root);
    free_btree(root);
    CloseWindow();
    return 0;
}

