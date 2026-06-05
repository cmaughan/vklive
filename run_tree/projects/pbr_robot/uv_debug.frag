#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"
#include "vklive_pbr_material.glsl"

layout(set = 1, binding = 0) uniform sampler2D studio_sky;

layout(location = 0) in vec3 outWorldPos;
layout(location = 1) in vec3 outNormal;
layout(location = 2) in vec3 outColor;
layout(location = 3) in vec2 outUV;

layout(location = 0) out vec4 outFragColor;

vec3 materialDebugColor(uint materialIndex)
{
    if (materialIndex == 0)
    {
        return vec3(2.0, 0.12, 0.08);
    }
    if (materialIndex == 1)
    {
        return vec3(0.08, 0.85, 0.2);
    }
    if (materialIndex == 2)
    {
        return vec3(0.15, 0.35, 1.0);
    }
    return vec3(1.0, 0.0, 1.0);
}

float uvGrid(vec2 uv, float cells, float width)
{
    vec2 lineDistance = abs(fract(uv * cells) - 0.5);
    float nearestLine = 0.5 - min(lineDistance.x, lineDistance.y);
    return 1.0 - smoothstep(width, width * 2.0, nearestLine);
}

float outsideUv(vec2 uv)
{
    vec2 below = step(uv, vec2(0.0));
    vec2 above = step(vec2(1.0), uv);
    return clamp(below.x + below.y + above.x + above.y, 0.0, 1.0);
}

void main()
{
    bool useImportedUv = gl_FragCoord.x < ubo.iResolution.x * 0.5;
    vec2 flippedUv = vec2(outUV.x, 1.0 - outUV.y);
    vec2 sampleUv = useImportedUv ? outUV : flippedUv;

    vec4 baseColor = vklBaseColor(sampleUv);
    vec3 color = clamp(baseColor.rgb, vec3(0.0), vec3(1.0));

    vec3 uvGradient = vec3(fract(sampleUv.x), fract(sampleUv.y), 0.0);
    color = mix(color, uvGradient, 0.28);

    float coarseGrid = uvGrid(sampleUv, 10.0, 0.025);
    float fineGrid = uvGrid(sampleUv, 40.0, 0.012);
    color = mix(color, vec3(1.0), max(coarseGrid * 0.65, fineGrid * 0.25));

    uint materialIndex = vklMaterialIndex();
    vec3 materialColor = materialDebugColor(materialIndex);
    float materialStripe = 1.0 - step(0.09, fract((sampleUv.x + sampleUv.y) * 18.0));
    color = mix(color, materialColor, materialStripe * 0.35);

    float uvOutOfRange = outsideUv(outUV);
    color = mix(color, vec3(1.0, 0.0, 1.0), uvOutOfRange);

    float splitLine = 1.0 - smoothstep(0.0, 3.0, abs(gl_FragCoord.x - ubo.iResolution.x * 0.5));
    color = mix(color, vec3(0.0, 1.0, 1.0), splitLine);

    outFragColor = vec4(color, 1.0);
}
