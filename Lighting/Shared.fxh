// 정점 셰이더.

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
};

struct VShaderOut
{
    float4 pos : SV_POSITION;
    float3 Norm : TEXCOORD0;
};