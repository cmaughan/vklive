#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout (location = 0) out vec4 outFragColor;

void main() 
{
    // Temp
    vec2 coord = ((gl_FragCoord.xy / ubo.iResolution.xy) * 2.0 - 1.0);

    vec2 toCenter = vec2(0)-coord;
    float angle = atan(toCenter.y, toCenter.x);
    float radius = length(toCenter);

    vec4 color = vec4(1.0);
    color.y = cos(angle * ubo.iTime * 1.5); 
    color.z = sin(angle * ubo.iTime * 1.5);
    color.x = 0.0;

    outFragColor = color;
}