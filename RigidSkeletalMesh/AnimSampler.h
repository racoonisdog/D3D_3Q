#pragma once
#include "Animation.h"

//보간함수
namespace AnimSampler {

    template<typename KeyT>
    inline int FindSpanLinear(const vector<KeyT>& keys, double t) {
        //키가 없다면 그냥 나오고
        if (keys.empty()) return -1;
        //키가 하나밖에 없다면 보간필요 X
        if (keys.size() == 1) return 0;
        int i = 0;
        while (i + 1 < (int)keys.size() && keys[i + 1].time <= t) ++i;

        //인덱스 초과 방지(size-1가 마지막 인덱스니 size() -2)
        return min(i, (int)keys.size() - 2);
    }

    //넘겨받은 시간에 맞는 T R S를 담아주는 함수
    inline void SampleTRS(const Animation& clip, const BoneAnimChannel* ch, double timeSec,
        Vector3& outT,
        Quaternion& outR,
        Vector3& outS)
    {
        if (!ch) { outT = Vector3::Zero; outR = Quaternion::Identity; outS = Vector3::One; return; }

        const double tAll = timeSec * (clip.GetTicksPerSecond() > 0.0 ? clip.GetTicksPerSecond() : 24.0);
        double t = (clip.GetDurationTicks() > 0.0) ? fmod(tAll, clip.GetDurationTicks()) : tAll;
        if (t < 0.0) t += clip.GetDurationTicks();

        // T
        if (ch->TKeys.empty()) outT = Vector3::Zero;
        else if (ch->TKeys.size() == 1) outT = ch->TKeys[0].v;
        else {
            int i = FindSpanLinear(ch->TKeys, t);
            const auto& k0 = ch->TKeys[i]; const auto& k1 = ch->TKeys[i + 1];
            float a = (k1.time > k0.time) ? float((t - k0.time) / (k1.time - k0.time)) : 0.0f;
            outT = k0.v * (1.0f - a) + k1.v * a;
        }

        // S
        if (ch->SKeys.empty()) outS = Vector3::One;
        else if (ch->SKeys.size() == 1) outS = ch->SKeys[0].v;
        else {
            int i = FindSpanLinear(ch->SKeys, t);
            const auto& k0 = ch->SKeys[i]; const auto& k1 = ch->SKeys[i + 1];
            float a = (k1.time > k0.time) ? float((t - k0.time) / (k1.time - k0.time)) : 0.0f;
            outS = k0.v * (1.0f - a) + k1.v * a;
        }

        // R
        if (ch->RKeys.empty()) outR = Quaternion::Identity;
        else if (ch->RKeys.size() == 1) outR = ch->RKeys[0].q;
        else {
            int i = FindSpanLinear(ch->RKeys, t);
            const auto& k0 = ch->RKeys[i]; const auto& k1 = ch->RKeys[i + 1];
            float a = (k1.time > k0.time) ? float((t - k0.time) / (k1.time - k0.time)) : 0.0f;
            outR = Quaternion::Slerp(k0.q, k1.q, a);
        }
    }
}
