#pragma once
#include "BoneInfo.h"
#include <map>


struct TempVertexBoneData {
	int BoneIndices[4] = { 0 };
	float BoneWeights[4] = { 0.0f };
	int Count = 0;
};

struct SkeletonInfo
{
	vector<BoneInfo> Bones;
	map<string, int> m_BoneMappingTable;
	map<string, int> m_MeshMappingTable;
	//vector<Matrix> FBXlist;
	//임시 저장소. 정점별 뼈 가중치(TempVertexBoneData)를 메쉬 인덱스별로 담고있음
	std::map<int, std::vector<TempVertexBoneData>> m_MeshBoneData;

    void CalculateGlobalInverseRoot();

	bool DebugValidateBindPoseOnce() const;
};