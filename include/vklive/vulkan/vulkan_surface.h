#pragma once

#include <glm/glm.hpp>

#pragma warning(disable : 26812)
#include <vulkan/vulkan.hpp>
#pragma warning(default : 26812)
#include <vklive/vulkan/vulkan_context.h>

struct Surface;

namespace vulkan
{

struct VulkanContext;

struct Allocation
{
    vk::DeviceMemory memory;
    vk::DeviceSize size{ 0 };
    vk::DeviceSize alignment{ 0 };
    vk::DeviceSize allocSize{ 0 };
    void* mapped = nullptr;
    vk::MemoryPropertyFlags memoryPropertyFlags;
};

// These structures mirror the scene structures and add the vulkan specific bits
// The vulkan objects should not live longer than the scene!
struct VulkanSurface : Allocation
{
    VulkanSurface(Surface* pS)
        : pSurface(pS)
    {
    }

    Surface* pSurface;
    std::string debugName;
    glm::uvec2 currentSize;

    vk::Image image;
    vk::Extent3D extent;
    vk::ImageView view;

    // For sampling the image in a shader
    vk::Sampler sampler;
    vk::DescriptorSetLayout samplerDescriptorSetLayout;
    vk::DescriptorSet samplerDescriptorSet;

    vk::Format format{ vk::Format::eUndefined };

};

using MipData = ::std::pair<vk::Extent3D, vk::DeviceSize>;

void surface_create(VulkanContext& ctx, VulkanSurface& vulkanImage, const vk::ImageCreateInfo& imageCreateInfo, const vk::MemoryPropertyFlags& memoryPropertyFlags);
void surface_create(VulkanContext& ctx, VulkanSurface& vulkanImage, const glm::uvec2& size, vk::Format colorFormat, bool sampled, const std::string& name);
void surface_create_depth(VulkanContext& ctx, VulkanSurface& vulkanImage, const glm::uvec2& size, vk::Format depthFormat, bool sampled, const std::string& name);

// void surface_set_layout(VulkanContext& ctx, vk::CommandBuffer const& commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);
void surface_unmap(VulkanContext& ctx, VulkanSurface& img);
void surface_destroy(VulkanContext& ctx, VulkanSurface& img);
//VulkanSurface surface_stage_to_device(VulkanContext& ctx, vk::ImageCreateInfo imageCreateInfo, const vk::MemoryPropertyFlags& memoryPropertyFlags, vk::DeviceSize size, const void* data, const std::vector<MipData>& mipData = {}, const vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);

void surface_set_layout(VulkanContext& ctx, vk::CommandBuffer cmdbuffer, vk::Image image, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout, vk::ImageSubresourceRange subresourceRange);
void surface_set_layout(VulkanContext& ctx, vk::CommandBuffer cmdbuffer, vk::Image image, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);
void surface_set_layout(VulkanContext& ctx, vk::CommandBuffer cmdbuffer, vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);
void surface_set_layout(VulkanContext& ctx, vk::Image image, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout, vk::ImageSubresourceRange subresourceRange);
void surface_set_layout(VulkanContext& ctx, vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);

void surface_set_sampling(VulkanContext& ctx, VulkanSurface& image);
} // namespace vulkan
