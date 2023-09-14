#pragma once

#include "vklive/model.h"

#include "vulkan_context.h"
#include "vulkan_buffer.h"

namespace vulkan
{

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

    std::string debugName;

    static inline std::unordered_map<ModelCreateInfo, std::shared_ptr<VulkanModel>, ModelCreateInfoHash> ModelCache;
};


vk::Format component_format(Component component);

std::shared_ptr<VulkanModel> vulkan_model_load(VulkanContext& ctx, const ModelCreateInfo& createInfo);

void vulkan_model_destroy(VulkanContext& ctx, VulkanModel& model);
void vulkan_model_stage(VulkanContext& ctx, VulkanModel& model);

std::shared_ptr<VulkanModel> vulkan_model_create(VulkanContext& ctx, VulkanScene& vulkanScene, const Geometry& geom);

} // namespace vulkan
