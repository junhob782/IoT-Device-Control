#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// [단계 1] Windows API와 Raylib의 충돌 방지 및 네트워크 라이브러리 세팅
// ============================================================================
#if defined(_WIN32)
    #define Rectangle Win32Rectangle
    #define CloseWindow Win32CloseWindow
    #define ShowCursor Win32ShowCursor
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    #undef Rectangle
    #undef CloseWindow
    #undef ShowCursor
    #undef LoadImage
    #undef DrawText
    #undef DrawTextEx
    #undef PlaySound
#endif

#include "raylib.h"
#include "../common/packet.h"

// ============================================================================
// [단계 2] 2560x1600 초고해상도 및 렌더링 밸런스 설정
// ============================================================================
#define SERVER_PORT 8080 // K명령을 쏠 서버의 포트
#define CLIENT_PORT 9090 // 서버가 쏘는 데이터를 받을 내 포트

#define SCREEN_W 2560       
#define SCREEN_H 1600       
#define SIDEBAR_W 750       // 방대한 데이터 출력을 위해 750px로 광활하게 확장
#define RADAR_W (SCREEN_W - SIDEBAR_W)
#define CENTER_X (RADAR_W / 2)
#define CENTER_Y (SCREEN_H / 2)

#define MAX_TARGETS 100
#define MAX_TAIL 45         
#define ZOOM 14000.0f       

#define TARGET_SIZE 20.0f   
#define TAIL_SIZE 6.5f      
#define PREDICT_SIZE 5.0f   
#define SELECT_RING 50      

typedef struct { float lat; float lon; } TailPoint;
typedef struct {
    TargetPacket data;
    TailPoint tail[MAX_TAIL]; 
    int tail_idx; 
    int tail_cnt;
    Vector2 velocity;         
    float last_update_time;
    bool active;
} TrackDisplay;

TrackDisplay track_list[MAX_TARGETS];
int selected_id = -1;
void PredictTrajectoryOLS(TrackDisplay* track, int future_steps, Vector2* predicted_pts);
// ============================================================================
// [이벤트 로그 시스템] - 시스템의 모든 행동을 기록
// ============================================================================
#define MAX_LOGS 8
char sys_logs[MAX_LOGS][128];
int log_count = 0;

void AddLog(const char* msg) {
    if (log_count < MAX_LOGS) {
        strcpy(sys_logs[log_count], msg);
        log_count++;
    } else {
        for (int i = 1; i < MAX_LOGS; i++) strcpy(sys_logs[i-1], sys_logs[i]);
        strcpy(sys_logs[MAX_LOGS-1], msg);
    }
}

float GetDist(Vector2 v1, Vector2 v2) { return sqrtf(powf(v1.x - v2.x, 2) + powf(v1.y - v2.y, 2)); }

// ============================================================================
// [기능 1] Quadtree 기반 동적 격자망 시각화
// ============================================================================
void DrawQuadtreeVisualizer(int x, int y, int width, int height, int level) {
    if (level > 4) return; 
    DrawRectangleLines(x, y, width, height, Fade(DARKGREEN, 0.15f));
    
    bool should_split = false;
    for (int i = 0; i < MAX_TARGETS; i++) {
        if (!track_list[i].active) continue;
        int tx = CENTER_X + (int)((track_list[i].data.lon - 127.0) * ZOOM);
        int ty = CENTER_Y - (int)((track_list[i].data.lat - 37.5) * ZOOM);
        if (tx >= x && tx < x + width && ty >= y && ty < y + height) {
            should_split = true; break;
        }
    }

    if (should_split) {
        int half_w = width / 2; int half_h = height / 2;
        DrawQuadtreeVisualizer(x, y, half_w, half_h, level + 1);
        DrawQuadtreeVisualizer(x + half_w, y, half_w, half_h, level + 1);
        DrawQuadtreeVisualizer(x, y + half_h, half_w, half_h, level + 1);
        DrawQuadtreeVisualizer(x + half_w, y + half_h, half_w, half_h, level + 1);
    }
}

// ============================================================================
// [기능 2] 네트워크 데이터 수신 및 물리 연산 갱신
// ============================================================================
void UpdateTrack(TargetPacket pkt) {
    for (int i = 0; i < MAX_TARGETS; i++) {
        if (track_list[i].active && track_list[i].data.id == pkt.id) {
            track_list[i].velocity.x = (pkt.lon - track_list[i].data.lon);
            track_list[i].velocity.y = (pkt.lat - track_list[i].data.lat);

            if (fabs(track_list[i].velocity.x) > 0.000001 || fabs(track_list[i].velocity.y) > 0.000001) {
                track_list[i].tail[track_list[i].tail_idx] = (TailPoint){track_list[i].data.lat, track_list[i].data.lon};
                track_list[i].tail_idx = (track_list[i].tail_idx + 1) % MAX_TAIL;
                if (track_list[i].tail_cnt < MAX_TAIL) track_list[i].tail_cnt++;
            }
            track_list[i].data = pkt;
            track_list[i].last_update_time = (float)GetTime();
            
            // 서버에서 파괴됨(0)을 알림
            if (pkt.status == 0) { 
                track_list[i].active = false; 
                if (selected_id == pkt.id) {
                    AddLog(TextFormat("> [SUCCESS] Target #%04d Eliminated.", selected_id));
                    selected_id = -1; 
                }
            }
            return;
        }
    }
    // 신규 표적 식별
    if (pkt.status != 0) {
        for (int i = 0; i < MAX_TARGETS; i++) {
            if (!track_list[i].active) {
                memset(&track_list[i], 0, sizeof(TrackDisplay));
                track_list[i].data = pkt; track_list[i].active = true;
                track_list[i].last_update_time = (float)GetTime(); 
                AddLog(TextFormat("> [ALERT] New Target #%04d Detected.", pkt.id));
                return;
            }
        }
    }
}

// ============================================================================
// 메인 GUI 렌더링 루프
// ============================================================================
int main(void) {
    InitWindow(SCREEN_W, SCREEN_H, "T-MAP ULTIMATE C4I TERMINAL [GALAXY BOOK PRO MAX]");
    SetTargetFPS(60);

    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    
    // 수신용(9090) 포트 바인딩
    struct sockaddr_in recv_addr;
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(CLIENT_PORT);
    recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(s, (struct sockaddr*)&recv_addr, sizeof(recv_addr)); 

    // 송신용(8080) 서버 주소
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; 
    server_addr.sin_port = htons(SERVER_PORT); 
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    
    u_long mode = 1; ioctlsocket(s, FIONBIO, &mode); 
    memset(track_list, 0, sizeof(track_list));

    AddLog("> [SYSTEM] Radar Link Established.");
    AddLog("> [SYSTEM] Quadtree Visualizer Online.");
    
    float radar_sweep_angle = 0.0f; // 레이더 회전 각도 변수

    while (!WindowShouldClose()) {
        
        // 1. 네트워크 패킷 수신
        TargetPacket pkt; struct sockaddr_in f; int flen = sizeof(f);
        while (recvfrom(s, (char*)&pkt, sizeof(TargetPacket), 0, (struct sockaddr*)&f, &flen) > 0) { 
            UpdateTrack(pkt); 
        }

        // 2. 마우스 제어 및 십자선 피킹
        Vector2 mouse = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            float min_d = 75.0f; int found = -1;
            for (int i = 0; i < MAX_TARGETS; i++) {
                if (!track_list[i].active) continue;
                int tx = CENTER_X + (int)((track_list[i].data.lon - 127.0) * ZOOM);
                int ty = CENTER_Y - (int)((track_list[i].data.lat - 37.5) * ZOOM);
                if (GetDist(mouse, (Vector2){(float)tx, (float)ty}) < min_d) found = track_list[i].data.id;
            }
            if (found != -1 && selected_id != found) {
                selected_id = found;
                AddLog(TextFormat("> [LOCK-ON] Target #%04d Acquired.", selected_id));
            }
        }

        // 3. 키보드 [K] 키 요격 명령 송신
        if (IsKeyPressed(KEY_K) && selected_id != -1) {
            sendto(s, (char*)&selected_id, sizeof(int), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
            AddLog(TextFormat("> [ENGAGE] Intercept Signal Sent: #%04d", selected_id));
        }

        // --------------------------------------------------------------------
        // 레이더망 그리기 시작
        // --------------------------------------------------------------------
        BeginDrawing();
        ClearBackground(GetColor(0x010201FF)); // 딥 다크 스페이스
        DrawRectangle(0, 0, RADAR_W, SCREEN_H, GetColor(0x030603FF));

        // [신규] 능동 레이더 스윕 (360도 회전)
        radar_sweep_angle += 2.0f;
        if (radar_sweep_angle >= 360.0f) radar_sweep_angle = 0.0f;
        DrawCircleSector((Vector2){CENTER_X, CENTER_Y}, 1600.0f, radar_sweep_angle - 40.0f, radar_sweep_angle, 30, Fade(GREEN, 0.15f));
        DrawLineEx((Vector2){(float)CENTER_X, (float)CENTER_Y}, 
                   (Vector2){CENTER_X + cosf(radar_sweep_angle * DEG2RAD) * 1600, CENTER_Y + sinf(radar_sweep_angle * DEG2RAD) * 1600}, 3.0f, GREEN);

        // Quadtree 및 레이더 격자
        DrawQuadtreeVisualizer(0, 0, RADAR_W, SCREEN_H, 0);
        for (int r = 300; r <= 1500; r += 300) {
            DrawCircleLines(CENTER_X, CENTER_Y, (float)r, Fade(GREEN, 0.3f));
            DrawText(TextFormat("%d KM", r/25), CENTER_X + r + 15, CENTER_Y + 15, 24, Fade(GREEN, 0.6f));
        }
        DrawLineEx((Vector2){(float)CENTER_X, 0}, (Vector2){(float)CENTER_X, (float)SCREEN_H}, 2.0f, Fade(GREEN, 0.4f));
        DrawLineEx((Vector2){0, (float)CENTER_Y}, (Vector2){(float)RADAR_W, (float)CENTER_Y}, 2.0f, Fade(GREEN, 0.4f));

        // [신규] 마우스 십자선 (Crosshair & Coordinate Overlay)
        if (mouse.x < RADAR_W) {
            DrawLineEx((Vector2){mouse.x, 0}, (Vector2){mouse.x, SCREEN_H}, 1.0f, Fade(GREEN, 0.5f));
            DrawLineEx((Vector2){0, mouse.y}, (Vector2){RADAR_W, mouse.y}, 1.0f, Fade(GREEN, 0.5f));
            DrawCircleLines((int)mouse.x, (int)mouse.y, 15, Fade(GREEN, 0.8f));
            
            float mouse_lon = 127.0f + (mouse.x - CENTER_X) / ZOOM;
            float mouse_lat = 37.5f - (mouse.y - CENTER_Y) / ZOOM;
            DrawText(TextFormat("LAT: %.5f\nLON: %.5f", mouse_lat, mouse_lon), mouse.x + 20, mouse.y - 40, 20, GREEN);
        }

        // 전술 표적 렌더링 (과거 + 현재 + 미래)
        for (int i = 0; i < MAX_TARGETS; i++) {
            if (!track_list[i].active) continue;
            int tx = CENTER_X + (int)((track_list[i].data.lon - 127.0) * ZOOM);
            int ty = CENTER_Y - (int)((track_list[i].data.lat - 37.5) * ZOOM);
            Color baseCol = (track_list[i].data.threat_level >= 8) ? RED : (track_list[i].data.threat_level >= 5 ? ORANGE : YELLOW);

            // 과거 항적
            for (int j = 0; j < track_list[i].tail_cnt; j++) {
                int idx = (track_list[i].tail_idx - track_list[i].tail_cnt + j + MAX_TAIL) % MAX_TAIL;
                int px = CENTER_X + (int)((track_list[i].tail[idx].lon - 127.0) * ZOOM);
                int py = CENTER_Y - (int)((track_list[i].tail[idx].lat - 37.5) * ZOOM);
                float alpha = ((float)j / MAX_TAIL) * 0.9f;
                DrawCircle(px, py, TAIL_SIZE, Fade(baseCol, alpha));
                if (j > 0) {
                    int prev_idx = (idx - 1 + MAX_TAIL) % MAX_TAIL;
                    int p_x = CENTER_X + (int)((track_list[i].tail[prev_idx].lon - 127.0) * ZOOM);
                    int p_y = CENTER_Y - (int)((track_list[i].tail[prev_idx].lat - 37.5) * ZOOM);
                    DrawLineEx((Vector2){(float)p_x, (float)p_y}, (Vector2){(float)px, (float)py}, 4.0f, Fade(baseCol, alpha * 0.5f));
                }
            }

            // ② 미래 선행 경로(Prediction Path) 그리기 - [AI 선형 회귀 모델 적용]
            Vector2 predicted_path[15];
            PredictTrajectoryOLS(&track_list[i], 15, predicted_path); // AI 함수 호출!

            for (int k = 1; k <= 15; k++) {
                int px = CENTER_X + (int)((predicted_path[k-1].x - 127.0) * ZOOM);
                int py = CENTER_Y - (int)((predicted_path[k-1].y - 37.5) * ZOOM);
                
                DrawCircle(px, py, PREDICT_SIZE, Fade(baseCol, 0.8f - (k/15.0f)));
                // AI가 예측한 점들 사이를 선으로 부드럽게 이어주는 시각 효과 추가
                if (k > 1) {
                    int prev_px = CENTER_X + (int)((predicted_path[k-2].x - 127.0) * ZOOM);
                    int prev_py = CENTER_Y - (int)((predicted_path[k-2].y - 37.5) * ZOOM);
                    DrawLineEx((Vector2){(float)prev_px, (float)prev_py}, (Vector2){(float)px, (float)py}, 2.5f, Fade(baseCol, 0.4f));
                }   
            }

            // ③ 표적 본체 및 LOCK 애니메이션 (이 부분은 기존과 동일하게 유지)
            float angle = atan2f(track_list[i].velocity.y, track_list[i].velocity.x);

// 2. 삼각형의 세 꼭짓점 좌표 계산 (기수가 이동 방향을 향하도록)
Vector2 v1 = { tx + cosf(angle) * TARGET_SIZE, ty - sinf(angle) * TARGET_SIZE }; // 머리
Vector2 v2 = { tx + cosf(angle + 2.5f) * TARGET_SIZE * 0.8f, ty - sinf(angle + 2.5f) * TARGET_SIZE * 0.8f }; // 왼쪽 날개
Vector2 v3 = { tx + cosf(angle - 2.5f) * TARGET_SIZE * 0.8f, ty - sinf(angle - 2.5f) * TARGET_SIZE * 0.8f }; // 오른쪽 날개

// 3. 전술 삼각형 그리기
DrawTriangle(v1, v2, v3, baseCol);
DrawTriangleLines(v1, v2, v3, WHITE); // 테두리를 흰색으로 줘서 뚜렷하게
            DrawText(TextFormat("ID:%04d", track_list[i].data.id), tx + 25, ty - 25, 20, WHITE);
            if (selected_id == track_list[i].data.id) {
                DrawCircleLines(tx, ty, SELECT_RING + (int)(sinf(GetTime()*15)*12), GREEN);
                DrawPolyLinesEx((Vector2){(float)tx, (float)ty}, 6, SELECT_RING + 15, (float)GetTime()*200.0f, 4.0f, GREEN);
            }
        }

        // --------------------------------------------------------------------
        // 우측 대시보드 (750px 초거대 정보 패널)
        // --------------------------------------------------------------------
        DrawRectangle(RADAR_W, 0, SIDEBAR_W, SCREEN_H, GetColor(0x0A0A0AFF));
        DrawLineEx((Vector2){(float)RADAR_W, 0}, (Vector2){(float)RADAR_W, (float)SCREEN_H}, 4.0f, GREEN);
        DrawText("C4I COMMAND CONSOLE", RADAR_W + 50, 70, 52, SKYBLUE);
        DrawLineEx((Vector2){(float)RADAR_W + 50, 140}, (Vector2){(float)SCREEN_W - 50, 140}, 2.0f, DARKGREEN);

        // [신규] 실시간 교전 목록 (Active Target Roster)
        DrawText("[ ACTIVE SENSORS ]", RADAR_W + 50, 180, 26, GRAY);
        int roster_y = 220;
        for(int i=0; i<MAX_TARGETS; i++) {
            if(track_list[i].active) {
                Color c = track_list[i].data.threat_level >= 8 ? RED : ORANGE;
                DrawText(TextFormat("TRK #%04d | THREAT: %d", track_list[i].data.id, track_list[i].data.threat_level), RADAR_W + 50, roster_y, 28, c);
                roster_y += 35;
                if(roster_y > 450) break; // 최대 6~7개만 표시
            }
        }

        // 선택된 표적 상세 텔레메트리
        if (selected_id != -1) {
            TrackDisplay* sel = NULL;
            for(int i=0; i<MAX_TARGETS; i++) {
                if(track_list[i].active && track_list[i].data.id == selected_id) { sel = &track_list[i]; break; }
            }
            if (sel) {
                int start_y = 520;
                DrawText(TextFormat("LOCKED ID: [ #%04d ]", sel->data.id), RADAR_W + 60, start_y, 48, WHITE);
                DrawRectangle(RADAR_W + 50, start_y + 80, SIDEBAR_W - 100, 320, GetColor(0x151515FF));
                DrawText("PRECISION TELEMETRY", RADAR_W + 80, start_y + 110, 26, GRAY);
                DrawText(TextFormat("LAT: %.7f", sel->data.lat), RADAR_W + 80, start_y + 170, 42, YELLOW);
                DrawText(TextFormat("LON: %.7f", sel->data.lon), RADAR_W + 80, start_y + 240, 42, YELLOW);

                int tl = sel->data.threat_level;
                DrawText(TextFormat("THREAT ASSESSMENT: %d", tl), RADAR_W + 60, start_y + 440, 36, (tl>=8?RED:ORANGE));
                DrawRectangle(RADAR_W + 60, start_y + 490, SIDEBAR_W - 120, 50, DARKGRAY);
                DrawRectangle(RADAR_W + 60, start_y + 490, (int)((SIDEBAR_W - 120) * (tl/10.0f)), 50, (tl>=8?RED:ORANGE));
                
                DrawRectangleLines(RADAR_W + 60, start_y + 600, SIDEBAR_W - 120, 120, RED);
                DrawText("WEAPON SYSTEM ARMED", RADAR_W + 110, start_y + 625, 30, RED);
                DrawText("[ PRESS 'K' TO INTERCEPT ]", RADAR_W + 90, start_y + 675, 36, RED);
            }
        }

        // [신규] 전술 이벤트 로그 터미널
        DrawLineEx((Vector2){(float)RADAR_W + 50, 1330}, (Vector2){(float)SCREEN_W - 50, 1330}, 2.0f, DARKGRAY);
        DrawText("SYSTEM EVENT LOG", RADAR_W + 50, 1350, 24, DARKGRAY);
        for(int i = 0; i < log_count; i++) {
            DrawText(sys_logs[i], RADAR_W + 50, 1390 + (i * 25), 22, LIGHTGRAY);
        }

        DrawFPS(20, 20);
        EndDrawing();
    }
    closesocket(s); WSACleanup(); CloseWindow();
    return 0;

    
}

// ============================================================================
// [신규 AI 모듈] OLS 선형 회귀(Linear Regression) 기반 궤적 예측 엔진
// 과거의 모든 항적 데이터를 분석하여 최적의 미래 추세선을 수학적으로 도출합니다.
// ============================================================================
void PredictTrajectoryOLS(TrackDisplay* track, int future_steps, Vector2* predicted_pts) {
    int N = track->tail_cnt;
    
    // 데이터가 2개 미만이라 회귀 분석이 불가능할 경우 (초기 스폰 상태)
    if (N < 2) { 
        for(int k = 1; k <= future_steps; k++) {
            predicted_pts[k-1].x = track->data.lon + track->velocity.x * k * 4.0f;
            predicted_pts[k-1].y = track->data.lat + track->velocity.y * k * 4.0f;
        }
        return;
    }

    float sum_t = 0, sum_t2 = 0;
    float sum_lat = 0, sum_t_lat = 0;
    float sum_lon = 0, sum_t_lon = 0;

    // 원형 큐(순환 배열)를 시간 순서대로 풀어서 선형 회귀 연산 (O(N))
    for (int i = 0; i < N; i++) {
        int idx = (track->tail_idx - N + i + MAX_TAIL) % MAX_TAIL;
        float t = (float)i;
        float lat = track->tail[idx].lat;
        float lon = track->tail[idx].lon;

        sum_t += t;
        sum_t2 += t * t;
        sum_lat += lat;
        sum_t_lat += t * lat;
        sum_lon += lon;
        sum_t_lon += t * lon;
    }

    float denominator = (N * sum_t2) - (sum_t * sum_t);
    if (fabs(denominator) < 0.00001f) denominator = 0.00001f; // 0 나누기 방지

    // 위도(Lat)와 경도(Lon)에 대한 각각의 회귀 계수(기울기 a, 절편 b) 도출
    float a_lat = ((N * sum_t_lat) - (sum_t * sum_lat)) / denominator;
    float b_lat = (sum_lat - (a_lat * sum_t)) / N;

    float a_lon = ((N * sum_t_lon) - (sum_t * sum_lon)) / denominator;
    float b_lon = (sum_lon - (a_lon * sum_t)) / N;

    // 도출된 머신러닝 모델(방정식)에 미래의 시간(t)을 대입하여 예측 좌표 산출
    for(int k = 1; k <= future_steps; k++) {
        float future_t = (float)(N - 1) + (k * 4.0f); // 미래 시간 스텝 대입
        predicted_pts[k-1].y = a_lat * future_t + b_lat; // 예측된 위도
        predicted_pts[k-1].x = a_lon * future_t + b_lon; // 예측된 경도
    }
}