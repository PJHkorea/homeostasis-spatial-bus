/*
 * Homeostasis Spatial Bus - Real-Time Gaming Infrastructure
 * File: target_kernel_xdp/bitwise_spatial_mux.c (1부 고도화 본)
 *
 * [수리물리학적 철학 - 하드웨어 가속 고도화 본]
 * 가속기 커널(spatial_viscosity.cu) 및 Rust 오케스트레이터의 64바이트 래치 명세와 1:1 대칭 정렬을 강제하여
 * 이종 도메인 간 FFI 주소 해킹 시 발생하는 메모리 참조 오프셋 뒤틀림 오류를 물리적으로 원천 파괴합니다.
 */

#include <linux/bpf.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <bpf/bpf_helpers.h>

/* [★ 하드웨어 ABI 정렬 스펙 완벽 수호: 전 시스템 헌법과 동기화] */
#define MAX_SPATIAL_USERS 10485760  // system_bounds.toml 기반 1050만 체급 가드레일
#define Q_SCALE_FACTOR 65536         // 커널 내부 부동소수점 제약 극복을 위한 Q16.16 고정소수점 변환 배수

/* [고도화 핵심] 유저 세션 물리 정적 테이블 구조체 64바이트 완전 격리 승격 */
struct player_session_slot {
    __u64 expiry_tick;       // 8 Bytes (만료 타임스탬프 래치)
    __u32 gate_mask;         // 4 Bytes (대수학 엔진이 마킹한 차단 마스크 비트: 0x00000000 = 정상, 0xFFFFFFFF = 매크로)
    __u32 action_flags;      // 4 Bytes (유저 가변 액션 플래그)
    __u8  _padding[48];      // 48 Bytes -> [버그 픽스] 정확히 64바이트 하드웨어 물리 캐시라인 대칭형 실리콘 슬롯 동결
} __attribute__((aligned(64)));

/* 640MB 크기의 정적 물리 메모리 래티스 버퍼 공간 영구 선점 (HBM 메모리 매핑) */
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, MAX_SPATIAL_USERS);
    __type(key, __u32);
    __type(value, struct player_session_slot);
} spatial_session_table SEC(".maps");

/**
 * execute_silicon_bitwise_mux - 조건문 없는 실리콘 레벨 비트 멀티플렉서
 * @gate_mask: 대수학 저격 엔진이 내린 게이트 신호 (0: 정상, 0xFFFFFFFF: 차단)
 * @normal_action: 자격 합격 시 집행할 액션 (XDP_PASS)
 * @drop_action: 자격 불합격 시 집행할 액션 (XDP_DROP)
 */
static __always_inline int execute_silicon_bitwise_mux(
    __u32 gate_mask, int normal_action, int drop_action
) {
    // gate_mask가 0x00000000이면 hardware_mask = 0x00000000
    // gate_mask가 0xFFFFFFFF이면 hardware_mask = 0xFFFFFFFF
    __s32 hardware_mask = (__s32)gate_mask;

    // 조건문(if/JMP) 없이 오직 비트 논리곱(&)과 논리합(|) 단 1클록 만에 결과 분배 완료
    return (normal_action & ~hardware_mask) | (drop_action & hardware_mask);
}

SEC("xdp_spatial_mux")
int xdp_spatial_gating_handler(struct xdp_md *ctx) {
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;

    // 패킷 기본 바운더리 체크 (eBPF Verifier 통곡의 벽 사전 통과용 정적 가드)
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end)
        return XDP_DROP;

    if (eth->h_proto != __constant_htons(ETH_P_IP))
        return XDP_PASS;

    struct iphdr *iph = (void *)(eth + 1);
    if ((void *)(iph + 1) > data_end)
        return XDP_DROP;

       // 유저의 식별키를 IP 주소 끝자리 혹은 매핑 인덱스로 해킹 선점
    __u32 player_index = __constant_ntohl(iph->saddr) & (MAX_SPATIAL_USERS - 1); // 무분기 링 바운드 마스크

    // 640MB 정적 테이블에서 유저 고유 슬롯 주소선 포인터 가로채기 (무복사 하이재킹)
    struct player_session_slot *slot = bpf_map_lookup_elem(&spatial_session_table, &player_index);
    if (!slot)
        return XDP_PASS; // 테이블 에러 시 기본 패스 가드

    // ------------------ [트랙 1: 무분기 시간 가드레일 전개] ------------------
    __u64 current_ktime = bpf_ktime_get_ns();
    
    // [고도화 포인트] 컴파일러가 최적화 과정에서 slot->expiry_tick 읽기 명령을 레지스터 단에 
    // 임시 임베딩(Latching)하여 실시간 만료 시간을 놓치는 레이스 컨디션 버그를 원천 차단하기 위해,
    // volatile 키워드로 강제 캐스팅하여 메모리 직접 참조(Direct Memory Read)를 집행합니다.
    __u64 target_expiry = *(volatile __u64 *)&slot->expiry_tick;
    
    // 만약 (만료시간 - 현재시간)이 음수가 되면 자동 로그인 세션이 만료된 것임
    __s64 time_delta = (__s64)target_expiry - (__s64)current_ktime;
    
    // 정수 부호 비트를 산술 우측 시프트(>> 63)하여 조건문 없이 0xFFFFFFFF 또는 0x00000000 마스크 유도
    // 세션 유효 시: time_delta >= 0 -> time_invalid_mask = 0x00000000
    // 세션 만료 시: time_delta <  0 -> time_invalid_mask = 0xFFFFFFFF
    __s32 time_invalid_mask = (__s32)(time_delta >> 63);

    // ------------------ [글로벌 제어 플레인 마스크 결합] ------------------
    // [고도화 포인트] 상위 Rust/CUDA 제어 플레인이 원자적(Atomic Swapper)으로 덮어씌우는 gate_mask 역시
    // volatile 메모리 스캔 가드를 적용하여 캐시 일관성 지터를 0ns로 래칭합니다.
    __u32 target_gate_mask = *(volatile __u32 *)&slot->gate_mask;
    
    // 대수학 저격 엔진(csg_detector.py)이 박아둔 차단 신호와 시간 만료 마스크를 비트 논리합(|)으로 융합
    __u32 final_gating_mask = target_gate_mask | (__u32)time_invalid_mask;

    // ------------------ [실리콘 레벨 융합 게이팅 체인 집행] ------------------
    // CPU에게 "이 패킷 버릴까요?" 묻지 않고 기계어 연산 장치 단에서 즉시 액션 스위칭
    int final_action = execute_silicon_bitwise_mux(final_gating_mask, XDP_PASS, XDP_DROP);

    return final_action;
}

char _license[] SEC("license") = "GPL";
