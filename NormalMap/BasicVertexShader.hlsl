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