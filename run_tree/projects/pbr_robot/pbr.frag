#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"
#include "vklive_pbr_material.glsl"

layout (set = 1, binding = 0) uniform sampler2D studio_sky;

layout (location = 0) in vec3 outWorldPos;
layout (location = 1) in vec3 outNormal;
layout (location = 2) in vec3 outColor;
layout (location = 3) in vec2 outUV;
layout (location = 4) in vec3 outTangent;
layout (location = 5) in vec3 outBitangent;

layout (location = 0) out vec4 outFragColor;

const float PI = 3.14159265359;

vec2 dirToEquirectUv(vec3 dir)
{
    dir = normalize(dir);
    return vec2(atan(dir.z, dir.x) / (2.0 * PI) + 0.5, asin(clamp(dir.y, -1.0, 1.0)) / PI + 0.5);
}

vec3 sampleEnvironment(vec3 dir)
{
    return texture(studio_sky, dirToEquirectUv(dir)).rgb;
}

vec3 safeNormalize(vec3 v, vec3 fallback)
{
    float len2 = dot(v, v);
    return len2 > 0.0000001 ? v * inversesqrt(len2) : fallback;
}

vec3 fallbackTangent(vec3 n)
{
    vec3 up = abs(n.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    return safeNormalize(cross(up, n), vec3(1.0, 0.0, 0.0));
}

vec3 pbrNormal(vec2 uv)
{
    vec3 n = safeNormalize(outNormal, vec3(0.0, 1.0, 0.0));
    vec3 t = safeNormalize(outTangent - n * dot(outTangent, n), fallbackTangent(n));
    vec3 bitangentInput = safeNormalize(outBitangent, cross(n, t));
    float handedness = dot(cross(n, t), bitangentInput) < 0.0 ? -1.0 : 1.0;
    vec3 b = safeNormalize(cross(n, t) * handedness, cross(n, t));
    mat3 tbn = mat3(t, b, n);

    vec3 tangentNormal = vklNormalSample(uv);
    return safeNormalize(tbn * tangentNormal, n);
    //return vec3(0.0, 0.0, 0.0);
}

float distributionGGX(vec3 n, vec3 h, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float ndoth = max(dot(n, h), 0.0);
    float ndoth2 = ndoth * ndoth;
    float denom = ndoth2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 0.0001);
}

float geometrySchlickGGX(float ndotv, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return ndotv / max(ndotv * (1.0 - k) + k, 0.0001);
}

float geometrySmith(vec3 n, vec3 v, vec3 l, float roughness)
{
    float ndotv = max(dot(n, v), 0.0);
    float ndotl = max(dot(n, l), 0.0);
    return geometrySchlickGGX(ndotv, roughness) * geometrySchlickGGX(ndotl, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 tonemap(vec3 color)
{
    color = color / (color + vec3(1.0));
    return pow(color, vec3(1.0 / 1.2));
}

void main()
{
    vec3 n = pbrNormal(outUV);
    vec3 v = normalize(ubo.eye.xyz - outWorldPos);
    vec3 l = normalize(vec3(0.45, -0.35, 0.75));
    vec3 h = normalize(v + l);

    vec4 baseColor = vklBaseColor(outUV);
    vec2 metallicRoughness = vklMetallicRoughness(outUV);
    vec3 emissive = vklEmissive(outUV);
    float occlusion = vklOcclusion(outUV);

    vec3 albedo = clamp(baseColor.rgb, vec3(0.0), vec3(1.0)) * 3;
    float metallic = clamp(metallicRoughness.x, 0.0, 1.0);
    float roughness = clamp(metallicRoughness.y, 0.04, 1.0);

    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 f = fresnelSchlick(max(dot(h, v), 0.0), f0);
    float d = distributionGGX(n, h, roughness);
    float g = geometrySmith(n, v, l, roughness);

    float ndotl = max(dot(n, l), 0.0);
    float ndotv = max(dot(n, v), 0.0);
    vec3 numerator = d * g * f;
    float denominator = max(4.0 * ndotv * ndotl, 0.0001);
    vec3 specular = numerator / denominator;

    vec3 ks = f;
    vec3 kd = (vec3(1.0) - ks) * (1.0 - metallic);
    vec3 direct = (kd * albedo / PI + specular) * vec3(3.0, 2.85, 2.65) * ndotl;

    vec3 reflected = reflect(-v, n);
    vec3 envDiffuse = sampleEnvironment(n) * albedo * occlusion * 0.08;
    vec3 envSpecular = sampleEnvironment(reflected) * fresnelSchlick(ndotv, f0) * (1.0 - roughness * 0.85) * 0.95;

    vec3 color = direct + envDiffuse + envSpecular + emissive;
    outFragColor = vec4(tonemap(color), 5.0);
}
