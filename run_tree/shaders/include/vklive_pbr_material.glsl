#ifndef VKLIVE_PBR_MATERIAL_GLSL
#define VKLIVE_PBR_MATERIAL_GLSL

#define VKLIVE_MAX_MATERIALS 64

struct VklMaterial
{
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    vec4 metallicRoughnessOcclusion;
    ivec4 textureIndices;
};

layout(push_constant) uniform VklDraw
{
    uint materialIndex;
} vklDraw;

layout(std430, set = 2, binding = 0) readonly buffer VklMaterials
{
    VklMaterial vklMaterials[];
};

layout(set = 2, binding = 1) uniform sampler2D vklBaseColorTextures[VKLIVE_MAX_MATERIALS];
layout(set = 2, binding = 2) uniform sampler2D vklNormalTextures[VKLIVE_MAX_MATERIALS];
layout(set = 2, binding = 3) uniform sampler2D vklMetallicRoughnessTextures[VKLIVE_MAX_MATERIALS];
layout(set = 2, binding = 4) uniform sampler2D vklEmissiveTextures[VKLIVE_MAX_MATERIALS];
layout(set = 2, binding = 5) uniform sampler2D vklOcclusionTextures[VKLIVE_MAX_MATERIALS];

uint vklMaterialIndex()
{
    return min(vklDraw.materialIndex, uint(VKLIVE_MAX_MATERIALS - 1));
}

vec4 vklBaseColor(vec2 uv)
{
    uint index = vklMaterialIndex();
    return texture(vklBaseColorTextures[index], uv) * vklMaterials[index].baseColorFactor;
}

vec3 vklNormalSample(vec2 uv)
{
    uint index = vklMaterialIndex();
    return texture(vklNormalTextures[index], uv).xyz * 2.0 - 1.0;
}

vec2 vklMetallicRoughness(vec2 uv)
{
    uint index = vklMaterialIndex();
    vec4 sampleValue = texture(vklMetallicRoughnessTextures[index], uv);
    vec4 factors = vklMaterials[index].metallicRoughnessOcclusion;
    return vec2(sampleValue.b * factors.x, sampleValue.g * factors.y);
}

vec3 vklEmissive(vec2 uv)
{
    uint index = vklMaterialIndex();
    return texture(vklEmissiveTextures[index], uv).rgb * vklMaterials[index].emissiveFactor.rgb;
}

float vklOcclusion(vec2 uv)
{
    uint index = vklMaterialIndex();
    return texture(vklOcclusionTextures[index], uv).r * vklMaterials[index].metallicRoughnessOcclusion.z;
}

#endif
