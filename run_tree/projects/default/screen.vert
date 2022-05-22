#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"
 
layout (location = 0) in vec4 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor; 
layout (location = 3) in vec3 inNormal;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outEyePos;
layout (location = 3) out vec3 outLightVec;
layout (location = 4) out vec2 outUV;

layout (set = 3, binding = 0) uniform sampler2D samplerA;

void main() 
{
    outNormal = inNormal;
    outColor = inColor;
    outUV = inUV; 
    gl_Position = inPos;
    outEyePos = vec3(ubo.model * inPos);
    vec3 lightPos = vec3(0.0, 0.0, .0);
    outLightVec = normalize(lightPos.xyz - outEyePos);
}
