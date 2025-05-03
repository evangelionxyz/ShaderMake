#include "binding_helpers.hlsli"

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
    float2 tilingFactor : TILINGFACTOR;
    float4 color : COLOR;
    uint texIndex : TEXINDEX;
};

VK_BINDING(0, 0)  Texture2D textures[16] : REGISTER_SRV(0, 0);
VK_BINDING(0, 0)  SamplerState samplerState : REGISTER_SAMPLER(0, 0);

float4 main(PSInput input) : SV_TARGET
{
    float4 texColor = textures[input.texIndex].Sample(samplerState, input.texCoord * input.tilingFactor);
    return input.color * texColor;
}