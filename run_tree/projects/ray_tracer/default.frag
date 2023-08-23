#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec4 inPosition;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outAlbedo;
layout (location = 2) out vec4 outNormal;

void main() 
{
    outAlbedo = vec4(inColor, 1.0);
    outPosition = ubo.model * inPosition;
    outNormal = transpose(inverse(ubo.model)) * vec4(inNormal, 1.0);
}