#pragma once

#include <array>

#include "vklive/model.h"

#include "vulkan_context.h"
#include "vulkan_buffer.h"
#include "vulkan_surface.h"

namespace vulkan
{

inline constexpr uint32_t VulkanMaxMaterials = 64;

struct VulkanGpuMaterial
{
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    glm::vec4 emissiveFactor = glm::vec4(0.0f);
    glm::vec4 metallicRoughnessOcclusion = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    glm::ivec4 textureIndices = glm::ivec4(0);
};

struct VklDrawPushConstants
{
    uint32_t materialIndex = 0;
};

struct VulkanModelTexture
{
    VulkanSurface surface{ nullptr };
    vk::DescriptorImageInfo descriptor;
    fs::path sourcePath;
    bool fallback = true;
};

struct AccelerationStructure
{
    vk::AccelerationStructureKHR handle;
    VulkanBuffer buffer;
    vk::DeviceAddress asDeviceAddress;
    uint32_t vertexOffset;
};

struct VulkanModel : Model
{
    VulkanModel()
    {
    }

    VulkanBuffer vertices;
    VulkanBuffer indices;

    std::vector<AccelerationStructure> accelerationStructures;
    AccelerationStructure topLevelAS;
    bool initAccel = false;

    vk::WriteDescriptorSetAccelerationStructureKHR topLevelASDescriptor;
    vk::DescriptorBufferInfo verticesDescriptor;
    vk::DescriptorBufferInfo indicesDescriptor;

    VulkanBuffer materialsBuffer;
    vk::DescriptorBufferInfo materialsDescriptor;
    std::vector<VulkanGpuMaterial> gpuMaterials;
    std::vector<VulkanModelTexture> materialTextures;
    VulkanModelTexture fallbackBaseColorTexture;
    VulkanModelTexture fallbackNormalTexture;
    VulkanModelTexture fallbackMetallicRoughnessTexture;
    VulkanModelTexture fallbackEmissiveTexture;
    VulkanModelTexture fallbackOcclusionTexture;
    std::vector<vk::DescriptorImageInfo> baseColorTextureDescriptors;
    std::vector<vk::DescriptorImageInfo> normalTextureDescriptors;
    std::vector<vk::DescriptorImageInfo> metallicRoughnessTextureDescriptors;
    std::vector<vk::DescriptorImageInfo> emissiveTextureDescriptors;
    std::vector<vk::DescriptorImageInfo> occlusionTextureDescriptors;
    vk::DescriptorSet materialDescriptorSet = nullptr;
    vk::DescriptorSetLayout materialDescriptorSetLayout = nullptr;

    std::string debugName;

    static inline std::unordered_map<ModelCreateInfo, std::shared_ptr<VulkanModel>, ModelCreateInfoHash> ModelCache;
};


vk::Format component_format(Component component);

std::shared_ptr<VulkanModel> vulkan_model_load(VulkanContext& ctx, const ModelCreateInfo& createInfo);

void vulkan_model_destroy(VulkanContext& ctx, VulkanModel& model);
void vulkan_model_stage(VulkanContext& ctx, VulkanModel& model);
void vulkan_model_prepare_materials(VulkanContext& ctx, VulkanModel& model);
bool vulkan_model_prepare_material_descriptors(VulkanContext& ctx, VulkanModel& model, vk::DescriptorSetLayout layout);

std::shared_ptr<VulkanModel> vulkan_model_create(VulkanContext& ctx, VulkanScene& vulkanScene, const Geometry& geom);

} // namespace vulkan
