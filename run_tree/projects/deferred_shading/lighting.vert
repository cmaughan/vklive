#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"
 
layout (location = 0) in vec4 inPos;
layout (location = 1) in vec2 inUV;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec3 outEyePos;

struct Light {
    vec4 position;
    vec3 color;
    float radius;
};

void main() 
{
    gl_Position = inPos;
    outUV = vec2(inUV.x, 1 - inUV.y);
    outEyePos = vec3(ubo.eye);
}
