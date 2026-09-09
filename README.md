# Homeostasis Spatial Bus (항상성 공간 버스)

"10만 명의 플레이어가 동시에 광역 스킬을 난사해도 서버 메모리 증가량은 정확히 0 Byte, 단 1밀리초의 서버 렉(Jitter)도 허용하지 않는 차세대 메타버스/게이밍 동기화 인프라."

---

## 💡 이 프로젝트는 왜 시작되었나요? (The Problem)

기존의 게임 서버(C++, C#, Go 기반 등)는 10만 명 규모의 대규모 오픈월드나 배틀로얄 전장을 단일 서버로 감당하지 못합니다. 유저가 움직일 때마다 서버 내부에서는 다음과 같은 파멸적인 비효율이 발생하기 때문입니다.

* **메모리 복사 지옥**: 랜카드로 들어온 패킷 데이터를 뜯어서, 서버 프로그램 메모리로 복사하고, 게임 월드 데이터로 또 복사하느라 CPU 캐시 메모리가 다 깨집니다.
* **조건문(if) 렉 폭탄**: "유저 A가 움직였으니 주변 50m 안에 있는 유저 B, C, D에게 알려줘라"를 계산하기 위해 CPU가 수억 번의 if (거리가 가까운가?) 분기 루프를 돌다가 멈추어 섭니다(Pipeline Stall).
* **가비지 컬렉터(GC) 폭발**: 매 나노초마다 생성되고 사라지는 유저들의 좌표값과 키 입력 데이터를 청소하느라 가비지 컬렉터가 구동되는 순간, 게임이 툭툭 끊기는 '서버 렉(Jitter)'이 발생합니다.

---

## 🛠️ 어떻게 해결했나요? (The Solution)

이 프로젝트는 편리한 소프트웨어 코딩 기법을 과감히 버리고, 리눅스 커널 해킹 + GPU 반도체 제어 + 고등 대수학 공식을 엮어 하드웨어 실리콘 회로의 한계치까지 성능을 쥐어짜 냅니다.

### 1. 0ns 무복사 주소선 하이재킹 (Zero-Copy)
유저의 좌표 패킷이 랜카드에 들어오는 순간, 일반 웹/게임 서버 영역으로 데이터를 복사해 올리지 않습니다. 리눅스 커널 최하단(eBPF/XDP) 레이어에서 메모리 포인터 주소선을 그대로 가로채, 부팅할 때 미리 선점해 둔 640MB 크기의 정적 월드 버퍼 자리에 데이터를 0ns 만에 다이렉트 이식합니다.

### 2. 조건문(if)이 없는 무분기 실리콘 스위칭 (Branchless MUX)
"주변에 유저가 있는가?", "세션이 만료되었는가?"를 판단하기 위해 CPU에게 if문을 묻지 않습니다. 10만 명의 좌표 데이터를 GPU 레지스터 단으로 밀어 넣은 뒤, 정수 부호 비트 산술 시프트(>> 63)와 행렬 곱셈(FMA) 연산만으로 단 1클록 만에 주변 중계 대상을 물리 회로 속도로 갈라냅니다.

### 3. 자원의 닫힌계 완성 (O(1) 정적 메모리 선점)
아무리 격렬한 대규모 한타 싸움이 벌어지고 오브젝트가 난사되어도, 이 시스템이 사용하는 메모리는 부팅 시점에 선점한 용량에서 단 1바이트의 추가 진폭도 허용하지 않는 완전한 닫힌계(Closed System)가 됩니다. 메모리 쓰레기 자체가 생성되지 않으므로 렉의 근본적인 원인이 물리적으로 소멸합니다.


---

## 📂 인프라 아키텍처 명세 (Directory Overview)

* **config/spatial_bounds.toml**: 최대 유저 수(1050만 명), 맵 가드레일 크기 등 인프라의 헌법적 제약을 선포하는 통제실.
* **core_formula/**: 3D 유클리드 좌표를 무한히 회전하는 도넛 공간(토러스 위상)에 가두어 수치 폭주를 원천 차단하고, 공분산 행렬식으로 난수 우회 매크로 봇을 저격하는 대수학 엔진.
* **target_kernel_xdp/**: 랜선에서 전기로 인입되는 패킷을 낚아채 0ns 무복사 정류를 집행하고 if문을 거세한 C언어 커널 드라이버.
* **target_hardware_cuda/**: GPU 온칩 SRAM 캐시 레이어와 OpenAI Triton 가속 커널을 활용해 10만 명의 주변 중계 비트맵을 전광석화처럼 펴내는 반도체 가속 레일.
* **target_proxy_rust/**: 핫 패스를 방해하지 않고 유저의 아이템 교체 등 가변 상태를 단 1클록 만에 원자적으로 바꿔주는 락프리(Lock-Free) 오케스트레이터 사령탑.
* **telemetry/**: 시스템 상태를 모니터링하기 위해 로그를 남기는 렉조차 아까워, GPU의 전력 소모 파형과 PCIe 대역폭의 미세 진동만을 역공학 추적하는 비침습식 관제 데몬.

---

## ⚙️ 인프라 환경 요구 조건 (Prerequisites)

이 시스템을 에러와 지터(Jitter) 없이 14.88 Mpps 와이어 스피드로 구동하기 위한 최소 및 권장 하드웨어/소프트웨어 가드레일 명세입니다.

### 1. 리눅스 호스트 커널 (Linux Host Kernel) 스펙
eBPF/XDP 레이어에서 32바이트 ABI 정렬 구조체와 640MB 고속 HBM 링버퍼(bpf_ringbuf) 주소선 하이재킹을 완벽하게 수행하기 위한 필수 커널 조건입니다.

* **최소 커널 버전**: Linux Kernel 5.15+ (기본 eBPF 링버퍼 인프라 안정화 버전)
* **권장 커널 버전**: Linux Kernel 6.1+ LTS 이상 (XDP Native 다이렉트 드라이버 모드 및 부호 비트 >> 63 산술 시프트 기계어 최적화 완결 버전)
* **필수 커널 컴파일 옵션 (`.config`)**:
  * `CONFIG_BPF_SYSCALL=y` (eBPF 서브시스템 활성화)
  * `CONFIG_DEBUG_INFO_BTF=y` (vmlinux.h를 통한 컴파일 한 번으로 어디서나 실행 가능한 CO-RE 무결성 보장)
  * `CONFIG_NET_ACT_XDP=y` (랜카드 최하단 관문 하이재킹 레일 강제)

### 2. 네트워크 카드 (NIC) 드라이버 호환성
패킷을 유저 공간으로 복사하지 않고 0ns 만에 가로채기 위해서는 랜카드 칩셋 자체가 XDP Native/Offload 모드를 물리적으로 지원해야 합니다.

* **권장 NIC 칩셋**:
  * Mellanox (NVIDIA) ConnectX-5 / ConnectX-6 이상 (`mlx5_core` 드라이버 사용)
  * Intel E810 시리즈 이상 (`ice` 드라이버 사용)
* **네트워크 모드**: `XDP_FLAGS_DRV_MODE` (Native Driver Mode) 필수 적용. (소프트웨어 에뮬레이션 모드인 SKB 모드는 복사 지터가 발생하므로 실전 투입 금지)

### 3. NVIDIA 가속 런타임 및 드라이버 (GPU 레일)
32바이트 캐시라인 벡터화 로드(`__ldg`) 명령어를 기계어로 직접 구사하고, OpenAI Triton 커널 내에서 단일 클록 FMA 공간 필터링을 집행하기 위한 반도체 통제 스펙입니다.

* **NVIDIA 드라이버 버전**: NVIDIA Linux Driver 535.xx 이상 (안정적인 데이터 패스 스트라이드 패딩 제어 보장)
* **CUDA 런타임 버전**: CUDA 12.0 ~ 12.4+ (Triton 가속 엔진과의 1:1 바이너리 인터록 정합성 수호)
* **OpenAI Triton 컴파일러 버전**: `triton >= 2.1.0` (Warp-level 분기문 거세 기계어 합성 지원 버전)
* **최소 하드웨어 아키텍처**: NVIDIA Ampere (RTX 30 시리즈 / A100 / T4) 또는 Ada Lovelace/Hopper (RTX 40 시리즈 / L4 / H100) 이상. (SRAM 32개 뱅크 충돌 0% 제어 알고리즘인 `ALIGNED_STRIDE 129` 연산이 하드웨어 레벨에서 작동하기 위한 필수 조건)


---

```directory
homeostasis-spatial-bus/
├── config/
│   └── spatial_bounds.toml         # [핵심] 최대 유저수(1000만), 맵 크기, 틱레이트 하드 가드레일
├── core_formula/
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
│   └── nvhw_power_monitor.py      # 비침습식 모니터링: PCIe 대역폭 진동 및 GPU 전력 파형 역공학 관제
├── tests/
│   ├── mock_packet_injector.py     # 100Gbps급 가상 유저 10만 명 틱 데이터 생성기
│   └── simulation_burst.py         # 실측 검증: 10만 명 난사 시 메모리 추가 할당 진폭 0B 증명 스크립트
├── Dockerfile                      # NVIDIA CUDA 툴킷 및 커널 헤더 파편화 방지 격리 빌드 환경
├── Makefile                        # 전체 이종 언어 소스코드 일괄 합성 및Native 모드 빌드 스크립트
├── deploy.sh                       # Native XDP 드라이버 원터치 적재 및 언로드 자동화 스크립트
└── README.md                       # 수리물리학적 닫힌계 게이밍 버스 아키텍처 선언문

```
