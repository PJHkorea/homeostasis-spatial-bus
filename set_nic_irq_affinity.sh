#!/usr/bin/env bash
# =========================================================================
# Homeostasis Spatial Bus - Real-Time Kernel IRQ Affinity Orchestrator
# File: ./telemetry/set_nic_irq_affinity.sh
# =========================================================================
set -euo pipefail

INTERFACE=${1:-eth0}

if [ "$EUID" -ne 0 ]; then
    echo "🚨 인프라 오류: 커널 IRQ 아키텍처를 조작하려면 root 권한이 필수적입니다."
    exit 1
fi

echo "⚡ [STAGE 1] 레거시 irqbalance OS 데몬 영구 처형 및 정지..."
systemctl stop irqbalance 2>/dev/null || true
systemctl disable irqbalance 2>/dev/null || true

echo "⚡ [STAGE 2] 물리 랜카드 [ $INTERFACE ] 직결 NUMA 노드 스캔..."
# 네트워크 카드가 꽂혀있는 PCI 버스선 메모리 뱅크 노드 추출 (포인터 래그 0%)
NUMA_NODE=$(cat /sys/class/net/"$INTERFACE"/device/numa_node)
if [ "$NUMA_NODE" -lt 0 ]; then
    NUMA_NODE=0
fi
echo "├─ 탐지된 네이티브 하드웨어 NUMA Node: $NUMA_NODE"

# 해당 NUMA 노드에 속한 물리 CPU 내부의 논리 스레드 코어 리스트 가로채기
CPU_LIST=$(cat /sys/devices/system/node/node"$NUMA_NODE"/cpulist)
echo "├─ 물리 바인딩 가용 CPU Core Rail : $CPU_LIST"

echo "⚡ [STAGE 3] 하드웨어 링버퍼 IRQ 채널 핀포인트 매핑 개시..."
# 랜카드의 MSI-X 인터럽트 벡터 주소선을 역추적합니다.
IRQS=$(ls /sys/class/net/"$INTERFACE"/device/msi_irqs/)

# NUMA 노드의 전반부 물리 코어 4개(예: 0, 2, 4, 6)를 인터럽트 전용 레일로 래칭 선점
# (나머지 홀수 하이퍼스레딩 코어는 Rust 마스터 사령탑 데몬이 선점 독점하도록 양보)
CORE_INDEX=0
ALLOCATED_CORES=(0 2 4 6) 
NUM_ALLOCATED=${#ALLOCATED_CORES[@]}

for IRQ in $IRQS; do
    TARGET_CORE=${ALLOCATED_CORES[$((CORE_INDEX % NUM_ALLOCATED))]}
    
    # 16진수 비트 마스크(Affinity Mask) 스케일 변환 
    # Core 0 -> 1, Core 2 -> 4, Core 4 -> 10, Core 6 -> 40
    MASK=$(printf "%x" $((1 << TARGET_CORE)))
    
    # 리눅스 커널 실제 인터럽트 가시성 주소선 단에 비트 마스크 강제 주입
    echo "$MASK" > "/proc/irq/$IRQ/smp_affinity"
    
    # 커널 인터럽트 이름 역추적 로그 추출
    IRQ_NAME=$(cat /proc/interrupts | grep -E "^\s*$IRQ:" | awk '{print $NF}' || echo "TxRx-Channel")
    echo "├─ 인터록 성공: IRQ [$IRQ] ($IRQ_NAME) ──> CPU Core [$TARGET_CORE] (Mask: 0x$MASK)"
    
    CORE_INDEX=$((CORE_INDEX + 1))
done

echo "🔥 [SUCCESS] NIC 하드웨어 인터럽트 캐시라인 물리 격리 정렬 완결."
