// 정점 셰이더.
Texture2D gTexture : register(t0);
Texture2D gEmission : register(t1);
Texture2D gNormal : register(t2);
Texture2D gSpecular : register(t3);
Texture2D gToonRampTexture : register(t4);
Texture2D gQuantizedRampTexture : register(t5);

SamplerState samLinear : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
    matrix worldinverseT;
	
    float4 LightDir;
    float4 LightColor;
	
    float4 lightambient;
    float4 lightdiffuse;
    float4 lightspecular;
    float3 camPos;
    float shininess;
    
    float clipValue;
    float3 padding;
}

cbuffer MeshConstantBuffer : register(b1)
{
    float4 matambient;
    float4 matdiffuse;
    float4 matspecular;
    
    int UseClip;
    float3 padding2;
};

cbuffer FinalBoneMatrix : register(b2)
{
    matrix gFinalBone[128];
};

cbuffer OutLine : register(b3)
{
    int LineFlag;
    float OutlineThickness;
    float2 padding3;
    float4 LineColor;
};

struct VShaderIn
{
    float3 pos : POSITION;
	float2 Tex : TEXCOORD0;
    float3 Tan : TANGENT;
    float3 BiTan : BINORMAL;
    float3 Norm : NORMAL;
};

struct VShaderOut
{
    float4 pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
    float3 Tan : TANGENT;
    float3 BiTan : BINORMAL;
    float3 Norm : NORMAL;
	float3 WorldPos : TEXCOORD1;
};


struct BVSIn
{
    float3 pos : POSITION;
    float2 Tex : TEXCOORD0;
    float3 Tan : TANGENT;
    float3 BiTan : BINORMAL;
    float3 Norm : NORMAL;
    
    uint4 BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
};


struct BVSOut
{
    float4 pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
    float3 Tan : TANGENT;
    float3 BiTan : BINORMAL;
    float3 Norm : NORMAL;
    float3 WorldPos : TEXCOORD1;
};

float3 EncodeNormal(float3 N)
{
    return N * 0.5 + 0.5;
}

float3 DecodeNormal(float3 N)
{
    return N * 2 - 1;
}