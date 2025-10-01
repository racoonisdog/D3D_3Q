#include <Shared.fxh>

float4 main(VShaderOut input) : SV_TARGET
{
    //택스쳐의 색 ( 즉 그림 )을 계산하는 식
    float4 albedo = txDiffuse.Sample(samLinear, input.Tex);
    // 한 면의 법선벡터수치를 단위벡터로 만드는  식
    float3 NormalV = normalize(input.Norm);

    // 방향광 가정: LightDirection이 "광원→장면"이면 표면→광원은 -LightDirection
    float3 LightV  = -normalize(LightDir.xyz);			// LightDir가 '물체 -> 광원' 방향
    float3 Vector = normalize(camPos - input.WorldPos); 	//반사된 빛을 바라볼 카메라 벡터 ( World는 물체의 좌표, 물체에서 카메라 위치까지의 벡터를 뜻함)

    float DiffuseFactor = saturate(dot(NormalV, LightV ));		// 법선벡터와 빛의 반사 벡터를 내적 ( saturate -> 0 ~ 1 범위의 값으로 조정해 주는 역할 , 0보다 작으면 0으로 1보다 크면 1로)
	// 왜해주는가? 둘의 내적 => cos세타 값이기 때문에 최소 -1~1 이지만 음수의 경우 표면이 빛을 등지고 있다는 것, 그렇기 때문에 조명 기여를 0으로 잘라내는것이 맞다, 이렇기 해서 나온것은 조명 강도를 정하는 스칼라 계수이다
    
    float3 ambient = (matambient.rgb * lightambient.rgb) * albedo.rgb;

    float3 diffuse = 0;
    float3 specular = 0;

    if (DiffuseFactor > 0.0f)								// 0아래면 반사가 안된다는것 -> 어두우니깐 계산안함
    {
        // Blinn-Phong
        float3 HalfVector = normalize(LightV  + Vector);				// half벡터 ( 빛과 시선의 중간 )
        float NdotH = saturate(dot(NormalV, HalfVector));					// 조명강도
        float spec = pow(NdotH, shininess);				// 조명강도를 Shininess만큼 제곱

        // 필요에 따라 LightColor vs LightDiffuse/Specular 중 하나만 쓰기
        diffuse  = albedo.rgb * matdiffuse.rgb  * lightdiffuse.rgb  * DiffuseFactor;
		specular =               matspecular.rgb * lightspecular.rgb * spec;
    }

    float3 color = ambient + diffuse + specular;
    return float4(color, albedo.a);
}

float4 PSSolid(VShaderOut pIn) : SV_Target
{
    return OutputColor;
}