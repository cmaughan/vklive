#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

layout (binding = 1) uniform sampler2D samplerA;

void main() 
{
    vec2 texCoord= inUV * 4.0;
    outFragColor = vec4(1.0, 1.0, 1.0, 1.0) - texture(samplerA, texCoord);
    outFragColor.w = 1.0;
}