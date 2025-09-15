// 정점 셰이더.

cbuffer ConstantBuffer : register(b0)
{
	//CPU쪽에서 넘겨주는 변수 이름과 동일해야함
    matrix world;
    matrix view;
    matrix projection;
}


struct VShaderIn
{
	//(R, G, B, A)를 쓴다면 float4 , 텍스처 좌표(UV 좌표, U축, V축 정보)를 쓴다면 float2
    float3 pos : POSITION;
    float4 color : COLOR;
};

struct VShaderOut
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};