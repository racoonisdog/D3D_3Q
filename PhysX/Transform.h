#pragma once
#include <d3d11.h>
#include <vector>
#include "../Common/GameApp.h"
#include <directxtk/SimpleMath.h>
#include <memory>
#include <utility>
#include <algorithm>


using namespace DirectX::SimpleMath;
using namespace std;

struct Transform
{
	Vector3 localPosition{ 0, 0, 0 };
	Quaternion localRotation{ 0, 0, 0 ,1.0f };
	Vector3 localSacle{ 1.0f, 1.0f, 1.0f };
	
	Matrix WorldMatrix() const
	{
		return Matrix::CreateScale(localSacle) *
			Matrix::CreateFromQuaternion(localRotation) *
			Matrix::CreateTranslation(localPosition);
	}
};

