#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout (set = 1, binding = 0) uniform sampler2D studio_sky;

layout (location = 0) in vec3 outRay;

layout (location = 0) out vec4 outFragColor;

const float PI = 3.14159265359;

vec2 dirToEquirectUv(vec3 dir)
{
    dir = normalize(dir);
    return vec2(atan(dir.z, dir.x) / (2.0 * PI) + 0.5, asin(clamp(dir.y, -1.0, 1.0)) / PI + 0.5);
}

vec3 tonemap(vec3 color)
{
    color = color / (color + vec3(1.0));
    return pow(color, vec3(1.0 / 2.2));
}

void main()
{
    vec3 color = texture(studio_sky, dirToEquirectUv(outRay)).rgb;
    outFragColor = vec4(tonemap(color), 1.0);
}
