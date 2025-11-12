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

VShaderOut main(VShaderIn vIn)
{
    VShaderOut VerTOut;

    float4 pW = mul(float4(vIn.pos, 1.0f), world);
    float4 pV = mul(pW, view);
    float4 pH = mul(pV, projection);
	VerTOut.pos = pH.xyww;  //xyww -> float4(pH.xy, pH.w, pH.w 
	
	VerTOut.dir = vIn.pos;
	
    return VerTOut;
}