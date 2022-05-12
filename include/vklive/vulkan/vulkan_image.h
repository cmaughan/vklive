#pragma once

#pragma warning(disable : 26812)
#include <vulkan/vulkan.hpp>
#pragma warning(default : 26812)
#include <vklive/vulkan/vulkan_context.h>
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

struct VulkanImage : Allocation
{
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

VulkanImage image_create(VulkanContext& ctx, const vk::ImageCreateInfo& imageCreateInfo, const vk::MemoryPropertyFlags& memoryPropertyFlags);
// void image_set_layout(VulkanContext& ctx, vk::CommandBuffer const& commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);
void image_unmap(VulkanContext& ctx, VulkanImage& img);
void image_destroy(VulkanContext& ctx, VulkanImage& img);
VulkanImage image_stage_to_device(VulkanContext& ctx, vk::ImageCreateInfo imageCreateInfo, const vk::MemoryPropertyFlags& memoryPropertyFlags, vk::DeviceSize size, const void* data, const std::vector<MipData>& mipData = {}, const vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);

void image_set_layout(VulkanContext& ctx, vk::CommandBuffer cmdbuffer, vk::Image image, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout, vk::ImageSubresourceRange subresourceRange);
void image_set_layout(VulkanContext& ctx, vk::CommandBuffer cmdbuffer, vk::Image image, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);
void image_set_layout(VulkanContext& ctx, vk::CommandBuffer cmdbuffer, vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);
void image_set_layout(VulkanContext& ctx, vk::Image image, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout, vk::ImageSubresourceRange subresourceRange);
void image_set_layout(VulkanContext& ctx, vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);

void image_set_sampling(VulkanContext& ctx, VulkanImage& image);
} // namespace vulkan
