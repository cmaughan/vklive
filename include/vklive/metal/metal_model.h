#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <vklive/model.h>

struct Geometry;

namespace metal
{

inline constexpr uint32_t MetalMaxMaterials = 64;

struct MetalGpuMaterial
{
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    glm::vec4 emissiveFactor = glm::vec4(0.0f);
    glm::vec4 metallicRoughnessOcclusion = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    glm::ivec4 textureIndices = glm::ivec4(0);
};

struct MetalDrawPushConstants
{
    uint32_t materialIndex = 0;
};

struct MetalModelTexture
{
    MetalModelTexture() = default;
    MetalModelTexture(const MetalModelTexture&) = delete;
    MetalModelTexture& operator=(const MetalModelTexture&) = delete;
    MetalModelTexture(MetalModelTexture&& other) noexcept
        : texture(other.texture)
        , sampler(other.sampler)
        , sourcePath(std::move(other.sourcePath))
        , fallback(other.fallback)
    {
        other.texture = nullptr;
        other.sampler = nullptr;
        other.fallback = true;
    }
    MetalModelTexture& operator=(MetalModelTexture&& other) noexcept = delete;

    void* texture = nullptr;
    void* sampler = nullptr;
    fs::path sourcePath;
    bool fallback = true;
};

struct MetalContext;
struct MetalScene;

struct MetalModel : Model
{
    Geometry* pGeometry = nullptr;
    void* vertexBuffer = nullptr;
    void* indexBuffer = nullptr;
    std::string debugName;
    uint32_t vertexStride = 0;
    bool staged = false;

    void* materialsBuffer = nullptr;
    std::vector<MetalGpuMaterial> gpuMaterials;
    std::vector<MetalModelTexture> materialTextures;
    MetalModelTexture fallbackBaseColorTexture;
    MetalModelTexture fallbackNormalTexture;
    MetalModelTexture fallbackMetallicRoughnessTexture;
    MetalModelTexture fallbackEmissiveTexture;
    MetalModelTexture fallbackOcclusionTexture;
    std::vector<MetalModelTexture*> baseColorTextures;
    std::vector<MetalModelTexture*> normalTextures;
    std::vector<MetalModelTexture*> metallicRoughnessTextures;
    std::vector<MetalModelTexture*> emissiveTextures;
    std::vector<MetalModelTexture*> occlusionTextures;
};

std::shared_ptr<MetalModel> metal_model_create(MetalContext& ctx, MetalScene& scene, Geometry& geometry);
void metal_model_destroy(MetalContext& ctx, MetalModel& model);

} // namespace metal
