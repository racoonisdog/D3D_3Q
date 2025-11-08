#include <shared.fxh>
// 정점 셰이더.
VShaderOut main(VShaderIn vIn)
{
    VShaderOut VerTOut;
	//mul 함수는 hlsl에서 제공하는 행렬/벡터 곱셈 함수
	//사용자 정의가 들어있는 shared.fxh에 world , view , projection 메트릭스와 정점의 포지션을 mul 함수로 곱하여 클립 공간으로 변환해줌
    VerTOut.pos = mul(float4(vIn.pos, 1.0f), world);		// 오브젝트 공간 -> 월드 공간으로 , 정점좌표를 Vector3으로 받기 때문에 동차좌표계로 변환(w는 1로)
    VerTOut.pos = mul(VerTOut.pos, view);                 // 월드 공간 -> 카메라 공간
    VerTOut.pos = mul(VerTOut.pos, projection);           // 카메라 공간 -> 클립 공간   
    
    float3x3 World3 = (float3x3) world;
    float3x3 Normal3 = (float3x3) worldinverseT;
	

	// 월드 노멀
    float3 Norworld = normalize(mul(vIn.Norm, Normal3));

    // 월드 탄젠트/비탄젠트
    float3 Tanworld = normalize(mul(vIn.Tan, Normal3));
    float3 Bitanworld = normalize(mul(vIn.BiTan, Normal3));

    // T를 N 평면에 정확히 올려놓기 위한 과정 (픽셀 보간/비균등 스케일로 틀어진 직교성 보정)
    Tanworld = normalize(Tanworld - Norworld * dot(Tanworld, Norworld));
    float s = (dot(cross(Norworld, Tanworld), Bitanworld) >= 0) ? 1.0 : -1.0; //s는 handedness 왼손, 오른손좌표계인지에 따라 - + 결정하도록 하는 용도
    Bitanworld = s * normalize(cross(Norworld, Tanworld));

    VerTOut.Norm = Norworld;
    VerTOut.Tan = Tanworld;
    VerTOut.BiTan = Bitanworld;
    VerTOut.Tex = vIn.Tex;
    VerTOut.WorldPos = mul(float4(vIn.pos, 1), world).xyz;

    return VerTOut;
}


VShaderOut BoneMain(BVSIn vIn)
{
    BVSOut VerTOut = (BVSOut) 0; // 출력 구조체 초기화

    // 스키닝 변환 행렬 S 계산 (가중치 합산)
    // S = Sum(Weight_i * BoneMatrix_i)
    float4x4 skinningMatrix = 0;
    float sumW = 0;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        uint bi = vIn.BoneIndices[i];
        float w = vIn.BoneWeights[i];
        sumW += w;
        if (w > 0)
            skinningMatrix += (gFinalBone[bi] * w);
    }

        // 가중치 합이 0이면 항등행렬로 폴백 (T포즈)
    if (sumW < 1e-6)
    {
        skinningMatrix = float4x4(
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        );
    }
    
    float4 animatedPos = mul(float4(vIn.pos, 1.0f), skinningMatrix);
// 2. 월드 공간으로 변환
// Animated Bone Space -> World Space
    float4 worldPos = mul(animatedPos, world);
    VerTOut.WorldPos = worldPos.xyz; // 픽셀 셰이더로 보낼 월드 위치 저장

// 3. 클립 공간으로 변환 (최종 화면 위치)
// World Space -> View/Projection Space -> Clip Space
    VerTOut.pos = mul(worldPos, view);
    VerTOut.pos = mul(VerTOut.pos, projection);


// 4. 노멀, 탄젠트, 비탄젠트 변환 (방향 벡터 처리)
// 방향 벡터는 변환 행렬의 회전 부분만 적용해야 합니다.
// (단, 스키닝 행렬은 이미 최종 변환을 포함하고 있으므로, 월드 행렬을 곱하기 전에 처리합니다.)

// A. 스키닝 변환 적용 (상단 3x3 회전 부분만 추출)
    float3x3 skinningMatrix3x3 = (float3x3) skinningMatrix;
    float3 animatedNormal = mul(vIn.Norm, skinningMatrix3x3);
    float3 animatedTangent = mul(vIn.Tan, skinningMatrix3x3);
    float3 animatedBiTangent = mul(vIn.BiTan, skinningMatrix3x3);

// B. 월드 공간으로 변환
// *참고: 노멀 벡터는 WorldInverseTranspose를 사용하지만,
//        여기서는 skinningMatrix3x3에 이미 애니메이션 변환이 적용되었으므로,
//        월드 행렬의 회전 부분(NormalMatrix)만 사용하는 것이 일반적입니다.
//        (님의 WorldInverseT 사용 로직을 유지하면서 정리합니다.)

    float3x3 NormalMatrix = (float3x3) worldinverseT; // 님의 기존 방식

    VerTOut.Norm = normalize(mul(animatedNormal, NormalMatrix));
    VerTOut.Tan = normalize(mul(animatedTangent, NormalMatrix));
    VerTOut.BiTan = normalize(mul(animatedBiTangent, NormalMatrix));


// 5. TBN 직교화 (선택적)
    VerTOut.Tan = normalize(VerTOut.Tan - VerTOut.Norm * dot(VerTOut.Tan, VerTOut.Norm));
    float s = (dot(cross(VerTOut.Norm, VerTOut.Tan), VerTOut.BiTan) >= 0) ? 1.0 : -1.0;
    VerTOut.BiTan = s * normalize(cross(VerTOut.Norm, VerTOut.Tan));


// 6. 기타 정보 설정
    VerTOut.Tex = vIn.Tex;

    return VerTOut;
}