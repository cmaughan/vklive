#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout (location = 0) in vec4 inPos;

void main() 
{
    gl_Position = inPos;
}
