// 정점 셰이더.
Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
    float4 LightDir[2];
    float4 LightColor[2];
    float4 OutputColor;
}


struct VShaderIn
{
    float3 pos : POSITION;
    float3 Norm : NORMAL;
	float2 Tex : TEXCOORD0;
};

struct VShaderOut
{
    float4 pos : SV_POSITION;
    float3 Norm : TEXCOORD1;
	float2 Tex : TEXCOORD0;
};