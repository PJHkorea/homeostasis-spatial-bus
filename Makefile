# =========================================================================
# Homeostasis Spatial Bus - Master Integrated Infrastructure Makefile
# File: ./Makefile
# =========================================================================

CARGO ?= cargo
RUST_PROXY_DIR := target_proxy_rust
KERNEL_XDP_DIR := target_kernel_xdp
BUILD_DIR := build

.PHONY: all kernel_build rust_proxy test clean

all: directories kernel_build rust_proxy
	@echo "🔥 [GRAND SLAM] 5세대 게이밍 공간 항상성 인프라 전체 생태계 통합 빌드 마감 완결."

# 1. 컴파일 완결 산출물 집약을 위한 마스터 물리 디렉토리 선점
directories:
	@mkdir -p $(BUILD_DIR)

# 2. 하위 커널 Makefile로 통제권을 넘겨서 eBPF 바이트코드 독립 수확 (Recursive Make)
kernel_build:
	@echo "⚡ [STAGE 1] 최전방 리눅스 커널 데이터 플레인 서브 레이어 컴파일 기동..."
	$(MAKE) -C $(KERNEL_XDP_DIR)
	@cp $(KERNEL_XDP_DIR)/*.o $(BUILD_DIR)/

# 3. Rust Cargo 빌드 가동 (내부 build.rs가 NVCC를 자동 연쇄 호출하여 CUDA SASS 자물쇠 결합)
rust_proxy:
	@echo "⚡ [STAGE 2] Rust 마스터 오케스트레이터 및 CUDA 수리 커널 융합 링크 시작..."
	cd $(RUST_PROXY_DIR) && RUSTFLAGS="-C link-arg=-lbpf" $(CARGO) build --release
	@cp $(RUST_PROXY_DIR)/target/release/homeostasis-spatial-proxy $(BUILD_DIR)/ || \
	 cp $(RUST_PROXY_DIR)/target/release/main $(BUILD_DIR)/homeostasis-spatial-proxy 2>/dev/null

# 4. 전 수치해석/대수학 무결성 및 100Gbps 백서 시뮬레이터 일괄 테스트 집행
test:
	@echo "🧪 [BENCHMARK] 1,488만 Mpps 실하중 가드레일 수리 기하학 교차 단언 테스트 가동..."
	PYTHONPATH=. python3 tests/simulation_burst.py
	PYTHONPATH=. python3 tests/mock_packet_injector.py

clean:
	@echo "🧼 전역 인프라 컴파일 아티팩트 및 가속기 런타임 캐시 포어 퍼지 개시..."
	$(MAKE) -C $(KERNEL_XDP_DIR) clean
	cd $(RUST_PROXY_DIR) && $(CARGO) clean
	rm -rf $(BUILD_DIR)
	rm -rf ~/.triton/cache
	@echo "✨ [CLEAN] 실리콘 전 평면 완전 청정화 원형 복원 마감."
