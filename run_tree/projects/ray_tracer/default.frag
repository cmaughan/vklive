#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout (location = 0) in vec3 inColor;

layout (location = 0) out vec4 outColor;

void main() 
{
    outColor = vec4(inColor, 1.0);
}