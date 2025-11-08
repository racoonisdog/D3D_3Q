TextureCube txDiffuse : register(t0);
SamplerState samLinear : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
}


struct VShaderIn
{
    float3 pos : POSITION;
};

struct VShaderOut
{
    float4 pos : SV_POSITION;
	float3 dir : TEXCOORD0;
};

float4 main(VShaderOut pIn) : SV_TARGET
{
    return float4(txDiffuse.Sample(samLinear, normalize(pIn.dir)).rgb, 1.0);
}