#include <Shared_D.fxh>

G_bufferOut main(VShaderOut input) : SV_TARGET
{
    G_bufferOut G_Out;
    
    float2 uv = input.Tex;
        
    //머티리얼 텍스처 샘플
    float3 albedoSRGB = gTexture.Sample(samLinear, uv).rgb;
    float3 baseColor = pow(albedoSRGB, 2.2f);

    float3 Tan = normalize(input.Tan);
    float3 Bitan = normalize(input.BiTan);
    float3 Norm = normalize(input.Norm);
    
    float3x3 TBN = float3x3(Tan, Bitan, Norm);
    
    float3 nTS = DecodeNormal(gNormal.Sample(samLinear, uv).xyz);

    float3 nWS = normalize(mul(nTS, TBN));
    
    
    //float metallic = gMetallic.Sample(samLinear, uv).r;
    //float roughness = gRoughness.Sample(samLinear, uv).r;
    
    float metallic = 0.0f;
    float roughness = 0.0f;
    
    if (useModelValue == 1)
    {
        metallic = gMetallic.Sample(samLinear, input.Tex).r;// * m_Metaliic;
        roughness = gRoughness.Sample(samLinear, input.Tex).r;// * m_Roughness;
        
        float3 matDiffuseLinear = pow(matdiffuse.rgb, 2.2f);
        baseColor *= matDiffuseLinear;
    }
    else
    {
        metallic = saturate(m_Metaliic);
        roughness = saturate(m_Roughness);
        baseColor = metalcolor;
    }

    float3 emission = gEmission.Sample(samLinear, uv).rgb;
    
    float3 worldpos = input.WorldPos;
    
    //GBuffer에 쓸 값 구성
    G_Out.G_BaseColor = float4(baseColor, 1.0f);
    G_Out.G_Normal = float4(EncodeNormal(nWS), 1.0f);
    G_Out.G_Metallic = float4 (metallic, 0.0f, 0.0f, 1.0f);
    G_Out.G_Roughness = float4(roughness, 0.0f, 0.0f, 1.0f);
    G_Out.G_Emission = float4(emission, 1.0f);
    G_Out.G_Position = float4(worldpos, 1.0f);
    
    return G_Out;
}

float4 DeferredLight(VSFullScreenOut input) : SV_TARGET
{
    float2 uv = input.UV;

    float3 worldPos = D_WorldPosition.Sample(samLinear, uv).xyz;

    float3 nEnc = D_WorldNormal.Sample(samLinear, uv).xyz;
    float3 nWS = normalize(DecodeNormal(nEnc));
    
    float shadowFactor = CalculateShadowFactor(worldPos, gShadowMap, samLinear, ShadowView, ShadowProjection);

    float metallic = D_Metallic.Sample(samLinear, uv).r;
    float roughness = D_Roughness.Sample(samLinear, uv).r;
    float4 baseData = D_BaseColor.Sample(samLinear, uv);
    float3 baseColor = baseData.rgb;
    float alpha = baseData.a;
    float3 emission = D_Emission.Sample(samLinear, uv).rgb;

    metallic = saturate(metallic);
    roughness = saturate(roughness);
    
    //
    float3 LightV = -normalize(LightDir.xyz);
    float3 Vector = normalize(camPos - worldPos);
    float3 HalfV = normalize(LightV + Vector);

    float NdotL = saturate(dot(nWS, LightV));
    float NdotV = saturate(dot(nWS, Vector));
    float NdotH = saturate(dot(nWS, HalfV));
    float VdotH = saturate(dot(Vector, HalfV));

    float3 dielectricF0v = float3(0.04f, 0.04f, 0.04f);
    float3 f0 = lerp(dielectricF0v, baseColor, metallic);

    roughness = saturate(roughness);
    float D_roughness = max(roughness, 0.02f);

    float3 directLighting = 0.0f;
    float3 indirectIBL = 0.0f;

    const float PBRPI = 3.14159265359f;

    float D = DistributionGGX(NdotH, D_roughness);
    float3 F = FresnelSchlick(VdotH, f0);
    float G = GemoetrySmithForDirect(NdotL, NdotV, roughness);

    float3 F_spec = FresnelSchlick(VdotH, f0);
    float3 F_kd = FresnelSchlick(NdotV, f0);
    float3 kd_IBL = (1.0 - F_kd) * (1.0 - metallic);
    float3 kd = (1.0 - F) * (1.0 - metallic);

    if (useIBLValue == 1)
    {
        float3 N = normalize(nWS);
        float3 V = normalize(camPos - worldPos);
        float3 Lr = reflect(-V, N);

        float2 specularBRDF_IBL = BRDFLUT.Sample(samClamp, float2(NdotV, roughness)).rg;

        float3 irradiance = gIBLDiffuse.Sample(samLinear, N).rgb;
        float3 diffuseIBL = kd_IBL * baseColor * irradiance;

        uint specularTextureLevels, width, height;
        gIBLSpecular.GetDimensions(0, width, height, specularTextureLevels);

        float maxMip = (float) (specularTextureLevels - 1);
        float lod = roughness * maxMip;
        float3 PrefilteredColor = gIBLSpecular.SampleLevel(samLinear, Lr, lod).rgb;

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
    float3 lightColor = LightColor.rgb * LightIntensity;

    directLighting = (diffuseBRDF + specularBRDF) * lightColor * NdotL * shadowFactor;

    float3 colorLinear = directLighting + indirectIBL + emission;

    return float4(colorLinear, alpha);
    //return float4(baseColor, 1.0f);
}


float4 LDR_ps(PS_INPUT_QUAD input) : SV_TARGET
{
    float3 hdr = gTonemap.Sample(samLinear, input.uv).rgb;
    
    float ExposureScale = pow(2.0f, Exposure);
    hdr *= ExposureScale;

    float3 mapped = ACESFilm(hdr);
    
    mapped = pow(mapped, 1.0f / Gamma);

    return float4(mapped, 1.0f);
}


float4 HDR_ps(PS_INPUT_QUAD input) : SV_TARGET
{
    float3 linear709 = gTonemap.Sample(samLinear, input.uv).rgb;
    float exposureScale = pow(2.0f, Exposure);
    float3 exposure = linear709 * exposureScale;
    
    //float3 tonemapped = tonemapped = ACESFilm(hdr);;

    float3 tonemapped = ACESFilm(exposure);
    
    //tonemapped = max(tonemapped, 0.0f.xxx);
    
    
    float3 c2020 = Rec709ToRec2020(tonemapped);
    
    const float ST2084MAX = 10000.0;
    float hdrScalar = gMaxHDRNits / ST2084MAX;
    
    
    float3 ST2084 = LinearToST2084(c2020 * hdrScalar);
    
    return float4(ST2084, 1.0);

}