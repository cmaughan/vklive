#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"
 
layout (location = 0) in vec4 inPos;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec2 outUV;

void main() 
{
    outUV = inUV;
    gl_Position = inPos;
}
