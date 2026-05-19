#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include <vklive/scene.h>
#include <vklive/shader_bindings.h>

namespace metal
{

struct MetalContext;
struct MetalScene;

enum class MetalShaderStage
{
    Vertex,
    Fragment,
};

struct MetalShaderResourceBinding
{
    uint32_t set = 0;
    uint32_t binding = 0;
    ShaderBindingType type = ShaderBindingType::Unknown;
    uint32_t bufferIndex = std::numeric_limits<uint32_t>::max();
    uint32_t textureIndex = std::numeric_limits<uint32_t>::max();
    uint32_t samplerIndex = std::numeric_limits<uint32_t>::max();
};

struct MetalShader
{
    explicit MetalShader(Shader* pS)
        : pShader(pS)
    {
    }

    Shader* pShader = nullptr;
    MetalShaderStage stage = MetalShaderStage::Vertex;
    ShaderBindingSets bindingSets;
    std::string mslSource;
    std::string entryPointName;
    void* library = nullptr;
    void* function = nullptr;

    // Keyed by reflected (descriptor set, binding). Metal slots are deterministic and dense per resource index space.
    std::map<std::pair<uint32_t, uint32_t>, MetalShaderResourceBinding> resourceBindings;
};

std::shared_ptr<MetalShader> metal_shader_create(MetalContext& ctx, MetalScene& scene, Shader& shader);
void metal_shader_destroy(MetalContext& ctx, MetalShader& shader);

} // namespace metal
