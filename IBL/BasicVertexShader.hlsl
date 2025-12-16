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
    static const uint BoneCount = 128;
    float4x4 skinningMatrix = 0;
    float sumW = 0;

    
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        sumW += vIn.BoneWeights[i];
    }
    
    
    float invSumW;
    if (sumW > 1e-6)
    {
        invSumW = 1.0f / sumW;
    }
    else
    {
        invSumW = 0.0f;
    }
    
    [unroll]
    for (int j = 0; j < 4; ++j)
    {
        uint boneindex = vIn.BoneIndices[j];
        float weights = vIn.BoneWeights[j];
        
        // 정규화된 가중치 (weights * invSumW)를 사용
        if (weights > 1e-6 && boneindex < BoneCount)
        {
            skinningMatrix += (gFinalBone[boneindex] * weights * invSumW);
        }
    }

    // 예외 처리: 만약 모든 가중치가 0에 가까우면 Identity 행렬 사용
    if (invSumW == 0.0f)
    {
        skinningMatrix = float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    }
    

    float4 localPos = float4(vIn.pos, 1.0);
    float4 skinned = mul(localPos, skinningMatrix); //  mul(벡터, 행렬)로 복구 (원래 틀린 순서)
// (현재 코드는 mul(skinningMatrix, localPos)였음. mul(벡터, 행렬)이 올바른 순서임.)


// 3) 월드/뷰/프로젝션 변환 (모두 mul(벡터, 행렬)로 통일)
// world, view, projection이 M 상태로 CPU에서 넘어왔다면 mul(벡터, 행렬)이 올바름.
    float4 worldPos = mul(skinned, world); //  mul(벡터, 행렬) 유지
    VerTOut.WorldPos = worldPos.xyz;

    float4 viewPos = mul(worldPos, view); //  mul(벡터, 행렬) 유지
    VerTOut.pos = mul(viewPos, projection);

    // 4) 노멀/탄젠트/비탄젠트 스키닝 (3x3도 통일)
    float3x3 skinningMatrix3x3 = (float3x3) skinningMatrix;

    float3 nL = mul(vIn.Norm, skinningMatrix3x3); // 순서 변경: mul(벡터, 행렬)
    float3 tL = mul(vIn.Tan, skinningMatrix3x3); // 순서 변경: mul(벡터, 행렬)
    float3 bL = mul(vIn.BiTan, skinningMatrix3x3); // 순서 변경: mul(벡터, 행렬)

    // 월드 노멀 변환
    float3x3 Nmat = (float3x3) worldinverseT;

    float3 nW = normalize(mul(nL, Nmat)); // 순서 변경: mul(벡터, 행렬)
    float3 tW = normalize(mul(tL, Nmat)); // 순서 변경: mul(벡터, 행렬)
    float3 bW = normalize(mul(bL, Nmat)); // 순서 변경: mul(벡터, 행렬)
    // -----------------------------------------------------------------

    // 5) TBN 직교화(선택)
    tW = normalize(tW - nW * dot(tW, nW));
    float s; // 삼항 연산자를 if-else로 대체하여 인코딩 오류 방지
    if (dot(cross(nW, tW), bW) >= 0.0)
    {
        s = 1.0f;
    }
    else
    {
        s = -1.0f;
    }
    bW = s * normalize(cross(nW, tW));

    VerTOut.Norm = nW;
    VerTOut.Tan = tW;
    VerTOut.BiTan = bW;

    // 6) UV
    VerTOut.Tex = vIn.Tex;

    return VerTOut;
}


VShaderOut Shadow(BVSIn vIn)
{
    BVSOut VerTOut = (BVSOut) 0; // 출력 구조체 초기화
    
    static const uint BoneCount = 128;
    float4x4 skinningMatrix = 0;
    float sumW = 0;

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        sumW += vIn.BoneWeights[i];
    }
    
    float invSumW;
    if (sumW > 1e-6)
    {
        invSumW = 1.0f / sumW;
    }
    else
    {
        invSumW = 0.0f;
    }
    
    [unroll]
    for (int k = 0; k < 4; ++k)
    {
        uint boneindex = vIn.BoneIndices[k];
        float weights = vIn.BoneWeights[k];
        
        if (weights > 1e-6 && boneindex < BoneCount)
        {
            // gFinalBone은 상수 버퍼에서 가져온 최종 뼈대 행렬
            skinningMatrix += (gFinalBone[boneindex] * weights * invSumW);
        }
    }

    if (invSumW == 0.0f)
    {
        skinningMatrix = float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    }

    float4 localPos = float4(vIn.pos, 1.0);
    float4 skinned = mul(localPos, skinningMatrix);
    
    
    float4 worldPos = mul(skinned, world);
    VerTOut.pos = mul(mul(worldPos, ShadowView), ShadowProjection);
    
    return VerTOut;
}

VShaderOut Shadowmain(VShaderIn vIn)
{
    VShaderOut vOut;
    
    //World 변환 (오브젝트 공간 -> 월드 공간)
    float4 worldPos = mul(float4(vIn.pos, 1.0f), world);


    float4 lightViewPos = mul(worldPos, ShadowView);
    vOut.pos = mul(lightViewPos, ShadowProjection);
    
    //노멀, 탄젠트, 텍스처 좌표 등은 깊이 맵에는 필요 없으므로 계산안함.

    return vOut;
}