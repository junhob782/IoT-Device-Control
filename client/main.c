#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// [핵심] Windows API와 Raylib의 이름 충돌을 완벽하게 격리하는 트릭
// ============================================================================
#if defined(_WIN32)
    // 1. 윈도우 헤더를 읽기 전에, 충돌하는 이름들을 강제로 다른 이름으로 바꿉니다.
    #define Rectangle Win32Rectangle
    #define CloseWindow Win32CloseWindow
    #define ShowCursor Win32ShowCursor

    // 2. 윈도우 네트워크 헤더 포함 (가벼운 버전)
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")

    // 3. 윈도우 헤더를 다 읽었으니, 아까 바꿨던 이름들을 해제합니다.
    #undef Rectangle
    #undef CloseWindow
    #undef ShowCursor

    // 4. 윈도우가 매크로로 선점해버린 함수들도 해제합니다.
    #undef LoadImage
    #undef DrawText
    #undef DrawTextEx
    #undef PlaySound
#endif

// 5. 이제 어떤 방해도 없이 평화롭게 Raylib을 포함합니다!
#include "raylib.h"
#include "../common/packet.h"

// ============================================================================
// 시스템 설정 및 데이터 구조체
// ============================================================================
#define MAX_TARGETS 100
#define PORT 8080

typedef struct {
    TargetPacket data;
    float last_update_time;
    bool active;
} TrackDisplay;

TrackDisplay track_list[MAX_TARGETS];

void UpdateTrack(TargetPacket pkt) {
    for (int i = 0; i < MAX_TARGETS; i++) {
        if (track_list[i].active && track_list[i].data.id == pkt.id) {
            track_list[i].data = pkt;
            track_list[i].last_update_time = (float)GetTime();
            if (pkt.status == 0) track_list[i].active = false;
            return;
        }
    }
    if (pkt.status != 0) {
        for (int i = 0; i < MAX_TARGETS; i++) {
            if (!track_list[i].active) {
                track_list[i].data = pkt;
                track_list[i].active = true;
                track_list[i].last_update_time = (float)GetTime();
                return;
            }
        }
    }
}

int main(void) {
    InitWindow(800, 800, "T-MAP Tactical Radar Display");
    SetTargetFPS(60);

#if defined(_WIN32)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr*)&server_addr, sizeof(server_addr));

#if defined(_WIN32)
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#endif

    memset(track_list, 0, sizeof(track_list));

    while (!WindowShouldClose()) {
        TargetPacket pkt;
        struct sockaddr_in from;
        int from_len = sizeof(from);
        
        while (recvfrom(s, (char*)&pkt, sizeof(TargetPacket), 0, (struct sockaddr*)&from, &from_len) > 0) {
            UpdateTrack(pkt);
        }

        BeginDrawing();
        ClearBackground(BLACK);

        for (int r = 100; r <= 300; r += 100) {
            DrawCircleLines(400, 400, (float)r, Fade(DARKGREEN, 0.5f));
        }
        DrawLine(400, 100, 400, 700, Fade(DARKGREEN, 0.5f)); 
        DrawLine(100, 400, 700, 400, Fade(DARKGREEN, 0.5f)); 

        for (int i = 0; i < MAX_TARGETS; i++) {
            if (!track_list[i].active) continue;

            float time_diff = (float)GetTime() - track_list[i].last_update_time;
            if (time_diff > 5.0f) {
                track_list[i].active = false;
                continue;
            }

            float zoom = 5000.0f; 
            int screen_x = 400 + (int)((track_list[i].data.lon - 127.0) * zoom);
            int screen_y = 400 - (int)((track_list[i].data.lat - 37.5)  * zoom);

            float alpha = fmaxf(0.2f, 1.0f - (time_diff / 5.0f));
            Color baseColor = (track_list[i].data.threat_level >= 8) ? RED : YELLOW;
            Color targetColor = Fade(baseColor, alpha);

            DrawCircle(screen_x, screen_y, 6, targetColor);
            DrawCircleLines(screen_x, screen_y, 10 + sinf((float)GetTime() * 5.0f) * 2.0f, targetColor);
            
            DrawText(TextFormat("ID:%d", track_list[i].data.id), screen_x + 12, screen_y - 15, 16, targetColor);
            DrawText(TextFormat("T:%d", track_list[i].data.threat_level), screen_x + 12, screen_y + 2, 12, targetColor);
        }

        DrawFPS(10, 10);
        DrawText("TAC-RADAR ONLINE", 620, 770, 15, GREEN);
        
        EndDrawing();
    }

    closesocket(s);
#if defined(_WIN32)
    WSACleanup();
#endif
    // 주의: 여기서 CloseWindow는 Raylib의 함수를 정상적으로 호출합니다!
    CloseWindow();

    return 0;
}