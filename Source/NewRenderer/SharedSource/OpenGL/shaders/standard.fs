#include "shaders/version.include"
#define IS_FRAGMENT_SHADER
#include "shaders/constantBuffers.include"

// Interpolated values from the vertex shaders
in vec2 UV;
in vec4 VertexColor;

// Ouput data
out vec4 color;

uniform sampler2D baseTexture;

void main()
{
    color = texture( baseTexture, UV );
    if (color.a < 0.5)
    {
        discard;
    }
    color *= VertexColor;
	color.xyz = pow(color.xyz, Gamma);
}