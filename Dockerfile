# syntax=docker/dockerfile:1.4
# =========================================================================
# Homeostasis Spatial Bus - Enterprise Infrastructure Master Dockerfile
# File: ./Dockerfile
# =========================================================================

# -------------------------------------------------------------------------
# STAGE 1: 이종 하드웨어 융합 컴파일러 체인 (Builder Stage)
# -------------------------------------------------------------------------
FROM nvidia/cuda:12.4.1-devel-ubuntu22.04 AS builder

# 1. 커널 데이터 플레인 및 Rust 컴파일을 위한 저수준 핵심 패키지 주입
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    clang \
    llvm \
    libbpf-dev \
    linux-tools-common \
    linux-tools-generic \
    bpftool \
    make \
    curl \
    ca-certificates \
    git \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# 2. Rust 엔터프라이즈 툴체인 독립 래치 적재
ENV RUSTUP_HOME=/usr/local/rustup \
    CARGO_HOME=/usr/local/cargo \
    PATH=/usr/local/cargo/bin:$PATH
RUN curl --proto '=https' --tlsv1.2 -sSf https://rustup.rs | sh -s -- -y --default-toolchain 1.75.0

# 3. 소스코드 트리 전체 동기화 및 닫힌계 디렉토리 선점
WORKDIR /build
COPY . .

# 4. 마스터 Makefile 오케스트레이션 구동 (C 커널 바이트코드, CUDA SASS, Rust 정적 링킹 일괄 합성)
RUN make all

# -------------------------------------------------------------------------
# STAGE 2: 초경량 프로덕션 런타임 및 실리콘 뷰 이식 (Production Stage)
# -------------------------------------------------------------------------
FROM nvidia/cuda:12.4.1-runtime-ubuntu22.04 AS production

ENV DEBIAN_FRONTEND=noninteractive
# 최전방 드라이버 주입을 위한 iproute2 및 비침습 관제용 파이썬 환경만 미니멀 선점
RUN apt-get update && apt-get install -y --no-install-recommends \
    iproute2 \
    python3 \
    python3-pip \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# 비침습 하드웨어 물리 파형 추적기 구동용 고속 수치해석 벡터 라이브러리 적재
RUN pip3 install --no-cache-dir numpy pynvml

WORKDIR /app

# 5. Builder 단계에서 기계어로 완결 합성된 순수 마스터 아티팩트 포인터만 하이재킹 복사
# 컴파일러 노이즈와 소스코드 잔재를 100% 버려 공격 표면(Attack Surface)을 제로화합니다.
COPY --from=builder /build/build/ /app/build/
COPY --from=builder /build/config/ /app/config/
COPY --from=builder /build/telemetry/ /app/telemetry/
COPY --from=builder /build/deploy.sh /app/deploy.sh

# 6. 인프라 강제 가동 엔트리포인트 고정 및 64바이트 ABI 정합성 주입
RUN chmod +x /app/deploy.sh

# 컨테이너 부팅 즉시 NIC 카드 최하단 드라이버 레일에 조건문 없는 실리콘 MUX 필터를 원터치로 이식
ENTRYPOINT ["./deploy.sh"]
CMD ["load", "eth0"]
