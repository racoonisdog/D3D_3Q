// 정점 셰이더.
Texture2D gTexture : register(t0);
Texture2D gEmission : register(t1);
Texture2D gNormal : register(t2);
Texture2D gSpecular : register(t3);
Texture2D gShadowMap : register(t10);

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

cbuffer TransformVP : register(b3)
{
    matrix ShadowView;
    matrix ShadowProjection;
}

cbuffer PsValue : register(b4)
{
    int ps_value;
    float3 padding3;
}

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
    float3 WorldPos : Position;
    float4 PositionShadow : TEXCOORD1;
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
    float3 BiTan : BITANGENT;
    float3 Norm : NORMAL;
    float3 WorldPos : Position;
    float4 PositionShadow : TEXCOORD1;
};


float3 EncodeNormal(float3 N)
{
    return N * 0.5 + 0.5;
}

float3 DecodeNormal(float3 N)
{
    return N * 2 - 1;
}


float CalculateShadowFactor(float3 worldpos, Texture2D shadowmap, SamplerState samplestate, float4x4 shadowV, float4x4 shadowP)
{
    float4 LightClip = mul(float4(worldpos, 1.0), shadowV);
    LightClip = mul(LightClip, shadowP);
    
    //w로 나눠서 ndc 좌표로 변환해주기
    float3 LightNDC = LightClip.xyz / LightClip.w;

    // 현재 픽셀의 빛 시점 깊이 (비교 대상)
    float currentDepth = LightNDC.z;
    
    float2 shadowTexCoord = EncodeNormal(LightNDC);
    //DirectX는 반전해줘야함
    shadowTexCoord.y = 1.0f - shadowTexCoord.y;
    // 텍스처에서 기록된 깊이 읽기
    float recordedDepth = shadowmap.Sample(samplestate, shadowTexCoord).r;

    //깊이 편차 (Bias) 적용 (그림자 아티팩트 방지)
    float bias = 0.005f;
    
    //현재 깊이가 기록된 깊이보다 크면 (더 뒤에 있으면) 그림자
    float shadowFactor = 1.0f;

    if (currentDepth > recordedDepth + bias)
    {
        shadowFactor = 0.0f; // 그림자 영역 (직접광 차단)
    }

    return shadowFactor;
}