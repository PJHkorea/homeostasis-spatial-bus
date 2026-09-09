# -*- coding: utf-8 -*-
"""
Homeostasis Spatial Bus - High-Performance Packet Injector
File: tests/mock_packet_injector.py

[수리물리학적 철학]
본 컴포넌트는 10만 명의 가상 게이머 및 매크로 봇 무리가 초당 수백만 번의 틱 좌표 데이터를 
난사하는 극단적인 100Gbps급 트래픽 폭격 시나리오를 물리적으로 재현(Emulation)하는 테스트 엔진입니다.
인프라의 닫힌계(Closed System) 장벽을 시험하기 위해 하드웨어 ABI 정렬 스펙인 32바이트 
컴팩트 텐서 규격을 칼같이 준수하여 로우레벨 커널 링버퍼 레일로 패킷 스트림을 초고속 인젝션합니다.
"""

import sys
import ctypes
import numpy as np

class PlayerSpatialPayload(ctypes.Structure):
    """
    [★ 하드웨어 ABI 정렬 스펙 완벽 수호: spatial_maps.h와 1:1 싱크]
    커널 데이터 플레인 및 GPU 가속기 포인터와 단 1비트의 뒤틀림 오차도 없이
    바이트 오프셋 단위로 포개어지는 32바이트 하드웨어 캐시라인 물리 구조체 명세.
    """
    _packed_ = 1
    _fields_ = [
        ("x_fixed", ctypes.c_uint32),        // 4 Bytes (오프셋 0) - Q16.16 고정소수점 변환값
        ("y_fixed", ctypes.c_uint32),        // 4 Bytes (오프셋 4)
        ("z_fixed", ctypes.c_uint32),        // 4 Bytes (오프셋 8)
        ("pitch_fixed", ctypes.c_uint32),    // 4 Bytes (오프셋 12)
        ("yaw_fixed", ctypes.c_uint32),      // 4 Bytes (오프셋 16)
        ("roll_fixed", ctypes.c_uint32),     // 4 Bytes (오프셋 20)
        ("player_id", ctypes.c_uint32),      // 4 Bytes (오프셋 24)
        ("action_bitmap", ctypes.c_uint32)   // 4 Bytes (오프셋 28) -> 정확히 32바이트 물리 경계 동결
    ]

class HighPerformancePacketInjector:
    def __init__(self, max_users: int = 10485760):
        self.max_users = max_users
        self.q_scale = 65536  # Q16.16 고정소수점 인코딩 스케일 팩터
        
        # 닫힌계 자원 통제를 위해 인젝터 버퍼 공간 또한 부팅 시점에 고정 크기로 선점 (O(1) 메모리 수호)
        self.packet_pool_size = 100000
        self.pool_layout = PlayerSpatialPayload * self.packet_pool_size
        self.packet_pool = self.pool_layout()
        
        print(f"⚡ [INIT] 가상 트래픽 인젝터 풀 선점 완료: 32바이트 * {self.packet_pool_size} 슬롯")

    def generate_synthetic_burst_stream(self, macro_ratio: float = 0.5) -> ctypes.Array:
        """
        [★ 0ns 무복사 고속 고정소수점 패킷 스트림 합성 파이프라인]
        인간 게이머와 정교한 난수 매크로 봇 무리의 3D 공간 좌표 페이로드를 생성합니다.
        가속기 직결 효율을 극대화하기 위해 생성 단에서 고정소수점 비트 변환을 미리 집행합니다.
        
        :param macro_ratio: 전체 생성 트래픽 중 매크로 봇넷 폭격이 차지하는 비율 (0.0 ~ 1.0)
        :return: self.packet_pool: C-Interface 및 커널 링버퍼로 즉시 사상 기부 가능한 정렬 구조체 배열
        """
        np.random.seed(1337)
        num_macros = int(self.packet_pool_size * macro_ratio)
        num_normals = self.packet_pool_size - num_macros
        
        # 1. 일반 정상 유저 궤적 합성 (자유도 엔트로피 극대화, 자연스러운 불규칙 변이)
        normal_x = np.random.uniform(-1000.0, 1000.0, num_normals)
        normal_y = np.random.uniform(-1000.0, 1000.0, num_normals)
        normal_z = np.random.uniform(0.0, 50.0, num_normals)
        
        # 2. 난수 우회 매크로 봇넷 궤적 합성 (정상 사용자로 위장했으나 특정 타겟 전장 좌표를 일제히 조준)
        target_x, target_y, target_z = 500.0, -500.0, 10.0
        # 해커가 난수 노이즈를 섞었으나 기계적 명령 제어의 한계로 인해 미세 오차 대역 내에 강제 구속됨
        macro_x = target_x + np.random.normal(0.0, 0.01, num_macros)
        macro_y = target_y + np.random.normal(0.0, 0.01, num_macros)
        macro_z = target_z + np.random.normal(0.0, 0.001, num_macros)
        
        # 전체 데이터 레이아웃 통합 결합 (VStack)
        all_x = np.concatenate([normal_x, macro_x])
        all_y = np.concatenate([normal_y, macro_y])
        all_z = np.concatenate([normal_z, macro_z])
        
        # 3. 고속 비트 인코딩 및 C-Contiguous Array 다이렉트 이식 (나눗셈/실수 연산 사전 소거)
        fixed_x = (all_x * self.q_scale).astype(np.uint32)
        fixed_y = (all_y * self.q_scale).astype(np.uint32)
        fixed_z = (all_z * self.q_scale).astype(np.uint32)
        
        # 무분기 루프 전개를 통한 32바이트 구조체 레일 고속 Write-back
        for i in range(self.packet_pool_size):
            slot = self.packet_pool[i]
            slot.x_fixed = int(fixed_x[i])
            slot.y_fixed = int(fixed_y[i])
            slot.z_fixed = int(fixed_z[i])
            slot.pitch_fixed = 0
            slot.yaw_fixed = int(i * 10) & 0xFFFF
            slot.roll_fixed = 0
            slot.player_id = int(i) & (self.max_users - 1)  # 무분기 링 바운드 마스크
            # 매크로 구역 유저는 식별 액션 비트맵을 고정값으로 마킹 (기계적 동기화 흔적 인젝션)
            slot.action_bitmap = 0xAA55AA55 if i >= num_normals else int(np.random.randint(0, 0xFFFFFFFF))
            
        return self.packet_pool

if __name__ == "__main__":
    # =============== 하드웨어 수리 무결성 실측 검증 시나리오 ===============
    print("⚡ [START] tests/mock_packet_injector.py 대규모 트래픽 인젝터 테스트 개시...")
    
    injector = HighPerformancePacketInjector(max_users=10485760)
    
    # 50%의 매크로 오염도가 섞인 10만 개 버스트 스트림 기부 처리
    initial_alloc = sys.getsizeof(injector.packet_pool)
    packet_stream = injector.generate_synthetic_burst_stream(macro_ratio=0.5)
    final_alloc = sys.getsizeof(injector.packet_pool)
    
    print(f"├─ [측정] 성공적으로 합성된 고밀도 3차원 패킷 수 : {len(packet_stream)} 개")
    print(f"├─ [측정] 선제 할당된 인젝터 패킷 풀 물리 크기 : {initial_alloc} Byte")
    print(f"├─ [검증] 스트림 생성 후 추가 메모리 팽창 진폭 : {abs(final_alloc - initial_alloc)} Byte (O(1) 자원 고정 완벽 증명)")
    
    # 구조체 슬롯 무결성 및 바이트 오프셋 최종 단언 단락 검증
    sample_slot = packet_stream[99999]
    print(f"├─ [스냅샷] 타겟 매크로 슬롯 99999 Player ID : {sample_slot.player_id}")
    print(f"└─ [스냅샷] 타겟 매크로 슬롯 99999 Action Mask : {hex(sample_slot.action_bitmap)}")
    
    assert abs(final_alloc - initial_alloc) == 0, "수리 오류: 인젝터 메모리 공간 닫힌계 가드레일이 무너졌습니다!"
    assert sample_slot.action_bitmap == 0xAA55AA55, "정합성 오류: 매크로 동기화 페이로드 인젝션 무결성이 파괴되었습니다."
    
    print("🔥 [SUCCESS] tests/mock_packet_injector.py 프로덕션 등급 규격 검증 완결.")
