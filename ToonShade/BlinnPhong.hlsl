#include <Shared.fxh>

float4 main(VShaderOut input) : SV_TARGET
{
    if (LineFlag == 2)
    {
        return LineColor;
    }
    else
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
            float3 rampColor = gToonRampTexture.Sample(samLinear, float2(DiffuseFactor, 0.5f)).rgb;

        // 기존 DiffuseFactor 대신 램프 텍스처에서 읽어온 rampColor를 곱함
            diffuse = albedo.rgb * matdiffuse.rgb * lightdiffuse.rgb * rampColor;
        
        // ----------------------------------------------------
        // 양자화된 스페큘러 (Quantized Specular) 적용
            float specP = pow(NdotH, shininess); // 하이라이트 크기 조절
            //float specP = pow(NdotH, 100.0f);

        // specP를 램프 텍스처의 U 좌표로 사용하여 단계화된 하이라이트 색상을 얻음
            float3 specRampColor = gQuantizedRampTexture.Sample(samLinear, float2(specP, 0.5f)).rgb;
        
        // 스페큘러 맵 사용: gSpecular.Sample(samLinear, input.Tex).r (주석 해제)
        // 아니면 1.0f 유지
            float ksTex = gSpecular.Sample(samLinear, input.Tex).r; // Specular 맵 사용으로 가정
        
        // 최종 스페큘러 계산 시 specP 대신 램프 텍스처에서 읽어온 색상/강도를 곱함
            specular = (matspecular.rgb * lightspecular.rgb) * (specRampColor * ksTex);
        }
    
        float3 textureEmission = gEmission.Sample(samLinear, input.Tex).rgb;

        float alpha = albedo.a;
        if (UseClip == 1)
            clip(alpha - clipValue);

        float3 color = ambient + diffuse + specular + textureEmission;
        return float4(color, albedo.a);
    }
}