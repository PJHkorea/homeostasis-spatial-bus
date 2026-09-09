// =========================================================================
// Homeostasis Spatial Bus - Isolated Hardware Accelerator Build Script
// File: target_hardware_cuda/build.rs
// =========================================================================

use std::process::Command;

fn main() {
    // 1. 하드웨어 가속기 소스 및 인터페이스 ABI에 단 1비트의 수정이라도 발생하면 빌드 시스템 자동 동기화
    println!("cargo:rerun-if-changed=spatial_viscosity.cu");
    println!("cargo:rerun-if-changed=../target_kernel_xdp/spatial_maps.h");

    let out_dir = std::env::var("OUT_DIR").unwrap();
    let cuda_src = "spatial_viscosity.cu";
    let lib_out = format!("{}/libspatial_viscosity.a", out_dir);

    println!("⚡ [CUDA-BUILD] Isolated NVCC Hardware Transpilation 가동...");

    // 2. NVIDIA NVCC 컴파일러를 독점적으로 가동하여 SASS 기계어 레벨 압착
    // [-O3]: SM 레지스터 파이프라인 연산 배치를 전역 압착하여 무분기 질주 능력 극대화
    // [--use_fast_math]: 무거운 부동소수점 나눗셈 연산을 1클록 하드웨어 내장 기계어(rsqrtf 등)로 강제 캐스팅
    // [-Xcompiler=-fPIC]: Rust 사령탑 데몬 바이너리와 결합 시 메모리 주소 격리 UB 원천 분쇄
    // [--generate-code]: 엔터프라이즈 가속 인프라인 NVIDIA A100(sm_80) 및 H100(sm_90) 아키텍처 네이티브 바이너리 동시 합성
    let status = Command::new("nvcc")
        .args(&[
            "-O3",
            "--use_fast_math",
            "-Xcompiler=-fPIC",
            "--generate-code=arch=compute_80,code=sm_80",
            "--generate-code=arch=compute_90,code=sm_90",
            "-I../target_kernel_xdp",
            "-lib",
            cuda_src,
            "-o",
            &lib_out,
        ])
        .status()
        .expect("🚨 하드웨어 오류: NVIDIA NVCC 컴파일러를 실행할 수 없습니다! 드라이버 경로 확인 요망.");

    if !status.success() {
        panic!("🚨 CUDA 가속 커널 컴파일 실패. 하드웨어 SASS 빌드를 중단합니다.");
    }

    // 3. Rust Cargo 생태계가 이 모듈을 라이브러리로 인식할 수 있도록 정적 링킹 주소선 기부
    println!("cargo:rustc-link-search=native={}", out_dir);
    println!("cargo:rustc-link-lib=static=spatial_viscosity");

    // 4. 엔비디아 드라이버의 네이티브 포인터 탐색 경로 및 CUDA 런타임 공유 라이브러리(cudart) 링크 규칙 전사
    println!("cargo:rustc-link-search=native=/usr/local/cuda/lib64");
    println!("cargo:rustc-link-lib=dylib=cudart");
}
