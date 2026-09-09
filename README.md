```directory
homeostasis-spatial-bus/
├── config/
│   └── spatial_bounds.toml         # [핵심] 최대 유저수(1000만), 맵 크기, 틱레이트 하드 가드레일
├── core_formula/
│   ├── __init__.py
│   ├── space_morph.py              # 3D 구면 좌표 -> 닫힌 토러스 공간 위상 천이 대수학 엔진
│   └── csg_detector.py             # 공분산 행렬식(Det->0) 기반 매크로/어뷰징 좌표 동기화 저격 엔진
├── target_kernel_xdp/
│   ├── Makefile                    # eBPF 바이트코드 컴파일 명세
│   ├── bitwise_spatial_mux.c       # [트랙1] if문 없는 부호비트(>>63) 기반 광속 패킷 필터/뮤텍스 거세
│   ├── xdp_spatial_ingress.c       # 랜카드 관문. UDP 좌표 패킷 낚아채서 32B 컴팩트 텐서 정류
│   └── spatial_maps.h              # 640MB 정적 물리 메모리 래티스 그리드 매핑 정의 (HBM 선점)
├── target_hardware_cuda/
│   ├── build.rs                    # Rust-CUDA FFI 가속 컴파일 브릿지
│   ├── spatial_viscosity.cu        # 32B 캐시라인 물리 경계 및 __ldg 벡터화 초고속 로드 제어
│   └── spatial_filter.triton       # Triton 기반 1-Cycle FMA 가속 3D 충돌 판정 및 비트맵 브로드캐스트
├── target_proxy_rust/
│   ├── Cargo.toml
│   ├── src/
│   │   ├── main.rs                 # 전체 인프라 오케스트레이션 사령탑
│   │   ├── atomic_swapper.rs       # [트랙2] 유저 액션/스킬 입력 시 0ns 락프리 원자적 포인터 스왑
│   │   └── ring_buffer_monitor.rs  # 커널-GPU 간 0-Copy 순환 버퍼 텔레메트리 모니터링 데몬
├── telemetry/
│   ├── __init__.py
│   └── nvhw_power_monitor.py      # 비침습식 모니터링: PCIe 대역폭 진동 및 GPU 전력 파형 역공학 관제
├── tests/
│   ├── __init__.py
│   ├── mock_packet_injector.py     # 100Gbps급 가상 유저 10만 명 틱 데이터 생성기
│   └── simulation_burst.py         # 실측 검증: 10만 명 난사 시 메모리 추가 할당 진폭 0B 증명 스크립트
├── Dockerfile                      # NVIDIA CUDA 툴킷 및 커널 헤더 파편화 방지 격리 빌드 환경
├── Makefile                        # 전체 이종 언어 소스코드 일괄 합성 및Native 모드 빌드 스크립트
├── deploy.sh                       # Native XDP 드라이버 원터치 적재 및 언로드 자동화 스크립트
└── README.md                       # 수리물리학적 닫힌계 게이밍 버스 아키텍처 선언문

```
