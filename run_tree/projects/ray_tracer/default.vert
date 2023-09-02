#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout (location = 0) in vec4 inPos;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inNormal;

layout (location = 0) out vec3 outColor;

void main() 
{
    gl_Position = ubo.modelViewProjection * inPos;
    outColor = inColor;
}
