/*
 * Homeostasis Spatial Bus - Enterprise Gaming Infrastructure
 * File: target_kernel_xdp/spatial_maps.h
 *
 * [수리물리학적 철학]
 * 본 헤더 파일은 C 커널, Rust 오케스트레이터, Triton/CUDA 가속기 간에 교환되는
 * 유저 세션 및 3D 공간 좌표의 메모리 레이아웃 ABI(Application Binary Interface)를 고정 선언합니다.
 * 메모리 파편화와 가비지 컬렉션(GC) 지터를 원천 박멸하기 위해
 * 모든 구조체는 32바이트 및 64바이트 캐시라인 경계 배수로 하드락킹(Hard-locking) 정렬됩니다.
 */

#ifndef HOMEOSTASIS_SPATIAL_MAPS_H
#define HOMEOSTASIS_SPATIAL_MAPS_H

#include <linux/types.h>

/* ================= [전역 인프라 수리적 제약 장벽] ================= */
#define MAX_SPATIAL_USERS 10485760  // [2^23 + 2^21] 1050만 명 규모의 정적 닫힌계 바운더리 (spatial_bounds.toml 싱크)
#define Q_SCALE_FACTOR 65536         // 커널 내부 정수 연산을 위한 Q16.16 고정소수점 변환 배수 (2^16)

/* 
 * 1. struct player_spatial_payload (공간 좌표 데이터 플레인 명세)
 * [★ 하드웨어 ABI 정렬: 정확히 64바이트 캐시라인 동결 및 64비트 식별자 수호]
 * core_formula/space_morph.py 및 Triton/CUDA 가속기의 벡터화 로드 파이프라인과
 * 바이트 오프셋 단위로 100% 결합(Address Aliasing)되도록 명시적 패딩 및 정렬 강제.
 */
struct player_spatial_payload {
    __u32 x_fixed;           // 4 Bytes (오프셋 0)  - Q16.16 고정소수점 X 좌표
    __u32 y_fixed;           // 4 Bytes (오프셋 4)  - Q16.16 고정소수점 Y 좌표
    __u32 z_fixed;           // 4 Bytes (오프셋 8)  - Q16.16 고정소수점 Z 좌표
    __u32 pitch_fixed;       // 4 Bytes (오프셋 12) - Q16.16 고정소수점 회전값 Pitch
    __u32 yaw_fixed;         // 4 Bytes (오프셋 16) - Q16.16 고정소수점 회전값 Yaw
    __u32 roll_fixed;        // 4 Bytes (오프셋 20) - Q16.16 고정소수점 회전값 Roll
    __u32 action_bitmap;     // 4 Bytes (오프셋 24) - 유저 입력 키/액션 플래그 마스크
    __u32 _pad_align;        // 4 Bytes (오프셋 28) - 전반부 32바이트 하드웨어 가드레일 경계 패딩
    
    __u64 player_id;         // 8 Bytes (오프셋 32) - [★ 핵심] uint64_t 승격으로 상위 Rust/Python FFI 주소 직결
    __u8  _global_pad[24];   // 24 Bytes (오프셋 40) - 정확히 64바이트 대칭형 실리콘 슬롯 동결을 위한 더미 청크
} __attribute__((packed, aligned(64)));



struct player_session_slot {
    __u64 expiry_tick;       // 세션 만료 타임스탬프 래치
    __u32 gate_mask;         // 대수학 저격 엔진 마킹 비트
    __u32 action_flags;      // 유저 상태 관리 가변 제어 플래그
    __u8  _padding[48];      // 64바이트 물리 정렬을 위한 더미 청크
} __attribute__((packed, aligned(64)));


/* 
 * [수리 기하학적 총 메모리 점유율 명세 증명 - 64바이트 격상 본]
 * - spatial_matrix_grid   : 64바이트 * 10,485,760 entries = 671,088,640 Bytes (정확히 640 MB 선점)
 * - spatial_session_table  : 64바이트 * 10,485,760 entries = 671,088,640 Bytes (정확히 640 MB 선점)
 * - 총합 점유율 : 640MB + 640MB = 정확히 1280 MB (정확히 1.25 GB, 공간 복잡도 O(1) 닫힌계 고정 수호 완결)
 * 
 *  주의: 하위 배포 스크립트(deploy.sh) 내 커널 메모리 상한선 제한(ulimit -l) 설정 시 
 *    반드시 최소 1300MB(1.3GB) 이상의 선점 가드레일 공간이 확보되어 있어야 커널 거부 에러가 터지지 않습니다.
 */


#endif /* HOMEOSTASIS_SPATIAL_MAPS_H */
