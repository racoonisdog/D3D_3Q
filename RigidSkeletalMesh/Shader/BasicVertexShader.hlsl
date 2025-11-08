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
    matrix skinningMatrix = 0;
    
    for (int i = 0; i < 4; i++)
    {
        // 뼈 인덱스(vIn.BoneIndices[i])가 유효한지 확인 후 사용
        skinningMatrix += gFinalBone[vIn.BoneIndices[i]] * vIn.BoneWeights[i];
    }
    

    // 정점 위치 변환
    // 애니메이션/스키닝 적용: 오브젝트 공간 -> 월드 공간 (바인드 포즈)으로 변환된 위치
    float4 animatedPos = mul(float4(vIn.pos, 1.0f), skinningMatrix);
    
    // 월드/뷰/프로젝션 변환
    float4 worldPos = mul(animatedPos, world);
    //float4 worldPos = mul(float4(vIn.pos, 1.0f),skinningMatrix);
    VerTOut.pos = mul(worldPos, view);
    VerTOut.pos = mul(VerTOut.pos, projection);  
    
    // 노멀, 탄젠트, 비탄젠트 변환
    // 방향 벡터는 3x3 행렬과 곱해야 하며, worldinverseT 대신 skinningMatrix의 상단 3x3 부분을 사용
    float3x3 skinningMatrix3x3 = (float3x3) skinningMatrix;
    
    float3 animatedNormal = mul(vIn.Norm, skinningMatrix3x3);
    float3 animatedTangent = mul(vIn.Tan, skinningMatrix3x3);
    float3 animatedBiTangent = mul(vIn.BiTan, skinningMatrix3x3);
    
    //float3x3 WorldRotation = (float3x3) world;
    float3x3 NormalMatrix = (float3x3) worldinverseT;
    
    
    VerTOut.Norm = normalize(mul(animatedNormal, NormalMatrix));
    VerTOut.Tan = normalize(mul(animatedTangent, NormalMatrix));
    VerTOut.BiTan = normalize(mul(animatedBiTangent, NormalMatrix));
    
    // 3c. TBN 직교화 (선택적: 라이팅 정확도 향상)
    VerTOut.Tan = normalize(VerTOut.Tan - VerTOut.Norm * dot(VerTOut.Tan, VerTOut.Norm));
    float s = (dot(cross(VerTOut.Norm, VerTOut.Tan), VerTOut.BiTan) >= 0) ? 1.0 : -1.0;
    VerTOut.BiTan = s * normalize(cross(VerTOut.Norm, VerTOut.Tan));


    // 4. 기타 정보 설정
    VerTOut.WorldPos = worldPos.xyz;
    VerTOut.Tex = vIn.Tex;
    
    return VerTOut;
}