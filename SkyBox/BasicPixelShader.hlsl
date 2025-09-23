#include "Shared.fxh"

float4 main(VShaderOut pIn) : SV_TARGET
{
    float3 N = normalize(pIn.Norm);

    // 텍스처 알베도(디퓨즈 텍스처)
    float3 albedo = txDiffuse.Sample(samLinear, pIn.Tex).rgb;

    // 램버트 조명 누적
    float3 lighting = 0;
    [unroll]
    for (int i = 0; i < 2; ++i)
    {
        // 보통 L은 "표면에서 광원 방향" 벡터
        float3 L = normalize(LightDir[i]);
        float NdotL = saturate(dot(N, L));
        lighting += NdotL * LightColor[i];
    }

    // 최종색 = 텍스처 * (직사광) + 텍스처 * (주변광)   (또는 주변광은 텍스처 없이 더해도 됨)
    float3 color = albedo * lighting;

    return float4(color, 1.0);
}

float4 PSSolid(VShaderOut pIn) : SV_Target
{
    return OutputColor;
}
