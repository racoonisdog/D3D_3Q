#include <Shared.fxh>

float4 main(VShaderOut input) : SV_TARGET
{
    //텍스처 색상 (현재 흰색)
    float4 albedo = gTexture.Sample(samLinear, input.Tex);

    float shadowFactor = CalculateShadowFactor(input.WorldPos, gShadowMap, samLinear, ShadowView, ShadowProjection);
    
    //텍스처 색상에 재질 색상을 곱하여 반환
    float3 finalColor = albedo.rgb * matdiffuse.rgb * shadowFactor;

    // 최종값 리턴
    return float4(finalColor, albedo.a);
}
