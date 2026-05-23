#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inNormal;
layout (location = 4) in vec3 inTangent;
layout (location = 5) in vec3 inBitangent;

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 outColor;
layout (location = 3) out vec2 outUV;
layout (location = 4) out vec3 outTangent;
layout (location = 5) out vec3 outBitangent;

mat3 rotateY(float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        c, 0.0, -s,
        0.0, 1.0, 0.0,
        s, 0.0, c);
}

vec3 safeNormalize(vec3 v, vec3 fallback)
{
    float len2 = dot(v, v);
    return len2 > 0.0000001 ? v * inversesqrt(len2) : fallback;
}

void main()
{
    mat3 rotation = rotateY(ubo.iTime * 0.35);
    vec3 rotatedPos = rotation * inPos.xyz;
    vec3 rotatedNormal = rotation * inNormal;
    vec3 rotatedTangent = rotation * inTangent;
    vec3 rotatedBitangent = rotation * inBitangent;

    vec4 worldPos = ubo.model * vec4(rotatedPos * 1, 1.0);
    mat3 normalMatrix = mat3(transpose(inverse(ubo.model)));
    outWorldPos = worldPos.xyz;
    outNormal = safeNormalize(normalMatrix * rotatedNormal, vec3(0.0, 1.0, 0.0));
    outTangent = safeNormalize(normalMatrix * rotatedTangent, vec3(1.0, 0.0, 0.0));
    outBitangent = safeNormalize(normalMatrix * rotatedBitangent, vec3(0.0, 0.0, 1.0));
    outColor = max(inColor, vec3(0.44));
    outUV = inUV;
    gl_Position = ubo.projection * ubo.view * worldPos;
}
