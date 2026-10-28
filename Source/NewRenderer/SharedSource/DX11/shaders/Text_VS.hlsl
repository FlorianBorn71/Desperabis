cbuffer TextConstants : register(b0)
{
    float4 ScreenAndPosition;
    float4 TintColor;
    float4 ScaleAndTextureMode;
};

struct VertexInput
{
    float2 position : POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
};

struct PixelInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR0;
};

PixelInput vs_main_text(VertexInput input)
{
    PixelInput output;
    float2 screenPosition = input.position * ScaleAndTextureMode.x + ScreenAndPosition.zw;
    output.position = float4(
        screenPosition.x * (2.0f / ScreenAndPosition.x) - 1.0f,
        1.0f - screenPosition.y * (2.0f / ScreenAndPosition.y),
        0.0f,
        1.0f);
    output.texCoord = input.texCoord;
    output.color = input.color;
    return output;
}
