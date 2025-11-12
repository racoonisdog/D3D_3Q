#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <d3d11_1.h>
#include <directxtk/SimpleMath.h> // directXmath 대신 사용
#include <DirectXMath.h>


#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace std;

//윈도우 스마트 포인터용
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;


struct BoneInfo
{
	Matrix BindLocal;				//부모 기준 바인드포즈 변환(FBX에서 읽은 기본값)
	Matrix OffsetTransform;			// 스키닝용
	string Name;
	int ParentIndex;
};


//런타임 인스턴스
struct Bone
{
	Matrix LocalCurrent;		// 현재 프레임에서 부모 기준 로컬 변환 (애니메이션 결과)
	Matrix ModelCurrent;		// 부모까지 누적한 전역/모델 공간 변환
	string Name;					// 본의 이름
	string ParentBoneName;			// 부모 본의 이름
	int ParentIndex;				// 부모의 index
	int index;						// 현재 본의 index
};