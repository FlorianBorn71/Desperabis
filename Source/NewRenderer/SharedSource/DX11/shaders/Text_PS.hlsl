cbuffer TextConstants : register(b0)
{
    float4 ScreenAndPosition;
    float4 TintColor;
    float4 Scale;
};

Texture2D glyphAtlas : register(t0);
SamplerState glyphSampler : register(s0);

struct PixelInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
};

float4 ps_main_text(PixelInput input) : SV_TARGET
{
    float coverage = glyphAtlas.Sample(glyphSampler, input.texCoord).r;
    float4 color = input.color * TintColor;
    return float4(color.rgb, color.a * coverage);
}
