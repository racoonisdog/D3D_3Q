#include "Shared.fxh"

float4 main(VShaderOut pIn) : SV_TARGET
{
    float4 finalColor = 0;
    
    //do NdotL lighting for 2 lights
    for (int i = 0; i < 2; i++)
    {
        finalColor += saturate(dot((float3) LightDir[i], pIn.Norm) * LightColor[i]);
    }
    finalColor.a = 1;
    return finalColor;
}

float4 PSSolid(VShaderOut pIn) : SV_Target
{
    return OutputColor;
}