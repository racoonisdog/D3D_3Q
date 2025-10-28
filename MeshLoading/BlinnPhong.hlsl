#include <Shared.fxh>

float4 main(VShaderOut input) : SV_TARGET
{
    //택스쳐의 색 ( 즉 그림 )을 계산하는 식
    float4 albedo = gTexture.Sample(samLinear, input.Tex);
    
    // 노말맵의 색상을 벡터로 전환하는 함수 사용
    float3 nTS = DecodeNormal(gNormal.Sample(samLinear, input.Tex).xyz);

    float3 nWS = normalize(nTS.x * input.Tan + nTS.y * input.BiTan + nTS.z * input.Norm);

    //조명 벡터
    float3 LightV = -normalize(LightDir.xyz);
    float3 Vector = normalize(camPos - input.WorldPos);

    float DiffuseFactor = saturate(dot(nWS, LightV));
    float3 ambient = (matambient.rgb * lightambient.rgb) * albedo.rgb;

    float3 diffuse = 0;
    float3 specular = 0;
    
    if (DiffuseFactor > 0.0f)
    {
        float3 HalfVector = normalize(LightV + Vector);
        float NdotH = saturate(dot(nWS, HalfVector));
        float specP = pow(NdotH, shininess);

        diffuse = albedo.rgb * matdiffuse.rgb * lightdiffuse.rgb * DiffuseFactor;

        //스펙큘러 맵은 보통 회색조(스칼라) 한 채널만 사용하면되기 때문에 r 채널 사용
        //float ksTex = gSpecular.Sample(samLinear, input.Tex).r;
        float ksTex = 1.0f;
        specular = (matspecular.rgb * lightspecular.rgb) * (specP * ksTex);
    }
    float3 textureEmission = gEmission.Sample(samLinear, input.Tex).rgb;
    
    float alpha = albedo.a;
    if (UseClip) clip(alpha - 0.4f);
    
    float3 color = ambient + diffuse + specular + textureEmission;
    return float4(color, albedo.a);
}