#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout (location = 0) in vec4 inPos;

layout (location = 0) out vec3 outRay;

void main()
{
    vec2 clip = inPos.xy;
    vec4 view = ubo.projectionInverse * vec4(clip, 1.0, 1.0);
    view = vec4(view.xy, -1.0, 0.0);
    outRay = normalize((ubo.viewInverse * view).xyz);
    gl_Position = vec4(clip, 0.9999, 1.0);
}
