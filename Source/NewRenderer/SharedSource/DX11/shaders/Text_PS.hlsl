cbuffer TextConstants : register(b0)
{
    float4 ScreenAndPosition;
    float4 TintColor;
    float4 ScaleAndTextureMode;
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
    float4 texel = glyphAtlas.Sample(glyphSampler, input.texCoord);
    float4 color = input.color * TintColor;
    if (ScaleAndTextureMode.y > 0.5f)
    {
        return color * texel;
    }
    return float4(color.rgb, color.a * texel.r);
}
