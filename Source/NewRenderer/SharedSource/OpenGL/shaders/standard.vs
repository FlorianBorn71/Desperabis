#include "shaders/version.include"
#define IS_VERTEX_SHADER
#include "shaders/constantBuffers.include"

// Vertex input streams
layout(location = 0) in vec4 vertex_color;
layout(location = 1) in uvec4 vertex_positionIndexAndMode;
layout(location = 2) in vec2 vertex_uv;
layout(location = 3) in vec2 vertex_stretchParams;

// Output data
out vec2 UV;
out vec4 VertexColor;

#define STRETCHMODE_None 0
#define STRETCHMODE_Sprite 1
#define STRETCHMODE_StretchedLine 2

void main()
{
    mat4 WorldViewMat = WorldViewMatLeftRight[0];
    mat4 ViewWorldMat = ViewWorldMatLeftRight[0];
    mat4 ProjMat = ProjMatLeftRight[0];
    mat4 WorldViewProjMat = WorldViewProjMatLeftRight[0];

    uint posIndex = vertex_positionIndexAndMode.x | vertex_positionIndexAndMode.y << 8;
    uint otherPosIndex = vertex_positionIndexAndMode.z;
    uint stretchMode = vertex_positionIndexAndMode.w;

    vec3 inPosition = positionBuffer[posIndex + VertexOfs].xyz;
    vec3 inPosition2 = positionBuffer[otherPosIndex + VertexOfs].xyz;

    // animated
    if (VertexOfs2 >= 0)
    {
        vec3 lerpPos = positionBuffer[posIndex + VertexOfs2].xyz;
        vec3 lerpPos2 = positionBuffer[otherPosIndex + VertexOfs2].xyz;
        inPosition += (lerpPos - inPosition) * BlendWeight;
        inPosition2 += (lerpPos2 - inPosition2) * BlendWeight;
    }


    vec4 worldSpacePos = ModelToWSMat * vec4(inPosition, 1);
    vec4 viewPos = WorldViewMat * worldSpacePos;

    if (stretchMode == STRETCHMODE_None)
    {
        gl_Position =  WorldViewProjMat * worldSpacePos;
    }
    else if (stretchMode == STRETCHMODE_Sprite)
    {
        vec2 cornerSelect = vec2(vertex_stretchParams.x, vertex_uv.y); // uv.y is never flipped, for u we need the incoming stretch param
        float spriteScale = vertex_stretchParams.y * UniformScale;

        vec2 spriteCorner = mix(spriteCorners.xy, spriteCorners.zw, cornerSelect);
        vec2 spriteOfs = spriteCorner * spriteScale;
        vec3 up = (WorldViewMat * vec4(0, -1, 0, 0)).xyz;
        vec3 right = normalize(cross(viewPos.xyz, up));
        viewPos.xyz += up * spriteOfs.y + right * spriteOfs.x;
        worldSpacePos = ViewWorldMat * viewPos;
        gl_Position = ProjMat * viewPos;
    }
    else
    {
        vec4 otherPosWS = ModelToWSMat * vec4(inPosition2, 1);
        vec4 otherPos = WorldViewMat * vec4(otherPosWS.xyz, 1);
        vec3 diff = viewPos.xyz - otherPos.xyz;
        vec3 up = normalize(cross(viewPos.xyz, diff));
        viewPos.xyz += up * vertex_stretchParams.x * UniformScale;

        worldSpacePos = ViewWorldMat * viewPos;
        gl_Position = ProjMat * viewPos;
    }

	UV = vertex_uv.xy * textureAtlasData.xy + textureAtlasData.zw;
    VertexColor = vertex_color;
    VertexColor.a *= Transparency;
}
