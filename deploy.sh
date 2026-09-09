#!/usr/bin/env bash
# =========================================================================
# Homeostasis Spatial Bus - Production One-Touch Deployer
# File: ./deploy.sh
# =========================================================================

# 셀 스크립트 실행 무결성 보호 가드 (에러 즉시 중단 안전 배리어)
set -euo pipefail

# 인프라 마스터 아티팩트 정적 경로 래칭
BUILD_DIR="./build"
XDP_INGRESS_OBJ="${BUILD_DIR}/xdp_spatial_ingress.o"
XDP_MUX_OBJ="${BUILD_DIR}/bitwise_spatial_mux.o"
PROXY_BIN="${BUILD_DIR}/homeostasis-spatial-proxy"

# 최상위 운영체제 루트(root) 권한 가드레일 집행
if [ "$EUID" -ne 0 ]; then
    echo "🚨 인프라 오류: 커널 드라이버 레이어를 조작하려면 반드시 sudo 권한이 필요합니다."
    exit 1
fi

show_usage() {
    echo "========================================================================="
    echo "🎮 Homeostasis Spatial Bus - Infrastructure Lifecycle Management"
    echo "========================================================================="
    echo "Usage: sudo ./deploy.sh [load|unload] [network_interface]"
    echo "Example: sudo ./deploy.sh load eth0"
}

if [ $# -lt 2 ]; then
    show_usage
    exit 1
fi

COMMAND=$1
INTERFACE=$2

load_infrastructure() {
    echo "⚡ [STAGE 1] 1.25 GB 정적 닫힌계 메모리 수호를 위한 커널 가드레일 가동..."
    # 1,280 MB 대규모 물리 페이지 mlock 선점을 위해 OS 메모리 락킹 상한선 무제한 개방
    ulimit -l unlimited

    # 컴파일 타임 기계어 사전 존재 여부 동기화 검증 인터록
    if [ ! -f "$XDP_INGRESS_OBJ" ] || [ ! -f "$PROXY_BIN" ]; then
        echo "🧬 [BUILD] 빌드 아티팩트 유실 감지. 매스터 Makefile을 연쇄 호출합니다..."
        make all
    fi

    echo "⚡ [STAGE 2] 랜카드 최하단 드라이버 레일 하이재킹 적재 집행..."
    # 소프트웨어 모드가 아닌 NIC 하드웨어 드라이버 직접 후크 모드(xdpdrv)로 0ns 무복사 인입 관문 주입
    ip link set dev "$INTERFACE" xdpdrv object "$XDP_INGRESS_OBJ" section xdp_spatial_ingress

    echo "⚡ [STAGE 3] Rust 마스터 사령탑 데몬 비차단 백그라운드 영구 질주 가동..."
    # 호스트 셸 세션이 끊어져도 공간 버스 면역계가 영구 보존되도록 nohup 완전 격리 기동
    nohup "$PROXY_BIN" > /dev/null 2>&1 &
    
    echo "🔥 [SUCCESS] homeostasis-spatial-bus 물리 인터페이스 [ $INTERFACE ] 주입 가동 완결."
}

unload_infrastructure() {
    echo "🧼 [STAGE 1] Rust 마스터 오케스트레이터 시그널 배리어 프로세스 완전 안전 종료..."
    pkill -f "homeostasis-spatial-proxy" || true
    
    echo "🧼 [STAGE 2] 물리 인터페이스 드라이버 구속 해제 및 커널 HBM 해시 맵 메모리 대피..."
    ip link set dev "$INTERFACE" xdp off 2>/dev/null || true
    
    echo "✨ [SUCCESS] 전 평면 클린 인프라 원형 복원 마감 완료."
}

case "$COMMAND" in
    load)
        load_infrastructure
        ;;
    unload)
        unload_infrastructure
        ;;
    *)
        show_usage
        exit 1
        ;;
esac
