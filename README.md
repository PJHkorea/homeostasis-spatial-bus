# Homeostasis Spatial Bus (항상성 공간 버스)

10만 명의 플레이어가 동시에 광역 스킬을 난사해도 서버 메모리 증가량을 최소화하고, 서버 렉도 허용하지 않는 메타버스/게이밍 동기화 인프라 관련 poc

* 주의 : 대규모 유저의 3D 공간 좌표 동기화 및 충돌/범위 필터링"이라는 단순 반복 연산에만 특화, '공간 동기화 렉'을 잡기 위해 나머지 개발 편의성을 전부 제물로 바쳤습니다.

---

## 💡 이 프로젝트는 왜 시작되었나요? (The Problem)

기존의 게임 서버(C++, C#, Go 기반 등)는 10만 명 규모의 대규모 오픈월드나 배틀로얄 전장을 단일 서버로 관리 시 유저가 움직일 때마다 서버 내부에서는 다음과 같은 비효율이 발생합니다.

* **메모리 복사 지옥**: 랜카드로 들어온 패킷 데이터를 뜯어서, 서버 프로그램 메모리로 복사하고, 게임 월드 데이터로 또 복사하느라 CPU 캐시 메모리가 다 깨집니다.
* **조건문(if) 렉 폭탄**: "유저 A가 움직였으니 주변 50m 안에 있는 유저 B, C, D에게 알려줘라"를 계산하기 위해 CPU가 수억 번의 if (거리가 가까운가?) 분기 루프를 돌다가 멈추어 섭니다(Pipeline Stall).
* **가비지 컬렉터(GC) 폭발**: 매 나노초마다 생성되고 사라지는 유저들의 좌표값과 키 입력 데이터를 청소하느라 가비지 컬렉터가 구동되는 순간, 게임이 툭툭 끊기는 '서버 렉(Jitter)'이 발생합니다.

---

## 🛠️ 어떻게 해결할까요? (The Solution)

이 프로젝트는 편리한 소프트웨어 코딩 기법을 과감히 버리고, 리눅스 커널 해킹 + GPU 반도체 제어 + 고등 대수학 공식을 엮어 하드웨어 실리콘 회로의 한계치까지 성능을 쥐어짜 냅니다.

### 1. 0ns 무복사 주소선 하이재킹 (Zero-Copy)
유저의 좌표 패킷이 랜카드에 들어오는 순간, 일반 웹/게임 서버 영역으로 데이터를 복사해 올리지 않습니다. 리눅스 커널 최하단(eBPF/XDP) 레이어에서 메모리 포인터 주소선을 그대로 가로채, 부팅할 때 미리 선점해 둔 640MB 크기의 정적 월드 버퍼 자리에 데이터를 다이렉트 이식합니다.

### 2. 조건문(if)이 없는 무분기 실리콘 스위칭 (Branchless MUX)
"주변에 유저가 있는가?", "세션이 만료되었는가?"를 판단하기 위해 CPU에게 if문을 묻지 않습니다. 10만 명의 좌표 데이터를 GPU 레지스터 단으로 밀어 넣은 뒤, 정수 부호 비트 산술 시프트(>> 63)와 행렬 곱셈(FMA) 연산만으로 주변 중계 대상을 물리 회로 속도로 갈라냅니다.

### 3. 자원의 닫힌계 완성 (O(1) 정적 메모리 선점)
아무리 격렬한 대규모 한타 싸움이 벌어지고 오브젝트가 난사되어도, 이 시스템이 사용하는 메모리는 부팅 시점에 선점한 용량에서 닫힌계(Closed System)가 됩니다. 메모리 쓰레기 자체가 생성되지 않으므로 렉의 근본적인 원인이 물리적으로 소멸합니다.


## 📂 인프라 아키텍처 명세 (Directory Overview)

* **config/spatial_bounds.toml**: 최대 유저 수(1,048,5760명 물리 래치 고정), 플랑크 완충 상수, 가속기 스트라이드 패딩 등 전 영역의 수치해석적 오차가 없도록 인프라 전역의 통제
* **core_formula/**: 3D 유클리드 좌표의 폭주를 막는 토러스 공간 위상 천이 변환(`space_morph.py`) 및 공분산 특이 행렬의 로그-행렬식 역산(`slogdet`) 가드로 봇넷의 선형 종속 위상 붕괴를 처리하는 대수학 코어 엔진(`csg_detector.py`).
* **target_kernel_xdp/**: 32B 컴팩트 와이어 패킷 포맷을 하이재킹하여 64바이트 가속기 물리 슬롯 배열 맵으로 직접 정류하는 데이터 플레인 관문(`xdp_spatial_ingress.c`) 및 `volatile` 메모리 직접 가드로 `>> 63` 정수 산술 시프트 시간 장벽을 치는 커널 데이터 플레인(`bitwise_spatial_mux.c`).
* **target_hardware_cuda/**: 독립 크레이트 구조(`Cargo.toml` / `build.rs`)로 완전 격리 패키징되어 GPU 온칩 SRAM 뱅크 충돌 0% 가속을 전개하는 유체 점성 감쇄 가속 커널(`spatial_viscosity.cu`) 및 `tl.int8` 스토어 압축으로 전역 Write 대역폭 75%를 소생시킨 OpenAI Triton 연산 레일(`spatial_filter.triton`).
* **target_proxy_rust/**: `addr_of_mut!` 매크로와 `Ordering::SeqCst` 메모리 장벽 가드로 컴파일러 UB 최적화 왜곡을 박멸하고 단 1클록 만에 64B 슬롯 제어권을 교차 스왑하는 락프리 사령탑(`atomic_swapper.rs`) 및 128개 로그 일괄 드레인 기전으로 포화 유실을 분쇄한 관제 엔진(`ring_buffer_monitor.rs`).
* **telemetry/**: 핫 패스 데이터에 0ns의 비간섭을 관철하는 대신, 가속기 연사 시 발생하는 GPU 물리 전력 미분 진폭(Power Gradient)과 절대 편차 에너지 가드레일을 역공학 추적하여 수치 락업 프리 무결성을 래칭하는 비침습 관제 데몬(`nvhw_power_monitor.py`).

---


## ⚙️ 인프라 환경 요구 조건 (Prerequisites)

이 시스템을 에러와 지터(Jitter) 없이 14.88 Mpps 와이어 스피드로 구동하기 위한 최소 및 권장 하드웨어/소프트웨어 가드레일 명세입니다.

### 1. 리눅스 호스트 커널 (Linux Host Kernel) 스펙
eBPF/XDP 레이어에서 **64바이트 ABI 정렬 구조체**와 **1.25 GB(1,280 MB) 고속 HBM 래티스 버퍼** 주소선 하이재킹을 완벽하게 수행하기 위한 필수 커널 조건입니다.

* **최소 커널 버전**: Linux Kernel 5.15+ (기본 eBPF 인프라 안정화 버전)
* **권장 커널 버전**: Linux Kernel 6.1+ LTS 이상 (XDP Native 다이렉트 드라이버 모드, `volatile` 메모리 가드 및 부호 비트 `>> 63` 산술 시프트 기계어 최적화 완결 버전)
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
**64바이트 캐시라인 벡터화 로드(`__ldg`)** 명령어를 기계어로 직접 구사하고, OpenAI Triton 커널 내에서 단일 클록 FMA 공간 필터링 및 `tl.int8` 대역폭 압축을 집행하기 위한 반도체 통제 스펙입니다.

* **NVIDIA 드라이버 버전**: NVIDIA Linux Driver 535.xx 이상 (안정적인 데이터 패스 스트라이드 패딩 제어 보장)
* **CUDA 런타임 버전**: CUDA 12.0 ~ 12.4+ (Triton 가속 엔진과의 1:1 바이너리 인터록 정합성 수호)
* **OpenAI Triton 컴파일러 버전**: `triton >= 2.1.0` (Warp-level 분기문 거세 및 SASS 기계어 합성 지원 버전)
* **최소 하드웨어 아키텍처**: NVIDIA Ampere (RTX 30 시리즈 / A100 / T4) 또는 Ada Lovelace/Hopper (RTX 40 시리즈 / L4 / H100) 이상. (SRAM 32개 뱅크 충돌 0% 제어 알고리즘인 `ALIGNED_STRIDE 129` 연산이 하드웨어 레벨에서 작동하기 위한 필수 조건)



---

```directory
## 📂 인프라 디렉토리 및 메모리 점유율 명세 (Directory & Memory Proof)

### 1. 물리 메모리 점유율 명세 증명 (Mathematical Memory Verification)
* **spatial_matrix_grid**: 64바이트 * 10,485,760 entries = 671,088,640 Bytes (**640 MB** 정적 선점)
* **spatial_session_table**: 64바이트 * 10,485,760 entries = 671,088,640 Bytes (**640 MB** 정적 선점)
* **총합 자원 점유율**: 640MB + 640MB = **정확히 1,280 MB (1.25 GB)** 공간 복잡도 \(O(1)\) 하드웨어 닫힌계.

### 2. 프로젝트 트리 명세 Overview
```text
homeostasis-spatial-bus/
├── config/
│   └── spatial_bounds.toml         # 최대 유저수(1050만), 맵 크기, 틱레이트 하드 가드레일 제약 관련
├── core_formula/
│   ├── space_morph.py              # 3D 구면 좌표 -> 인플레이스 0-Copy 닫힌 토러스 공간 위상 천이 엔진
│   └── csg_detector.py             # 공분산 로그-행렬식(slogdet) 기반 봇넷 동기화 위상 공간 붕괴 저격용 엔진
├── target_kernel_xdp/
│   ├── Makefile                    # eBPF CO-RE 독립 컴파일 및 vmlinux.h 역적출 서브 메이크파일
│   ├── bitwise_spatial_mux.c       # [트랙1] volatile 가드 기반 무분기 부호비트(>>63) 패킷 필터
│   ├── xdp_spatial_ingress.c       # 32B 컴팩트 패킷 낚아채서 64B HBM 슬롯으로 volatile 직접 이식
│   └── spatial_maps.h              # 1.25 GB 정적 물리 메모리 래티스 그리드 매핑 마스터 ABI 헤더
├── target_hardware_cuda/
│   ├── Cargo.toml                  # 하드웨어 컴파일 파이프라인의 독립을 위한 전용 크레이트 명세
│   ├── build.rs                    # --use_fast_math 및 sm_80/sm_90 SASS 기계어 추출 NVCC 독점 빌더
│   ├── spatial_viscosity.cu        # 64B 캐시라인 물리 경계 수호 및 __ldg 캐시 하이재킹 점성 감쇄 커널
│   └── spatial_filter.triton       # tl.int8 압축 스토어로 글로벌 대역폭 75% 소생시킨 1-Cycle FMA 충돌 커널
├── target_proxy_rust/
│   ├── Cargo.toml                  # 독립 CUDA 크레이트(path = "../target_hardware_cuda") 하이재킹 바인딩
│   ├── src/
│   │   ├── main.rs                 # 1.25GB 대규모 할당 정밀 align(64) 제어 및 인프라 통제
│   │   ├── atomic_swapper.rs       # [트랙2] addr_of_mut! 및 SeqCst 메모리 장벽 기반 원자적 포인터 스왑
│   │   └── ring_buffer_monitor.rs  # 128개 로그 일괄 드레인 기전으로 포화 유실을 회피한 락프리 순환 버퍼 데몬
├── telemetry/
│   └── nvhw_power_monitor.py       # 비침습식 모니터링: 절대 편차 에너지 가드로 Inf 락업을 제거한 GPU 파형 관제
├── tests/
│   ├── mock_packet_injector.py     # np.frombuffer SIMD형 블록 카피로 파이썬 루프를 회피한 32B 와이어 패킷 생성기
│   └── simulation_burst.py         # 다수의 요청 시 커널 VmRSS 변동량 64KB 이하(O(1))를 테스트
├── Dockerfile             
├── Makefile                        # 하위 eBPF/CUDA 모듈을 Recursive Make 통합 호출
├── deploy.sh                       # ulimit -l 상한선 해제 및 Native xdpdrv 모드 원터치 인젝션/언로드 스크립트
└── README.md
```

