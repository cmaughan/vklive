#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

layout (set = 1, binding = 0) uniform sampler2D samplerA;

void main() 
{
    vec2 texCoord = inUV ;
    texCoord.x += (sin(ubo.iTime * texCoord.x) * .01) * .5;
    texCoord.y += (cos(ubo.iTime * texCoord.y) * .01) * .5;
    outFragColor = texture(samplerA, texCoord);
    outFragColor.w = 1.0;
}