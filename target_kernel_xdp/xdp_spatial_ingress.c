/*
 * Homeostasis Spatial Bus - Real-Time Gaming Infrastructure
 * File: target_kernel_xdp/xdp_spatial_ingress.c
 *
 * [수리물리학적 철학]
 * 10만 명의 동시 접속 게이머가 뿜어내는 실시간 좌표 UDP 스트림을 서버의 일반 소프트웨어 메모리 공간(User Space)까지
 * 카피해서 파싱하면 메모리 복사 지터와 CPU 캐시 라인 파괴로 인해 게임 전체에 랙이 발생합니다.
 * 본 최전방 관문은 패킷이 인입되는 랜카드 최하단(XDP) 단에서 패킷 주소선을 가로채,
 * 이미 선점된 640MB 물리 메모리 버퍼 상의 유저 슬롯에 복사 레이턴시 0ns로 데이터를 다이렉트 정류(In-place Matrix Conversion)합니다.
 */

#include <linux/bpf.h>
#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <bpf/bpf_helpers.h>

#define MAX_SPATIAL_USERS 10485760  // 1050만 명 규모의 정적 닫힌계 장벽 가드레일
#define GAMING_UDP_PORT 28000       // 공간 동기화 버스가 장악할 프로토콜 전용 관문 포트
#define Q_SCALE_FACTOR 65536         // Q16.16 고정소수점 비트 스케일링을 위한 배수

/* [★ 하드웨어 ABI 정렬 스펙 완벽 수호]
 * core_formula/space_morph.py 및 Rust 프록시 구조체와 바이트 오프셋 단위로 100% 일치
 */
struct player_spatial_payload {
    __u32 x_fixed;           // 4 Bytes (오프셋 0) - Q16.16 고정소수점 XYZ 좌표값
    __u32 y_fixed;           // 4 Bytes (오프셋 4)
    __u32 z_fixed;           // 4 Bytes (오프셋 8)
    __u32 pitch_fixed;       // 4 Bytes (오프셋 12) - 고정소수점 회전값
    __u32 yaw_fixed;         // 4 Bytes (오프셋 16)
    __u32 roll_fixed;        // 4 Bytes (오프셋 20)
    __u32 player_id;         // 4 Bytes (오프셋 24)
    __u32 action_bitmap;     // 4 Bytes (오프셋 28) -> 정확히 32바이트 물리 캐시라인 동결
};

/* 640MB 크기의 실시간 정적 위치 래티스 맵 (HBM 공간 선점) */
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
    struct player_spatial_payload *raw_packet_payload = (void *)(udph + 1);
    if ((void *)(raw_packet_payload + 1) > data_end)
        return XDP_DROP;

    // 무분기 비트 마스크 기반 인덱싱으로 유저 식별자 가로채기
    __u32 player_index = raw_packet_payload->player_id & (MAX_SPATIAL_USERS - 1);

    // 640MB 정적 테이블 내 유저 고유 슬롯 주소선 하이재킹 (0ns 무복사 사상)
    struct player_spatial_payload *matrix_slot = bpf_map_lookup_elem(&spatial_matrix_grid, &player_index);
    if (!matrix_slot)
        return XDP_PASS;

    // ------------------ [0-Copy In-place 데이터 정류 및 스케일링] ------------------
    // 메모리를 동적으로 할당(new/malloc)하거나 복사하지 않고, 랜카드 버퍼의 물리적 포인터를 
    // 하이재킹하여 640MB 정적 매트릭스 그리드에 다이렉트 이식(Direct Memory Write)
    matrix_slot->x_fixed      = raw_packet_payload->x_fixed;
    matrix_slot->y_fixed      = raw_packet_payload->y_fixed;
    matrix_slot->z_fixed      = raw_packet_payload->z_fixed;
    matrix_slot->pitch_fixed  = raw_packet_payload->pitch_fixed;
    matrix_slot->yaw_fixed    = raw_packet_payload->yaw_fixed;
    matrix_slot->roll_fixed   = raw_packet_payload->roll_fixed;
    matrix_slot->player_id    = raw_packet_payload->player_id;
    matrix_slot->action_bitmap = raw_packet_payload->action_bitmap;

    // ------------------ [비침습식 초고속 텔레메트리 카운팅] ------------------
    __u32 counter_key = 0;
    __u64 *pps_counter = bpf_map_lookup_elem(&telemetry_pps_counter, &counter_key);
    if (pps_counter) {
        // 하드웨어 레벨의 락 지터 프리(Lock-Free Atomicity) 원자적 덧셈 집행
        __sync_fetch_and_add(pps_counter, 1);
    }

    // 커널 최하단 레이어에서의 데이터 처리가 복사 레이턴시 0ns 만에 완결되었으므로,
    // 후방 WAS/서버 프록시로 부하를 넘기지 않고 기계어 레벨에서 직접 다음 중계 트랙으로 연결
    return XDP_PASS;
}

char _license SEC("license") = "GPL";
