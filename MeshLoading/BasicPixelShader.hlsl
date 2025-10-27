#include <Shared.fxh>

float4 main(VShaderOut input) : SV_TARGET
{
    float4 textureColor = gTexture.Sample(samLinear, input.Tex);

    return textureColor;
}
