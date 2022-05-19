#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

layout (binding = 1) uniform sampler2D samplerA;

void main() 
{
    outFragColor = texture(samplerA, inUV);
}