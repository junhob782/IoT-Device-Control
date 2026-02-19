/**
 * @file    launcher.c
 * @brief   T-MAP System Boot Loader & Main Menu
 * @details 시스템의 진입점(Entry Point). 사용자의 선택에 따라 GUI 레이더를 실행하거나 종료합니다.
 */

#include <stdio.h>
#include <stdlib.h>
#include <windows.h> // Sleep 함수 등을 위해 필요 (Windows 전용)

// GUI.c에 있는 레이더 실행 함수를 빌려옴 (Extern)
extern int RunRadarGUI(void);

void print_banner() {
    system("cls"); // 화면 지우기
    printf("\n");
    printf("    ========================================================\n");
    printf("    ||                                                    ||\n");
    printf("    ||       T-MAP TACTICAL CONTROL SYSTEM v2.0           ||\n");
    printf("    ||       [ Defense Architecture Solutions ]           ||\n");
    printf("    ||                                                    ||\n");
    printf("    ========================================================\n");
    printf("    ||  SYSTEM STATUS  :  ONLINE                          ||\n");
    printf("    ||  DATA LINK      :  SECURE                          ||\n");
    printf("    ||  GRAPHICS ENG   :  RAYLIB 5.5                      ||\n");
    printf("    ========================================================\n\n");
}

void loading_bar() {
    printf("    [SYSTEM] Booting Core Engine... \n    ");
    for(int i=0; i<30; i++) {
        printf("=");
        Sleep(20); // 0.02초 딜레이로 폼나게 로딩
    }
    printf(" [OK]\n\n");
}

int main() {
    int choice;

    // 1. 멋진 로딩 효과
    loading_bar();

    while (1) {
        // 2. 메인 메뉴 출력
        print_banner();
        printf("    [ MAIN MENU ]\n");
        printf("    1. LAUNCH RADAR GUI (Visual Mode)\n");
        printf("    2. DELETE ALL DATA (Factory Reset)\n");
        printf("    3. EXIT SYSTEM\n\n");
        printf("    COMMAND > ");

        // 3. 사용자 입력 받기
        if (scanf("%d", &choice) != 1) {
            // 문자를 입력했을 때 무한루프 방지
            while(getchar() != '\n'); 
            continue;
        }

        // 4. 분기 처리
        switch (choice) {
            case 1:
                printf("\n    [SYSTEM] Initializing Graphic Interface...\n");
                Sleep(500); 
                RunRadarGUI(); // 아까 이름 바꾼 그 함수 실행!
                break;
            
            case 2:
                printf("\n    [WARNING] Deleting 'tmap_data.dat'...\n");
                remove("tmap_data.dat"); // 파일 삭제 함수
                Sleep(1000);
                printf("    [SYSTEM] Data Wiped Successfully.\n");
                Sleep(1000);
                break;

            case 3:
                printf("\n    [SYSTEM] Shutting Down...\n");
                return 0;

            default:
                printf("\n    [ERROR] Invalid Command.\n");
                Sleep(1000);
        }
    }
    return 0;
}