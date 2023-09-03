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
    VulkanModel(const Geometry& geom)
        : geometry(geom)
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

    const Geometry& geometry;
    std::string debugName;
};

vk::Format component_format(Component component);

void vulkan_model_load(VulkanContext& ctx,
    VulkanModel& model,
    const std::string& filename,
    const VertexLayout& layout,
    const glm::vec3& scale = glm::vec3(1.0f),
    const int flags = DefaultModelFlags);

void vulkan_model_load(VulkanContext& ctx,
    VulkanModel& model,
    const std::string& filename,
    const VertexLayout& layout,
    const ModelCreateInfo& createInfo,
    const int flags);

void vulkan_model_destroy(VulkanContext& ctx, VulkanModel& model);
void vulkan_model_stage(VulkanContext& ctx, VulkanModel& model);

std::shared_ptr<VulkanModel> vulkan_model_create(VulkanContext& ctx, VulkanScene& vulkanScene, const Geometry& geom);

extern VertexLayout g_vertexLayout;

} // namespace vulkan
