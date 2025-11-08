#pragma once
#include "BoneInfo.h"
#include <map>
#include <set>
#include <unordered_map>

struct VecKey {
    double time;
    Vector3 v;
};
struct RotKey {
    double time;
    Quaternion q;
};

struct BoneAnimChannel {
    string BoneName;
    vector<VecKey> TKeys;  // position keys
    vector<RotKey> RKeys;  // rotation keys
    vector<VecKey> SKeys;  // scale keys

    // aiNodeAnim에서 원본 키만 복사
    void CreateKeys(const aiNodeAnim* ch) {
        TKeys.reserve(ch->mNumPositionKeys);
        for (unsigned i = 0; i < ch->mNumPositionKeys; ++i) {
            const auto& k = ch->mPositionKeys[i];
            TKeys.push_back({ (double)k.mTime, Vector3((float)k.mValue.x,(float)k.mValue.y,(float)k.mValue.z) });
        }
        RKeys.reserve(ch->mNumRotationKeys);
        for (unsigned i = 0; i < ch->mNumRotationKeys; ++i) {
            const auto& k = ch->mRotationKeys[i];
            RKeys.push_back({ (double)k.mTime, Quaternion((float)k.mValue.x,(float)k.mValue.y,(float)k.mValue.z,(float)k.mValue.w) });
        }
        SKeys.reserve(ch->mNumScalingKeys);
        for (unsigned i = 0; i < ch->mNumScalingKeys; ++i) {
            const auto& k = ch->mScalingKeys[i];
            SKeys.push_back({ (double)k.mTime, Vector3((float)k.mValue.x,(float)k.mValue.y,(float)k.mValue.z) });
        }
    }
};

// 전체 애니메이션 관리 클래스
class Animation
{
private:
    string m_Name;
    double m_DurationTicks = 0.0;       // 애니메이션 총 길이, aiAnim->mDuration로 접근
    double m_TicksPerSecond = 24.0;     // 틱/초 (애니메이션 FPS), aiAnim->mTicksPerSecond!=0 ? 틱값 : 24(기본값 24로)

    // 이 클립에 포함된 모든 뼈의 채널 (애니메이션 데이터)
    vector<BoneAnimChannel> m_Channels;
    // 특정 뼈의 채널을 이름으로 찾아서 인덱스반환
    std::unordered_map<string, int> m_NameToChannel; // BoneName -> channel index


public:
    // 이 클립을 로드하는 함수 (ModelLoader에서 호출됨)
    //로드 함수
    void CreateFromAi(const aiAnimation* a);

    // 채널 조회, 컨트롤러에서 계산용 FIND 함수
    const BoneAnimChannel* FindChannel(const std::string& boneName) const;

    double GetTicksPerSecond() const { return m_TicksPerSecond; }
    double GetDurationTicks()  const { return m_DurationTicks; }
    const std::string& GetName() const { return m_Name; }
};



