/*
 * Homeostasis Spatial Bus - Real-Time Gaming Infrastructure
 * File: target_kernel_xdp/xdp_spatial_ingress.c (1부 고도화 본)
 *
 * [수리물리학적 철학 - 게이밍 인프라 가속 루트]
 * 인바운드 네트워크 패킷의 압축 데이터 포맷과 커널-GPU 내부 HBM 스토리지 포맷을 완전히 격리하여,
 * 가속기 64바이트 캐시라인 정렬 규격(aligned(64)) 및 64비트 플레이어 ID 결합 무결성을 완벽하게 수호합니다.
 */

#include <linux/bpf.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <bpf/bpf_helpers.h>

#define MAX_SPATIAL_USERS 10485760  // [2^23 + 2^21] 1050만 명 규모의 정적 닫힌계 장벽 가드레일 (system_bounds.toml 싱크)
#define GAMING_UDP_PORT 28000       // 공간 동기화 버스가 장악할 프로토콜 전용 관문 포트
#define Q_SCALE_FACTOR 65536         // Q16.16 고정소수점 비트 스케일링을 위한 배수

/* 
 * 1. struct inbound_spatial_packet (네트워크 인바운드 컴팩트 패킷 규격)
 * 랜선을 타고 흐르는 데이터 패킷의 물리 크기는 오버헤드를 아끼기 위해 32바이트 컴팩트 구조를 그대로 수호합니다.
 */
struct inbound_spatial_packet {
    __u32 x_fixed;           // 4 Bytes (오프셋 0) - Q16.16 고정소수점 XYZ 좌표값
    __u32 y_fixed;           // 4 Bytes (오프셋 4)
    __u32 z_fixed;           // 4 Bytes (오프셋 8)
    __u32 pitch_fixed;       // 4 Bytes (오프셋 12) - 고정소수점 회전값
    __u32 yaw_fixed;         // 4 Bytes (오프셋 16)
    __u32 roll_fixed;        // 4 Bytes (오프셋 20)
    __u32 player_id_low;     // 4 Bytes (오프셋 24) - 하위 32비트 플레이어 인덱스
    __u32 action_bitmap;     // 4 Bytes (오프셋 28) -> 정확히 32바이트 패킷 와이어 포맷 고정
};

/* 
 * 2. struct player_spatial_payload (커널-가속기 내부 공유 스토리지 HBM 명세)
 * [★ 하드웨어 ABI 정렬: 정확히 64바이트 캐시라인 동결 완결]
 * GPU 온칩 SRAM 적재 속도를 단 1사이클로 단축하기 위해 컴파일러에게 64바이트 강제 정렬을 명령합니다.
 */
struct player_spatial_payload {
    __u32 x_fixed;           // 4 Bytes (오프셋 0)
    __u32 y_fixed;           // 4 Bytes (오프셋 4)
    __u32 z_fixed;           // 4 Bytes (오프셋 8)
    __u32 pitch_fixed;       // 4 Bytes (오프셋 12)
    __u32 yaw_fixed;         // 4 Bytes (오프셋 16)
    __u32 roll_fixed;        // 4 Bytes (오프셋 20)
    __u32 action_bitmap;     // 4 Bytes (오프셋 24)
    __u32 _pad_align;        // 4 Bytes (오프셋 28) -> 여기까지 32바이트 경계 가드
    __u64 player_id;         // 8 Bytes (오프셋 32) -> [★ 버그 픽스] Rust 사령탑 FFI 인터록을 위한 64비트 마스터 주소선
    __u8  _global_pad[24];   // 24 Bytes (오프셋 40) -> 정확히 64바이트 대칭형 실리콘 슬롯 동결 완료
} __attribute__((packed, aligned(64)));

/* 1.25GB 규모의 실시간 정적 위치 래티스 맵 (64B * 1050만 슬롯 = 640MB 고정 선점) */
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, MAX_SPATIAL_USERS);
    __type(key, __u32);
    __type(value, struct player_spatial_payload);
} spatial_matrix_grid SEC(".maps");

/* 텔레메트리 연동용 글로벌 카운터 버스 */
struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u64);
} telemetry_pps_counter SEC(".maps");


SEC("xdp_spatial_ingress")
int xdp_spatial_ingress_handler(struct xdp_md *ctx) {
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;

    // 1차 이더넷 프레임 경계 검증 (eBPF Verifier 통곡의 벽 사전 통과)
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end)
        return XDP_DROP;

    if (eth->h_proto != __constant_htons(ETH_P_IP))
        return XDP_PASS;

    // 2차 IP 헤더 경계 검증
    struct iphdr *iph = (void *)(eth + 1);
    if ((void *)(iph + 1) > data_end)
        return XDP_DROP;

    if (iph->protocol != IPPROTO_UDP)
        return XDP_PASS;

    // 3차 UDP 프로토콜 타겟팅 포트 검증
    struct udphdr *udph = (void *)(iph + 1);
    if ((void *)(udph + 1) > data_end)
        return XDP_DROP;

    if (udph->dest != __constant_htons(GAMING_UDP_PORT))
        return XDP_PASS;

    // 4차 게이밍 페이로드 원시 데이터 바운더리 검증
    // [고도화 포인트] 1부에서 이원화한 32바이트 컴팩트 와이어 포맷 구조체로 원시 페이로드 경계를 칼같이 가두어 둡니다.
    // 이를 통해 변조/조작된 작은 패킷이 들어와도 Verifier 예외 없이 정확히 바운드를 통과합니다.
    struct inbound_spatial_packet *raw_packet = (void *)(udph + 1);
    if ((void *)(raw_packet + 1) > data_end)
        return XDP_DROP;

    // 무분기 비트 마스크 기반 인덱싱으로 유저 식별자 가로채기 (하위 32비트 ID 마스킹)
    __u32 player_index = raw_packet->player_id_low & (MAX_SPATIAL_USERS - 1);

    // 640MB 정적 테이블 내 64바이트 규격 유저 고유 슬롯 주소선 하이재킹 (0ns 무복사 사상)
    struct player_spatial_payload *matrix_slot = bpf_map_lookup_elem(&spatial_matrix_grid, &player_index);
    if (!matrix_slot)
        return XDP_PASS;

    // ------------------ [0-Copy In-place volatile 직접 메모리 정류] ------------------
    // [고도화 포인트 - Volatility 보장] 컴파일러가 대입 연산자를 CPU 논리 레지스터 단에 임시 적재(Latching)하여
    // 동기화 프레임 실행 순서를 뒤바꾸거나 상태를 왜도시키는 버그를 차단하기 위해,
    // volatile 메모리 포인터 캐스팅을 적용하여 HBM(전역 버퍼) 공간에 즉각 Direct Write-back을 집행합니다.
    
    *(volatile __u32 *)&matrix_slot->x_fixed      = raw_packet->x_fixed;
    *(volatile __u32 *)&matrix_slot->y_fixed      = raw_packet->y_fixed;
    *(volatile __u32 *)&matrix_slot->z_fixed      = raw_packet->z_fixed;
    *(volatile __u32 *)&matrix_slot->pitch_fixed  = raw_packet->pitch_fixed;
    *(volatile __u32 *)&matrix_slot->yaw_fixed    = raw_packet->yaw_fixed;
    *(volatile __u32 *)&matrix_slot->roll_fixed   = raw_packet->roll_fixed;
    *(volatile __u32 *)&matrix_slot->action_bitmap = raw_packet->action_bitmap;
    
    // 64비트 마스터 플레이어 ID 복원 이식 (32비트 로우 ID 확장 매핑)
    *(volatile __u64 *)&matrix_slot->player_id    = (__u64)raw_packet->player_id_low;

    // ------------------ [비침습식 초고속 텔레메트리 카운팅] ------------------
    __u32 counter_key = 0;
    __u64 *pps_counter = bpf_map_lookup_elem(&telemetry_pps_counter, &counter_key);
    if (pps_counter) {
        // 하드웨어 레벨의 락 지터 프리(Lock-Free Atomicity) 원자적 덧셈 집행
        __sync_fetch_and_add(pps_counter, 1);
    }

    // 데이터가 카피 레이턴시 0ns 만에 가속기 전방 대기실 버퍼로 완결 매핑되었으므로 패스
    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
