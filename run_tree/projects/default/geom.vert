#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout (location = 0) in vec4 inPos;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inNormal;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outEyePos;
layout (location = 3) out vec3 outLightVec;

void main() 
{
    vec4 p = inPos;
    p.xyz = p.xyz + (inNormal * sin(inPos.x * ubo.iTime * 3.5) * .96);
    gl_Position = ubo.modelViewProjection * p;
    outEyePos = vec3(ubo.view * p);
    outNormal = inNormal;
    outColor = inColor;
    vec3 lightPos = vec3(0.0, 0.0, 0.0);
    outLightVec = normalize(lightPos.xyz - outEyePos);
}
