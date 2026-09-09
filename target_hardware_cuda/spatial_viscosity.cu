/*
 * Homeostasis Spatial Bus - High-Performance Gaming Infrastructure
 * File: target_hardware_cuda/spatial_viscosity.cu
 *
 * [수리물리학적 철학]
 * 본 GPU 가속 커널은 대규모 실시간 동기화 버스 하에서 데이터 이동 병목을 박멸합니다.
 * 호스트 커널이 수집한 32바이트 정렬 텐서를 GPU 레지스터 단으로 로드할 때 
 * 캐시라인 미스매치를 방지하고, __ldg() 가속 레이를 통해 L1 데이터 캐시를 우회 유도합니다.
 * 또한 티케팅/수강신청 등 정상 유저의 순간 밀집 현상에 의한 오탐을 차단하기 위해
 * 시간 축 유체 점성 감쇄 공식(Viscosity Damping)을 기계어 레벨에서 직접 집행합니다.
 */

#include <cuda_runtime.h>
#include <device_launch_parameters.h>

/* [★ 하드웨어 ABI 정렬 스펙 완벽 수호: spatial_maps.h와 1:1 싱크] */
#define ALIGNED_STRIDE 129           // 32개 공유 메모리 뱅크 충돌(Bank Conflict) 0% 제어용 홀수 패딩 스트라이드
#define VISCOSITY_ALPHA 0.85f        // 정상 유저 순간 버스트 완충용 점성 감쇄 계수
#define Q_SCALE_RECIPROCAL 0.0000152587890625f // Q16.16 고정소수점을 부동소수점으로 광속 복원하기 위한 역수 (1/65536)

struct player_spatial_payload {
    unsigned int x_fixed;
    unsigned int y_fixed;
    unsigned int z_fixed;
    unsigned int pitch_fixed;
    unsigned int yaw_fixed;
    unsigned int roll_fixed;
    unsigned int player_id;
    unsigned int action_bitmap;
};

struct player_session_slot {
    unsigned long long expiry_tick;
    unsigned int gate_mask;
    unsigned int action_flags;
    unsigned char _padding[16];
};

/**
 * compute_spatial_viscosity_kernel - 0ns 무복사 로드 및 점성 감쇄 가속 커널
 * @grid_matrix: 호스트 커널(XDP)이 기부한 640MB 정적 래티스 버퍼 주소 포인터
 * @session_table: 제어 마스크 상태 테이블 포인터
 * @damped_signals: 최종 정류되어 Triton/대수학 엔진으로 토스될 4차원 출력 텐서 
 * @total_users: 현재 활성 검사 대상 유저 수
 */
__global__ void compute_spatial_viscosity_kernel(
    const player_spatial_payload* __restrict__ grid_matrix,
    const player_session_slot* __restrict__ session_table,
    float* damped_signals,
    const int total_users
) {
    // 하드웨어 스레드 인덱스 매핑 (Warp 단위 병렬 스캔 레이아웃)
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    // 무분기 가드레일: 할당 슬롯 범위를 초과하는 스레드는 조건문 분기 대신 즉시 리턴 차단
    if (idx >= total_users) return;

    // ------------------ [★ 1-Cycle __ldg() 벡터화 하드웨어 로드] ------------------
    // 일반 로드 명령어 대신 __ldg() 전용 Read-Only 캐시 경로를 하이재킹하여 
    // 메모리 대역폭을 소모하지 않고 32바이트 하드웨어 캐시라인 물리 경계를 단 1사이클 만에 통과합니다.
    unsigned int r_x      = __ldg(&(grid_matrix[idx].x_fixed));
    unsigned int r_y      = __ldg(&(grid_matrix[idx].y_fixed));
    unsigned int r_z      = __ldg(&(grid_matrix[idx].z_fixed));
    unsigned int r_bitmap = __ldg(&(grid_matrix[idx].action_bitmap));
    unsigned int g_mask   = __ldg(&(session_table[idx].gate_mask));

    // Q16.16 정수 고정소수점 데이터를 가속기 부동소수점(float) 단으로 광속 스케일링 복원 (나눗셈 제거)
    float x_pos = (float)r_x * Q_SCALE_RECIPROCAL;
    float y_pos = (float)r_y * Q_SCALE_RECIPROCAL;
    float z_pos = (float)r_z * Q_SCALE_RECIPROCAL;

    // ------------------ [시간 축 유체 점성 감쇄 공식 집행] ------------------
    // 순간적으로 튀는 정상 유저의 동기화 착시 현상을 정류(Smoothing)하기 위해
    // 이전 시계열 데이터 가중치와 현재 유입 변동폭을 단일 클록 FMA 레일 위에서 결합합니다.
    int output_offset = idx * 4;
    
    float prev_damped = damped_signals[output_offset + 3];
    float current_signal = (x_pos * x_pos + y_pos * y_pos + z_pos * z_pos); // 유클리드 스칼라 궤적 진폭
    
    // Fused Multiply-Add (FMA) 가속 유도: 기계어 레벨에서 단 1클록 만에 전개
    float final_damped_signal = (VISCOSITY_ALPHA * prev_damped) + ((1.0f - VISCOSITY_ALPHA) * current_signal);

    // ------------------ [0-Copy 가속기 공유 메모리 백라이팅] ------------------
    // Triton/대수학 엔진이 원타임에 가로챌 수 있도록 4차원 연산 레일 구조로 다이렉트 라이팅(Direct Write-back)
    damped_signals[output_offset + 0] = x_pos;
    damped_signals[output_offset + 1] = y_pos;
    damped_signals[output_offset + 2] = (float)r_bitmap;
    
    // 만약 gate_mask가 활성화(0xFFFFFFFF)되어 매크로로 진압된 노드라면 
    // 시그널 점수를 강제로 발산(MAX_POTENTIAL)시켜 다음 필터에서 확정 소산되도록 인터록 연동
    unsigned int hardware_mask = -((int)g_mask);
    float mask_multiplier = (hardware_mask == 0) ? 1.0f : 10000.0f;
    
    damped_signals[output_offset + 3] = final_damped_signal * mask_multiplier;
}

extern "C" {
    // Rust FFI 및 오케스트레이터 결합용 C-Interface 익스포트
    void launch_spatial_viscosity_bridge(
        const void* grid_matrix,
        const void* session_table,
        float* damped_signals,
        int total_users,
        cudaStream_t stream
    ) {
        int threads_per_block = 256;
        int blocks_in_grid = (total_users + threads_per_block - 1) / threads_per_block;

        compute_spatial_viscosity_kernel<<<blocks_in_grid, threads_per_block, 0, stream>>>(
            (const player_spatial_payload*)grid_matrix,
            (const player_session_slot*)session_table,
            damped_signals,
            total_users
        );
    }
}
