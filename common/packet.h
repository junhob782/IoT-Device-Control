/**
 * @file    packet.h
 * @brief   T-MAP System: 서버(Core Engine)와 클라이언트(GUI) 간의 UDP 통신 규약
 * @details 네트워크 데이터 직렬화(Serialization)를 위한 공통 패킷 구조체를 정의합니다.
 */

#ifndef PACKET_H
#define PACKET_H

/* ============================================================================
   [ 메모리 정렬 (Memory Alignment) 강제 제어 ]
   네트워크 전송 시 컴파일러의 임의적인 메모리 패딩(Padding)을 방지하고,
   1바이트 단위로 데이터를 꽉꽉 압축하여 구조체 크기를 고정합니다.
============================================================================ */
#pragma pack(push, 1) 

/**
 * @struct TargetPacket
 * @brief 1개의 전술 표적(Tactical Track) 상태를 담아 나르는 데이터 캡슐
 * @note  총 크기: 4(id) + 8(lat) + 8(lon) + 4(threat) + 4(status) = 28 Bytes
 */
typedef struct {
    // 1. 식별 정보
    int id;               // 표적 고유 식별 번호 (Track ID)

    // 2. 공간 좌표 정보 (Raylib 화면에 찍힐 위치)
    double lat;           // X 좌표 (위도 또는 2D X축)
    double lon;           // Y 좌표 (경도 또는 2D Y축)

    // 3. 전술 상태 정보
    int threat_level;     // 위협도 (숫자가 높을수록 위험. GUI에서 색상(Red/Green) 결정에 사용)
    int status;           // 표적의 현재 상태 (1: 활성화(Active), 0: 격추됨(Destroyed))
    
} TargetPacket;

/* ============================================================================
   메모리 정렬 설정을 원래의 기본값으로 되돌립니다.
   (이후에 선언되는 일반 구조체들의 성능 저하를 막기 위함)
============================================================================ */
#pragma pack(pop) 

#endif // PACKET_H