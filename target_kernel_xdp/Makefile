# =========================================================================
# Homeostasis Spatial Bus - Real-Time Kernel Data Plane Sub-Makefile
# File: target_kernel_xdp/Makefile
# =========================================================================

CLANG ?= clang
LLVM_STRIP ?= llvm-strip
BPFTOOL ?= bpftool

# 전역 가드레일 헌법 ABI 헤더 참조선 지정
CFLAGS := -O2 -g -Wall -target bpf \
          -D__BPF_TRACING__ \
          -I../target_kernel_xdp \
          -I/usr/include/x86_64-linux-gnu

SRC_INGRESS := xdp_spatial_ingress.c
SRC_MUX := bitwise_spatial_mux.c
OBJ_INGRESS := xdp_spatial_ingress.o
OBJ_MUX := bitwise_spatial_mux.o

VMLINUX_H := vmlinux.h

.PHONY: all clean vmlinux_gen

all: vmlinux_gen $(OBJ_INGRESS) $(OBJ_MUX)
	@echo "🛰 [SUCCESS] eBPF 커널 데이터 플레인 바이트코드 합성 완결 완료."

# 1. 런타임 리눅스 커널로부터 CO-RE(Compile Once - Run Everywhere) 구동용 헤더 동적 적출
vmlinux_gen:
	@if [ ! -f $(VMLINUX_H) ]; then \
		echo "🧬 [KERNEL] 현재 기동 중인 커널 환경으로부터 vmlinux.h 구조 명세 역적출 시작..."; \
		$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $(VMLINUX_H) 2>/dev/null || \
		(echo "🚨 치명적 오류: 시스템 BTF 가동 불가. 빌드를 중단합니다." && exit 1); \
	fi

# 2. xdp_spatial_ingress.c 독점 컴파일 (64바이트 래치 하드웨어 정형화)
$(OBJ_INGRESS): $(SRC_INGRESS) $(VMLINUX_H) spatial_maps.h
	$(CLANG) $(CFLAGS) -c $< -o $@
	$(LLVM_STRIP) -g $@

# 3. bitwise_spatial_mux.c 독점 컴파일 (조건문 없는 실리콘 MUX 필터)
$(OBJ_MUX): $(SRC_MUX) $(VMLINUX_H) spatial_maps.h
	$(CLANG) $(CFLAGS) -c $< -o $@
	$(LLVM_STRIP) -g $@

clean:
	rm -f $(OBJ_INGRESS) $(OBJ_MUX) $(VMLINUX_H)
	@echo "🧼 [CLEAN] 커널 빌드 디렉토리 청정 해제."
