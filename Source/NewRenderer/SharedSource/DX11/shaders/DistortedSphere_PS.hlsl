#define IS_PIXEL_SHADER
#include "ShaderCommon.hlsl"

Texture2D sceneColor : register(t2);

float4 ps_main_distorted_sphere(PixelShaderInput input) : SV_TARGET
{
    float4 mask = baseTexture.Sample(defaultSampler, input.uv0);
    float2 localUV = (input.uv0 - TextureAtlas[0].textureAtlasData.zw) /
        TextureAtlas[0].textureAtlasData.xy;
    float2 local = localUV * 2.0 - 1.0;
    float radiusSquared = dot(local, local);
    float coverage = saturate((1.0 - radiusSquared) / max(fwidth(radiusSquared), 0.0001));
    clip(min(mask.a, coverage) - 0.001);

    // The sphere contains an inverted image of the whole scene, not a local lens.
    float radialStretch = rsqrt(max(1.0 - radiusSquared, 0.01));
    float2 screenUV = input.position.xy * RenderTargetSize.zw;
    float2 sampleUV = clamp(0.5 - local * radialStretch * 0.5,
        RenderTargetSize.zw * 0.5, 1.0 - RenderTargetSize.zw * 0.5);
    float3 background = sceneColor.Sample(samplerRT, screenUV).rgb;
    float3 refracted = sceneColor.Sample(samplerRT, sampleUV).rgb;
    float centerNess = mask.r;
    float3 glass = refracted * centerNess;
    return float4(lerp(background, glass, coverage * mask.a), 1.0);
}
