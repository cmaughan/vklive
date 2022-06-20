#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

// Sampler name here matches the surface declared in the scenegraph
// That's how the binding for sampling the target is resolved
layout (set = 1, binding = 0) uniform sampler2D A;

void main() 
{
    vec2 texCoord = inUV ;
//    texCoord.x += (sin(ubo.iTime * texCoord.x) * .01) * 4.0;
//    texCoord.y += (cos(ubo.iTime * texCoord.y) * .01) * 4.0;
    outFragColor = texture(A, texCoord);
    outFragColor.w = 1.0;
}