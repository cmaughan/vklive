#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inEyePos;
layout (location = 3) in vec3 inLightVec;
layout (location = 4) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

void main() 
{
    // Temp
    vec2 rg = (gl_FragCoord.xy / ubo.iResolution.xy) * 2.0 - 1.0; 
    float t = sin(ubo.iTime * .15f) * 10.0;
    rg.x = sin(rg.x * t) + cos(rg.y * t);
    rg.y = sin(rg.y * t) + cos(rg.x * t);

    //vec4 color = texture(samplerA, inUV);

    outFragColor.zw = vec2(0.0, 1.0);
    outFragColor.xy = rg;
}