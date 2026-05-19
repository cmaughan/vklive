#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>

#include <zest/file/file.h>

enum class ShaderBindingType
{
    Unknown,
    Sampler,
    CombinedImageSampler,
    SampledImage,
    StorageImage,
    UniformTexelBuffer,
    StorageTexelBuffer,
    UniformBuffer,
    StorageBuffer,
    UniformBufferDynamic,
    StorageBufferDynamic,
    InputAttachment,
    AccelerationStructure,
};

enum class ShaderStageBits : uint32_t
{
    Vertex = 1 << 0,
    Fragment = 1 << 1,
    Geometry = 1 << 2,
    Compute = 1 << 3,
    RayGen = 1 << 4,
    ClosestHit = 1 << 5,
    Miss = 1 << 6,
    AnyHit = 1 << 7,
};

using ShaderStageFlags = uint32_t;

struct ShaderBinding
{
    uint32_t binding = 0;
    ShaderBindingType type = ShaderBindingType::Unknown;
    uint32_t count = 1;
    ShaderStageFlags stageFlags = 0;
};

struct ShaderBindingMeta
{
    std::string name;
    fs::path shaderPath;
    int32_t line = -1;
    std::pair<int32_t, int32_t> range = std::make_pair(-1, -1);
};

struct ShaderBindingSet
{
    std::map<uint32_t, ShaderBinding> bindings;
    std::map<uint32_t, ShaderBindingMeta> bindingMeta;
};

using ShaderBindingSets = std::map<uint32_t, ShaderBindingSet>;

inline const char* shader_binding_type_to_string(ShaderBindingType type)
{
    switch (type)
    {
    case ShaderBindingType::Sampler:
        return "Sampler";
    case ShaderBindingType::CombinedImageSampler:
        return "CombinedImageSampler";
    case ShaderBindingType::SampledImage:
        return "SampledImage";
    case ShaderBindingType::StorageImage:
        return "StorageImage";
    case ShaderBindingType::UniformTexelBuffer:
        return "UniformTexelBuffer";
    case ShaderBindingType::StorageTexelBuffer:
        return "StorageTexelBuffer";
    case ShaderBindingType::UniformBuffer:
        return "UniformBuffer";
    case ShaderBindingType::StorageBuffer:
        return "StorageBuffer";
    case ShaderBindingType::UniformBufferDynamic:
        return "UniformBufferDynamic";
    case ShaderBindingType::StorageBufferDynamic:
        return "StorageBufferDynamic";
    case ShaderBindingType::InputAttachment:
        return "InputAttachment";
    case ShaderBindingType::AccelerationStructure:
        return "AccelerationStructure";
    case ShaderBindingType::Unknown:
    default:
        return "Unknown";
    }
}
