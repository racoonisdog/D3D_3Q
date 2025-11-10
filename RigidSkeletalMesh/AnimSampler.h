#pragma once
#include "Animation.h"

//보간함수
namespace AnimSampler {

    // 보간 비율을 0.0f에서 1.0f 사이로 제한
    template <typename T>
    T Clamp(T value, T min, T max)
    {
        return (value < min) ? min : (value > max) ? max : value;
    }

    // ********** 통합 저장 방식에 맞춘 SampleTRS 구현 **********
    void SampleTRS(
        const Animation& clip,
        const BoneAnimChannel* ch,
        double timeSec,
        Vector3& outPosition,
        Quaternion& outRotation,
        Vector3& outScale)
    {
        if (ch->m_keys.size() < 2)
        {
            // 키가 1개 이하일 경우 (바인드 포즈이거나 애니메이션 없음)
            if (ch->m_keys.size() == 1)
            {
                outPosition = ch->m_keys[0].m_position;
                outRotation = ch->m_keys[0].m_rotation;
                outScale = ch->m_keys[0].m_scale;
            }
            else
            {
                // 채널이 존재해도 키가 없으면 기본값 (0, 0, 0) (이 경우 Update에서 바인드 포즈를 사용할 가능성이 높음)
                outPosition = Vector3::Zero;
                outRotation = Quaternion::Identity;
                outScale = Vector3::One;
            }
            return;
        }

        // 1. 현재 시간을 틱 단위로 변환
        // Update 함수에서 timeSec는 이미 초(second) 단위로 전달된다고 가정합니다.
        // Assimp의 키는 틱 단위이므로, 틱으로 되돌립니다.
        double timeTick = timeSec * clip.GetTicksPerSecond();

        // 애니메이션 길이만큼 시간이 반복되도록 모듈로 연산
        timeTick = fmod(timeTick, clip.GetDurationTicks());

        // 2. 키프레임 쌍 탐색 (하나의 키 목록만 탐색)
        size_t index = 0;
        // 다음 키의 시간이 현재 틱보다 작거나 같으면 인덱스 전진
        while (index + 1 < ch->m_keys.size() && timeTick >= ch->m_keys[index + 1].m_time)
        {
            index++;
        }

        // 3. 보간 수행
        const auto& keyA = ch->m_keys[index];       // 시작 키
        const auto& keyB = ch->m_keys[index + 1];     // 종료 키

        double durationTick = keyB.m_time - keyA.m_time;

        // 보간 비율 t 계산
        float t = 0.0f;
        if (durationTick > 0.0)
        {
            t = (float)((timeTick - keyA.m_time) / durationTick);
            t = Clamp(t, 0.0f, 1.0f);
        }
        // else: durationTick이 0이면 (두 키의 시간이 같으면), t는 0.0f 유지 -> keyA 값 사용

        // 4. TRS 보간
        outPosition = Vector3::Lerp(keyA.m_position, keyB.m_position, t);
        outRotation = Quaternion::Slerp(keyA.m_rotation, keyB.m_rotation, t);
        outScale = Vector3::Lerp(keyA.m_scale, keyB.m_scale, t);
    }
}
