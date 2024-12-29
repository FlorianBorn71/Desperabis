
#include <CommonDefines.h>


////////////////////////////////////////////////////////////////////////////////////////////////
// Constant buffers
////////////////////////////////////////////////////////////////////////////////////////////////

// A constant buffer that stores the three basic column-major matrices for composing geometry.
cbuffer GlobalsConstantBuffer : register(b0)
{
    matrix WorldViewMatLeftRight[2]; // world to view
    matrix ViewWorldMatLeftRight[2]; // view back to world
    matrix ProjMatLeftRight[2];
    matrix WorldViewProjMatLeftRight[2];
    float Gamma, InverseGamma, fPadding1, fPadding2;
    float4 RenderTargetSize; // xy=size, zw=1/size
    float4 AmbientColor; // xyz = color, w = contribution of up-normal
    float4 FogParameter; // xyz = color, w = fog scale
    float4 LightmapScaleOfs; // worldpos->lm coord
    float4 CurrentTime; // x: abs time, y: time modulo 2 pi
    int NumVisibleLights; // number of entries in LightListConstantBuffer
    int NumVisibleCoronas; // not used here
    int g_padding1, g_padding2;
};

cbuffer InstanceConstantBuffer : register(b1)
{
    matrix ModelToWSMat;
    float UniformScale;
    int VertexOfs;  // vertex offset of primary model
    int VertexOfs2; // vertex offset of secondary model
    float BlendWeight; // blend factor between two models
    float4 ObjectTint; // Object tint color

#ifdef _DEBUG
    float DebugColorTint; // only in debug mode
    float inst_padding1, inst_padding2, inst_padding3;
#endif
};

cbuffer PositionConstantBuffer : register(b2)
{
    float4 positionBuffer[2]; // maximum number of vertices = 64k/sizeof(float4), alternatively use structured buffer
};

struct TextureAtlasEntry
{
    float4 textureAtlasData; // scale U/V, ofs UV
    float4 spriteCorners; // xy, zw
};

cbuffer TextureAtlasConstantBuffer : register(b3)
{
    TextureAtlasEntry TextureAtlas[16];
};

cbuffer MaterialConstantBuffer : register(b4)
{
    float Transparency; // for transparent objects the alpha modulo (0..1)
    float ApplyLighting; // 0 or 1, whether lighting should be applied
    float mat_padding1, mat_padding2;
};

struct PackedLightSourceParams
{
    float4 PositionAndRadius;
    float4 ColorAndCoronaSize;
};

cbuffer LightListConstantBuffer : register(b5)
{
    PackedLightSourceParams Lights[16]; // can be more of course
}

cbuffer WaterShaderConstants : register(b6)
{
    float4 ScreenToRTUV; // xy=1/rt size, z=scale u, w= ofs u (so that zw = -1,1 mirrors u coordinate)
    float4 WaterUv1Scroll;
    float4 WaterUv2Scroll;
};


// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
#ifdef ENABLE_ADVANCED_RENDERING
    float3 normal : NORMAL;
#endif
    float4 color : COLOR;
    uint4 positionIndexAndMode : TEXCOORD0;
    float2 uv0 : TEXCOORD1;
    float2 stretchParams : TEXCOORD2;
    uint instId  : SV_InstanceID;
};

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float4 emissiveColor : COLOR1;
    float2 uv0 : TEXCOORD0;
    float4 worldPosition : TEXCOORD1; // w = fog coordinate

#ifdef ENABLE_ADVANCED_RENDERING
    float3 normal : TEXCOORD3;
#endif

    // The render target array index is set here in the vertex shader.
#ifdef IS_VERTEX_SHADER
    uint viewId  : SV_RenderTargetArrayIndex;
#endif
};

// texture
Texture2D baseTexture : register(t0);
Texture2D lightmap : register(t1);
//Texture2D tex3 : register(t2);
SamplerState defaultSampler : register(s0);
SamplerState samplerRT : register(s1); // clamped


//#ifdef ENABLE_ADVANCED_RENDERING
struct PixelShaderInput_Mirror
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float4 scrollUV : TEXCOORD0; // 2 sets
    float fogCoord : TEXCOORD1;

    // The render target array index is set here in the vertex shader.
#ifdef IS_VERTEX_SHADER
    uint viewId  : SV_RenderTargetArrayIndex;
#endif
};

struct GeometryShaderInput_Corona
{
    float4 position : SV_POSITION;
    float4 color : COLOR0; // color and intensity
    float2 radiusAndFogWeight : TEXCOORD0;

    // The render target array index is set here in the vertex shader.
#if defined (IS_VERTEX_SHADER) || defined (IS_GEOMETRY_SHADER)
    uint viewId  : SV_RenderTargetArrayIndex;
#endif
};


struct PixelShaderInput_Corona
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float3 uv0 : TEXCOORD0; // z=fog lerp

    // The render target array index is set here in the vertex shader.
#ifdef IS_GEOMETRY_SHADER
    uint viewId  : SV_RenderTargetArrayIndex;
#endif
};

struct PixelShaderInput_Flames
{
    float4 position : SV_POSITION;
    float2 uv0 : TEXCOORD0;
    float intensity : TEXCOORD1;

    // The render target array index is set here in the vertex shader.
#if defined (IS_VERTEX_SHADER) || defined (IS_GEOMETRY_SHADER)
    uint viewId  : SV_RenderTargetArrayIndex;
#endif
};


//#endif

////////////////////////////////////////////////////////////////////////////////////////////////
// Common functions
////////////////////////////////////////////////////////////////////////////////////////////////

float GetFogWeight(float fogCoord)
{
    return saturate(exp(FogParameter.w * fogCoord));
}

float3 ApplyFog(float3 currentColor, float fogCoord)
{
    float fogWeight = GetFogWeight(fogCoord);
    return lerp(FogParameter.xyz, currentColor, fogWeight);
}


////////////////////////////////////////////////////////////////////////////////////////////////
// Vertex shader
////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef IS_VERTEX_SHADER

#define STRETCHMODE_None 0
#define STRETCHMODE_Sprite 1
#define STRETCHMODE_StretchedLine 2
#define STRETCHMODE_Flame 3


// Vertex shader for common geometry
PixelShaderInput vs_main(VertexShaderInput In)
{
    PixelShaderInput Out;

    // Note which view this vertex has been sent to. Used for matrix lookup.
    // Taking the modulo of the instance ID allows geometry instancing to be used
    // along with stereo instanced drawing; in that case, two copies of each 
    // instance would be drawn, one for left and one for right.
    int idx = In.instId % 2;

    matrix WorldViewMat = WorldViewMatLeftRight[idx];
    matrix ViewWorldMat = ViewWorldMatLeftRight[idx];
    matrix ProjMat = ProjMatLeftRight[idx];
    matrix WorldViewProjMat = WorldViewProjMatLeftRight[idx];

    int posIndex = In.positionIndexAndMode[0];
    int otherPosIndex = In.positionIndexAndMode[1];
    int stretchMode = In.positionIndexAndMode[2];

    // model position 1 and 2 (pos 2 is for stretched lines)
    float3 inPosition = positionBuffer[posIndex + VertexOfs].xyz;
    float3 inPosition2 = positionBuffer[otherPosIndex + VertexOfs].xyz;

    float3 wsNormal = float3(1,0,0);

    // animated
    if (VertexOfs2 >= 0)
    {
        float3 lerpPos = positionBuffer[posIndex + VertexOfs2].xyz;
        float3 lerpPos2 = positionBuffer[otherPosIndex + VertexOfs2].xyz;
        float weight = BlendWeight;
        if (weight < 0) // random weight per vertex
        {
            float random = CurrentTime.x * 50 + posIndex * 13;
            weight = (sin(random) * 0.5 + 0.5) * -weight;
        }
        inPosition += (lerpPos - inPosition) * weight;
        inPosition2 += (lerpPos2 - inPosition2) * weight;
    }

    float4 worldSpacePos = mul(float4(inPosition, 1), ModelToWSMat);
    float4 viewPos = mul(float4(worldSpacePos.xyz, 1), WorldViewMat);

    if (stretchMode == STRETCHMODE_None)
    {
#ifdef ENABLE_ADVANCED_RENDERING
        wsNormal = mul(float4(In.normal, 0), ModelToWSMat).xyz;
        //float ampl = sin(CurrentTime.y) * 0.1f;
        //worldSpacePos.xyz += wsNormal * ampl;
#endif
        Out.position = mul(float4(worldSpacePos.xyz, 1), WorldViewProjMat);
    }
    else if (stretchMode == STRETCHMODE_Sprite)
    {        
        float2 cornerSelect = float2(In.stretchParams.x, In.uv0.y); // uv.y is never flipped, for u we need the incomming stretch param
        float spriteScale = In.stretchParams.y * UniformScale;

        float2 spriteCorner = lerp(TextureAtlas[0].spriteCorners.xy, TextureAtlas[0].spriteCorners.zw, cornerSelect);
        float2 spriteOfs = spriteCorner * spriteScale;
        float3 up = mul(float4(0, -1, 0, 0), WorldViewMat).xyz;
        float3 right = normalize(cross(viewPos.xyz, up));
        //viewPos.xy -= spriteOfs;
        viewPos.xyz += up * spriteOfs.y + right * spriteOfs.x;
        worldSpacePos = mul(viewPos, ViewWorldMat);
        Out.position = mul(viewPos, ProjMat);

#ifdef ENABLE_ADVANCED_RENDERING
        const float roundness = -1.f;
        float4 viewNormal = float4(normalize(spriteOfs.xy), roundness, 0);
        wsNormal = normalize(mul(viewNormal, ViewWorldMat).xyz);
#endif
    }
    else if (stretchMode == STRETCHMODE_StretchedLine)
    {
        float4 otherPosWS = mul(float4(inPosition2, 1), ModelToWSMat);
        float4 otherPos = mul(float4(otherPosWS.xyz, 1), WorldViewMat);
        float3 diff = viewPos.xyz - otherPos.xyz;
        float3 up = normalize(cross(viewPos.xyz, diff));
        viewPos.xyz += up * In.stretchParams.x * UniformScale;

        worldSpacePos = mul(viewPos, ViewWorldMat);
        Out.position = mul(viewPos, ProjMat);

#ifdef ENABLE_ADVANCED_RENDERING
        const float roundness = -1.f;
        float4 viewNormal = float4(up.x, up.y, up.z + roundness, 0);
        wsNormal = normalize(mul(viewNormal, ViewWorldMat).xyz);
#endif
    }
    else //if (stretchMode == STRETCHMODE_Flame)
    {
        float fireIntensity = Transparency;
        float factor = 50.f + (float)posIndex * 4;
        float randomTime = (float)(posIndex * 11 + VertexOfs) + fireIntensity * fireIntensity * factor;

        float2 cornerSelect = float2(In.stretchParams.x, In.uv0.y); // uv.y is never flipped, for u we need the incomming stretch param
        float spriteScale = In.stretchParams.y * UniformScale;
        spriteScale *= (1 + 0.5*sin(randomTime)) * fireIntensity; // flickering scale
        float2 spriteCorner = lerp(TextureAtlas[0].spriteCorners.xy, TextureAtlas[0].spriteCorners.zw, cornerSelect);
        float2 spriteOfs = spriteCorner * spriteScale;
        float3 up = mul(float4(0, -1, 0, 0), WorldViewMat).xyz;
        float3 right = normalize(cross(viewPos.xyz, up));
        viewPos.xyz += up * spriteOfs.y + right * spriteOfs.x;
        viewPos.z += 0.01f;
        worldSpacePos = mul(viewPos, ViewWorldMat);
        Out.position = mul(viewPos, ProjMat);
    }


#ifdef ENABLE_ADVANCED_RENDERING
    Out.normal = wsNormal;
#endif 

    Out.color.rgb = In.color.rgb * ObjectTint.rgb;
    Out.color.a = ObjectTint.a * Transparency;

    Out.emissiveColor = float4(In.color.rgb * In.color.a, 0);

#ifdef _DEBUG
    if (DebugColorTint > 0.f)
    {
        Out.color = float4(0, 5, 0, 1);
    }
#endif
    Out.uv0 = In.uv0 * TextureAtlas[0].textureAtlasData.xy + TextureAtlas[0].textureAtlasData.zw;
    // Set the render target array index.
    Out.viewId = idx;
    Out.worldPosition.xyz = worldSpacePos.xyz;
    Out.worldPosition.w = viewPos.z; // fog coordinate
    return Out;
}


//#ifdef ENABLE_ADVANCED_RENDERING

// main vertex shader for all mirror receiver objects
PixelShaderInput_Mirror vs_main_receiver(VertexShaderInput In)
{
    PixelShaderInput_Mirror Out;
    PixelShaderInput Out1 = vs_main(In);

    Out.position = Out1.position;
    Out.color = Out1.color;
    // In this case we want the vertex alpha in here:
    Out.color.a *= In.color.a;

    // spriteCorners used here for 2 pairs of scroll offset
    Out.scrollUV.xy = Out1.worldPosition.xz * WaterUv1Scroll.xy + WaterUv1Scroll.zw;
    Out.scrollUV.zw = Out1.worldPosition.zx * WaterUv2Scroll.xy + WaterUv2Scroll.zw;
    Out.fogCoord = Out1.worldPosition.w; // fog coordinate
    Out.viewId = Out1.viewId;

    return Out;
}

// main vertex shader for coronas
GeometryShaderInput_Corona vs_main_corona(VertexShaderInput In)
{
    GeometryShaderInput_Corona Out;

    // Note which view this vertex has been sent to. Used for matrix lookup.
    // Taking the modulo of the instance ID allows geometry instancing to be used
    // along with stereo instanced drawing; in that case, two copies of each 
    // instance would be drawn, one for left and one for right.
    int idx = In.instId % 2;
    // Set the render target array index.
    Out.viewId = idx;

    matrix WorldViewMat = WorldViewMatLeftRight[idx];
//    matrix ViewWorldMat = ViewWorldMatLeftRight[idx];
//    matrix ProjMat = ProjMatLeftRight[idx];
    matrix WorldViewProjMat = WorldViewProjMatLeftRight[idx];

    int lightIndex = In.positionIndexAndMode[0];

    float4 lightPos = Lights[lightIndex].PositionAndRadius;
    float4 lightColor = Lights[lightIndex].ColorAndCoronaSize;
    float4 worldPos = float4(lightPos.xyz, 1);
    float4 viewPos = mul(worldPos, WorldViewMat);
    float3 viewDir = normalize(viewPos.xyz) * 0.1;
    //viewPos.xyz -= viewDir; // move slightly towards camera
    float4 projPos = mul(worldPos, WorldViewProjMat);

    float3 screenPos = projPos.xyz / projPos.w;
    screenPos.x = screenPos.x * 0.5 + 0.5;
    screenPos.y = screenPos.y * -0.5 + 0.5;

    float accum = 0.f;
    const int pixelRad = 5;
    const int edge = 2 * pixelRad + 1;
    const float fraction = 1.0 / (edge * edge);
    for (int y = -pixelRad; y <= pixelRad; y++)
    {
        float vOfs = y * RenderTargetSize.w; // 1/height
        for (int x = -pixelRad; x <= pixelRad; x++)
        {
            float uOfs = x * RenderTargetSize.z; // 1/width
            float4 texCol = baseTexture.SampleLevel(defaultSampler, screenPos.xy + float2(uOfs,vOfs), 0);
            float depthDiff = (texCol.r > screenPos.z) ? fraction : 0.f;
            accum += depthDiff;
        }
    }
    //accum *= (1.0 - GetFogWeight(viewPos.z)); // fade in coronas with fog

    Out.position = viewPos;
    Out.color.rgb = lightColor.rgb;
    Out.color.a = accum; // intensity
    Out.radiusAndFogWeight.x = lightPos.w * projPos.w; // radius but always same screen size
    Out.radiusAndFogWeight.y = GetFogWeight(viewPos.z);
    return Out;
}

// vertex shader for burning objects
PixelShaderInput_Flames vs_main_flames(VertexShaderInput In)
{
    PixelShaderInput temp = vs_main(In);
    PixelShaderInput_Flames Out;
    Out.position = temp.position;
    Out.uv0.xy = temp.uv0.xy;
    Out.intensity = saturate(Transparency+0.5);
    Out.viewId = temp.viewId;
    return Out;
}


// #endif // ENABLE_ADVANCED_RENDERING

#endif // IS_VERTEX_SHADER


////////////////////////////////////////////////////////////////////////////////////////////////
// Pixel shader
////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef IS_PIXEL_SHADER

#ifdef ENABLE_ADVANCED_RENDERING
float3 DynamicLightingColor(float3 worldPos, float3 worldNormal, float3 lightmapColor)
{
    float ambient = 1.f - worldNormal.y * AmbientColor.w;
    float3 lightAccum = AmbientColor.xyz * ambient * lightmapColor;
    for (int i = 0; i < NumVisibleLights; i++)
    {
        float3 dist = worldPos - Lights[i].PositionAndRadius.xyz;
        float distance = length(dist);
        float3 L = dist / distance;
        float nDotL = saturate(dot(L, worldNormal));
        float atten = saturate(1.0 - distance / Lights[i].PositionAndRadius.w) * nDotL;

        lightAccum += Lights[i].ColorAndCoronaSize.xyz * atten;
    }
    return lightAccum;
}
#endif // ENABLE_ADVANCED_RENDERING


// main pixel shader for all types of geometry
float4 ps_main(PixelShaderInput input) : SV_TARGET
{
    float4 texCol = baseTexture.Sample(defaultSampler, input.uv0.xy);
    clip(texCol.a - 0.5);

    float4 finalColor = input.color * texCol + input.emissiveColor;

    // for transparent objects, apply extra smooth alpha:
    finalColor.a *= saturate(dot(finalColor.rgb, float3(0.2126 * 3, 0.7152 * 3, 0.0722 * 3)));

#ifdef USE_FOG
    if (ApplyLighting > 0.0f)
    {
#ifdef ENABLE_ADVANCED_RENDERING
        float2 lmCoord = input.worldPosition.xz * LightmapScaleOfs.xy + LightmapScaleOfs.zw;
        float3 lightmapColor = lightmap.Sample(defaultSampler, lmCoord).xyz;

        finalColor.xyz = finalColor.xyz * DynamicLightingColor(input.worldPosition.xyz, input.normal, lightmapColor);
        //finalColor.xyz = DynamicLightingColor(input.worldPosition.xyz, input.normal, float3(0,0,0)) * 4;
#endif
        finalColor.xyz = ApplyFog(finalColor.xyz, input.worldPosition.w);
    }
#endif
    //finalColor.xyz = input.normal.xyz * 0.5 + 0.5;

    finalColor.xyz = pow(finalColor.xyz, Gamma);

    return finalColor;
}

//#ifdef ENABLE_ADVANCED_RENDERING

// main pixel shader for all mirror receiver objects
float4 ps_main_receiver(PixelShaderInput_Mirror input) : SV_TARGET
{
    float3 nrmlMap1 = lightmap.Sample(defaultSampler, input.scrollUV.xy).xyz;
    float3 nrmlMap2 = lightmap.Sample(defaultSampler, input.scrollUV.zw).xyz;
    float3 nrmlMap = (nrmlMap1 + nrmlMap2) - 1; // [-1..1] range
    //nrmlMap.z *= WaterDistortionStrength;
    nrmlMap = normalize(nrmlMap);
    nrmlMap.xy *= 0.030f; // pertubation amount
    nrmlMap.xy /= input.position.w; // perspective

    float2 rtUV = input.position.xy * ScreenToRTUV.xy + TextureAtlas[0].textureAtlasData.zw + nrmlMap.xy;
 //   float depthTex = tex3.Sample(samplerRT, rtUV).x;

    rtUV.x = rtUV.x * ScreenToRTUV.z + ScreenToRTUV.w; // mirror the u coordinate for mirrors, not refraction

    float4 finalColor = baseTexture.Sample(samplerRT, rtUV);
    finalColor.xyz *= input.color.xyz;
    // add a portion of the water color (glowing lava)
    finalColor.xyz += input.color.xyz * input.color.w;


//    finalColor.xyz = pow(finalColor.xyz, 1.5);

   // finalColor *= WaterTintColor;

    finalColor.xyz = ApplyFog(finalColor.xyz, input.fogCoord);
    finalColor.xyz = pow(finalColor.xyz, Gamma);

    return finalColor;
}

// main pixel shader for rendering coronas
float4 ps_main_corona(PixelShaderInput_Corona input) : SV_TARGET
{
    // lerp between corona texture and texture in fog
    float4 color1 = baseTexture.Sample(defaultSampler, input.uv0.xy);
    float4 color2 = lightmap.Sample(defaultSampler, input.uv0.xy) * 0.6f;
    float4 color = lerp(color2, color1, input.uv0.z);
    color *= input.color;
    return color;
}

// main pixel shader for all burning objects
float4 ps_main_flames(PixelShaderInput_Flames input) : SV_TARGET
{
    // lerp between corona texture and texture in fog
    float4 color = baseTexture.Sample(defaultSampler, input.uv0.xy);
    color.a *= input.intensity;
    return color;
}

// #endif // ENABLE_ADVANCED_RENDERING

#endif // IS_PIXEL_SHADER



#ifdef IS_GEOMETRY_SHADER

[maxvertexcount(4)]
void gs_main_corona(point GeometryShaderInput_Corona input[1], inout TriangleStream<PixelShaderInput_Corona> OutputStream)
{
    PixelShaderInput_Corona v;
    float4 centerViewPos = input[0].position;
    int idx = input[0].viewId;

    matrix ProjMat = ProjMatLeftRight[idx];
    float radius = input[0].radiusAndFogWeight.x; // corona size

    v.color = input[0].color;
    if (v.color.a < 0.001 || radius < 0.0001)
        return; // clip

    v.viewId = idx;
    float fogWeight = input[0].radiusAndFogWeight.y; // fog lerp
   // v.uv0.z = smoothstep(0, 1, fogWeight); // exaggerate effect
    v.uv0.z = saturate(fogWeight + 0.25f); // bias towards non-fogged corona texture
    float4 viewPos;

    // add 2 triangles:
    viewPos = centerViewPos;
    viewPos.xy += float2(-radius, radius);
    v.uv0.xy = float2(0, 1);
    v.position = mul(viewPos, ProjMat);
    OutputStream.Append(v);

    viewPos = centerViewPos;
    viewPos.xy += float2(radius, radius);
    v.uv0.xy = float2(1, 1);
    v.position = mul(viewPos, ProjMat);
    OutputStream.Append(v);

    viewPos = centerViewPos;
    viewPos.xy += float2(-radius, -radius);
    v.uv0.xy = float2(0, 0);
    v.position = mul(viewPos, ProjMat);
    OutputStream.Append(v);
    
    viewPos = centerViewPos;
    viewPos.xy += float2(radius, -radius);
    v.uv0.xy = float2(1, 0);
    v.position = mul(viewPos, ProjMat);
    OutputStream.Append(v);
}

#endif // IS_GEOMETRY_SHADER
