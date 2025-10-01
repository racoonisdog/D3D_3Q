// 정점 셰이더.
Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
	matrix worldinverseT;
	
    float4 LightDir;
    float4 LightColor;
    float4 OutputColor;
	
	float4 lightambient;
	float4 lightdiffuse;
	float4 lightspecular;
	float3 camPos;
	float shininess;
}

cbuffer MeshConstantBuffer : register(b1)
{
	float4 matambient;
	float4 matdiffuse;
	float4 matspecular;
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
	float3 WorldPos : TEXCOORD2;
};