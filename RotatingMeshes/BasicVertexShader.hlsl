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
    VerTOut.color = vIn.color;

    return VerTOut;
}