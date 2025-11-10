#include "Animation.h"

void Animation::CreateFromAi(const aiAnimation* a)
{
    m_Name = a->mName.length ? a->mName.C_Str() : "Unnamed";
    m_TicksPerSecond = (a->mTicksPerSecond != 0.0) ? (double)a->mTicksPerSecond : 24.0;
    m_DurationTicks = (double)a->mDuration;

    m_Channels.clear();
    m_NameToChannel.clear();
    m_Channels.reserve(a->mNumChannels);
    for (unsigned i = 0; i < a->mNumChannels; ++i) {
        const aiNodeAnim* ch = a->mChannels[i];
        BoneAnimChannel out; out.BoneName = ch->mNodeName.C_Str();
        // BoneAnimChannel::CreateKeys가 내부적으로 변경되었지만, 호출은 동일
        out.CreateKeys(ch);
        m_NameToChannel[out.BoneName] = (int)m_Channels.size();
        m_Channels.push_back(std::move(out));
    }
}

const BoneAnimChannel* Animation::FindChannel(const std::string& boneName) const
{
    auto it = m_NameToChannel.find(boneName);
    return (it == m_NameToChannel.end()) ? nullptr : &m_Channels[it->second];
}

void Animation::MergeChannels(const Animation* other)
{
    for (const auto& channel : other->m_Channels)
    {
        // 중복되는 뼈 채널이 없다면 (다른 조각이므로 중복되면 안 됨)
        if (m_NameToChannel.find(channel.BoneName) == m_NameToChannel.end())
        {
            m_NameToChannel[channel.BoneName] = (int)m_Channels.size();
            m_Channels.push_back(channel); // channel.m_keys 복사 발생
        }
    }
}
