#pragma once
#include "Animation.h"
#include "SkeletonInfo.h"
#include "ModelLoader.h"

class AnimationController
{
private:
	const SkeletonInfo* m_skel = nullptr;
	const std::map<std::string, Animation*>* m_allClips = nullptr;
	const Animation* m_clip = nullptr;
	double m_timeSec = 0.0;
	vector<Matrix> m_LBT, m_GBT, m_Final;
	string m_currentClipName;

public:
	void Initialize(const SkeletonInfo* skel) {
		m_skel = skel;
		m_timeSec = 0.0;
		m_LBT.resize(m_skel->Bones.size());
		m_GBT.resize(m_skel->Bones.size());
		m_Final.resize(m_skel->Bones.size());
	}

	void SetClip(const Animation* clip) { m_clip = clip; m_timeSec = 0.0; }

	void SetClip(const Animation* clip, double m_currentSec ) { m_clip = clip; m_timeSec = m_currentSec; }

	const string& CurrentClipName() const { return m_currentClipName; }

	void SetClipTable(const std::map<std::string, Animation*>* table) {
		m_allClips = table;
	}

	bool SetClipByName(const std::string& name, double startSec = 0.0) {
		if (!m_allClips) return false;
		auto it = m_allClips->find(name);
		if (it == m_allClips->end()) return false;
		m_clip = it->second;            // 현재 재생 클립
		m_currentClipName = name;
		m_timeSec = startSec;
		return true;
	}

	void Update(double dtSec);                    // Final 계산
	void GetFinalMatrices(Matrix out[128]) const;
};

