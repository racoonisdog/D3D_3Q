#include <Shared.fxh>

float4 main(VShaderOut input) : SV_TARGET
{    
    float shadowFactor = CalculateShadowFactor(input.WorldPos, gShadowMap, samLinear, ShadowView, ShadowProjection);
    
    //택스쳐의 색 ( 즉 그림 )을 계산하는 식
    float4 albedoSRGB = gTexture.Sample(samLinear, input.Tex);
    float3 albedo = pow(albedoSRGB.rgb, 2.2f);
    
    // 노말맵 텍스쳐를 uv좌표를 통해 샘플링
    float3 nTS = DecodeNormal(gNormal.Sample(samLinear, input.Tex).xyz);
    float3 nWS = normalize(nTS.x * input.Tan + nTS.y * input.BiTan + nTS.z * input.Norm);
    
    float3 normalColor = gNormal.Sample(samLinear, input.Tex).rgb;
    
    float3 emissionSRGB = gEmission.Sample(samLinear, input.Tex).rgb;
    float3 emission = pow(emissionSRGB, 2.2f);
    
    float opacity = albedoSRGB.a;
    
    float alpha = albedoSRGB.a;
    if (UseClip == 1)
        clip(alpha - clipValue);

    //조명 벡터
    float3 LightV = -normalize(LightDir.xyz);
    float3 Vector = normalize(camPos - input.WorldPos);
    float3 HalfV = normalize(LightV + Vector);
    
    float NdotL = saturate(dot(nWS, LightV));
    float NdotV = saturate(dot(nWS, Vector));
    float NdotH = saturate(dot(nWS, HalfV));
    float VdotH = saturate(dot(Vector, HalfV));
    
    
    float metallic;
    float roughness;
    
    float3 baseColor = albedo;
    
    if (useModelValue == 1)
    {
        metallic = gMetallic.Sample(samLinear, input.Tex).r * m_Metaliic;
        roughness = gRoughness.Sample(samLinear, input.Tex).r * m_Roughness;
        baseColor *= matdiffuse.rgb;
    }
    else
    {
        metallic = saturate(m_Metaliic);
        roughness = saturate(m_Roughness);
        baseColor = metalcolor;
    }
    
    roughness = saturate(roughness);
    float D_roughness = max(roughness, 0.0004f);

    
    
    // F0
    float3 dielectricF0 = float3(0.04f, 0.04f, 0.04f);
    float3 f0 = lerp(dielectricF0, baseColor, metallic);

    // direct lighting 결과 색
    float3 directLighting = 0.0f;
    float3 indirectIBL = 0.0f;
    
    const float PBRPI = 3.14159265359f;
    

    float D = DistributionGGX(NdotH, D_roughness);
    float3 F = FresnelSchlick(VdotH, f0);
    float G = GemoetrySmithForDirect(NdotL, NdotV, roughness);
    
    float3 F_spec = FresnelSchlick(VdotH, f0);
    float3 F_kd = FresnelSchlick(NdotV, f0);
    float3 kd_IBL = (1.0 - F_kd) * (1.0 - metallic);
    
    float3 kd = (1.0 - F) * (1.0 - metallic);  //lerp(1.0f.xxx - F, 0.0f.xxx, metallic);
    
    if (useIBLValue == 1)
    {
        float3 N = normalize(nWS);
        float3 V = normalize(camPos - input.WorldPos);
        float3 Lr = reflect(-V, N);
        
        float2 specularBRDF_IBL = BRDFLUT.Sample(samClamp, float2(NdotV, roughness)).rg;

        float3 irradiance = gIBLDiffuse.Sample(samLinear, N).rgb;

        float3 diffuseIBL = kd_IBL * baseColor * irradiance; //* PBRPI
    
        uint specularTextureLevels, width, height;
        gIBLSpecular.GetDimensions(0, width, height, specularTextureLevels);
    
        
        float maxMip = (float) (specularTextureLevels - 1);
        float lod = roughness * maxMip;
        float3 PrefilteredColor = gIBLSpecular.SampleLevel(samLinear, Lr, lod).rgb;
        //float3 PrefilteredColor = gIBLSpecular.SampleLevel(samLinear, Lr, roughness * specularTextureLevels).rgb;

        float3 F_ibl = FresnelSchlick(NdotV, f0);
        
        float3 specularIBL = PrefilteredColor * (F_ibl * specularBRDF_IBL.x + specularBRDF_IBL.y);
        indirectIBL = (diffuseIBL + specularIBL) * m_AmbientOcclusion;
    }
    else
    {
        indirectIBL = 0.0f;
    }     
    
    float3 specularBRDF = (D * F * G) / max(0.001f, 4.0f * NdotL * NdotV);
    float3 diffuseBRDF = kd * baseColor / PBRPI;
    float3 lightColor = LightColor.rgb;
        
    directLighting = (diffuseBRDF + specularBRDF) * lightColor * NdotL * shadowFactor;

    
     // ambient 있으면 + ambient
    float3 colorLinear = directLighting + indirectIBL + emission;

    //float3 colorSRGB = pow(saturate(colorLinear), 1.0f / 2.2f);
    float3 colorSRGB = pow(colorLinear, 1.0f / 2.2f);

    return float4(colorSRGB, alpha); // alpha = albedoSRGB.a
    
}