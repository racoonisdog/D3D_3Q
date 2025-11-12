#pragma once
#include "BoneInfo.h"
#include <map>
#include <set>
#include <unordered_map>

struct AnimationKey {
    double m_time;
    Vector3 m_position;
    Quaternion m_rotation;
    Vector3 m_scale;
};

struct BoneAnimChannel {
    string BoneName;
    // TKeys, RKeys, SKeys 대신 하나의 통합 vector 사용
    vector<AnimationKey> m_keys;

    // aiNodeAnim에서 원본 키만 복사 (로직 변경)
    void CreateKeys(const aiNodeAnim* ch) {
        // [주의] 이 로직은 mNumPositionKeys, mNumRotationKeys, mNumScalingKeys가 
        // 모두 동일하다는 가정을 전제로 합니다.

        // 가장 작은 키 개수를 찾아 혹시 모를 배열 인덱스 오류를 방지 (안전 장치 추가)
        unsigned keyNum = ch->mNumPositionKeys;
        if (ch->mNumRotationKeys < keyNum) keyNum = ch->mNumRotationKeys;
        if (ch->mNumScalingKeys < keyNum) keyNum = ch->mNumScalingKeys;

        m_keys.reserve(keyNum);

        for (unsigned i = 0; i < keyNum; ++i) {
            AnimationKey key;

            // 1. 시간 (Time)
            // 위치 키의 시간을 기준으로 사용
            key.m_time = (double)ch->mPositionKeys[i].mTime;

            // 2. 위치 (Translation)
            const auto& p = ch->mPositionKeys[i].mValue;
            key.m_position = Vector3((float)p.x, (float)p.y, (float)p.z);

            // 3. 회전 (Rotation)
            const auto& r = ch->mRotationKeys[i].mValue;
            key.m_rotation = Quaternion((float)r.x, (float)r.y, (float)r.z, (float)r.w);

            // 4. 크기 (Scale)
            const auto& s = ch->mScalingKeys[i].mValue;
            key.m_scale = Vector3((float)s.x, (float)s.y, (float)s.z);

            m_keys.push_back(key);
        }
    }

    // 이 함수를 애니메이션 컨트롤러에서 사용할 것입니다.
    // float perTick 변환은 Animation 클래스의 '재생 로직'에서 처리하는 것이 더 일반적입니다.
    // 여기서는 로딩만 담당하고, 재생 시점에 'm_TicksPerSecond'로 나눠서 시간을 변환합니다.
};


class Animation
{
private:
    string m_Name;
    double m_DurationTicks = 0.0;       // 애니메이션 총 길이, aiAnim->mDuration로 접근
    double m_TicksPerSecond = 24.0;     // 틱/초 (애니메이션 FPS), aiAnim->mTicksPerSecond!=0 ? 틱값 : 24(기본값 24로)

    // 이 클립에 포함된 모든 뼈의 채널 (애니메이션 데이터)
    // 특정 뼈의 채널을 이름으로 찾아서 인덱스반환
    std::unordered_map<string, int> m_NameToChannel; // BoneName -> channel index


public:
    vector<BoneAnimChannel> m_Channels;
    // 이 클립을 로드하는 함수 (ModelLoader에서 호출됨)
    void CreateFromAi(const aiAnimation* a);

    // 채널 조회, 컨트롤러에서 계산용 FIND 함수
    const BoneAnimChannel* FindChannel(const std::string& boneName) const;

    double GetTicksPerSecond() const { return m_TicksPerSecond; }
    double GetDurationTicks()  const { return m_DurationTicks; }
    string& GetName() { return m_Name; }
    
    void SetTicksPerSecond(double TicksPerSecond) { m_TicksPerSecond = TicksPerSecond; }
    void SetDurationTicks(double Duration) { m_DurationTicks = Duration; }

    void MergeChannels(const Animation* other);
};