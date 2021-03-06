#pragma once

#include <glm/glm.hpp>
#include <gli/gli.hpp>

#pragma warning(disable : 26812)
#include <vulkan/vulkan.hpp>
#pragma warning(default : 26812)
#include <vklive/vulkan/vulkan_context.h>
#include <vklive/vulkan/vulkan_buffer.h>
#include <vklive/file/file.h>

struct Surface;

namespace vulkan
{

struct VulkanContext;

enum class VulkanAllocationState
{
    Init,
    Loaded,
    Failed
};

struct Allocation
{
    VulkanAllocationState allocationState = VulkanAllocationState::Init;
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
    explicit VulkanSurface(Surface* pS)
        : pSurface(pS)
    {
    }

    VulkanSurface()
    {
    }

    Surface* pSurface = nullptr;
    std::string debugName;
    glm::uvec2 currentSize;

    vk::Image image;
    vk::Extent3D extent;
    vk::ImageView view;
    uint32_t mipLevels = 1;
    uint32_t layerCount = 1;

    // For sampling the image in a shader
    vk::Sampler sampler;
    vk::DescriptorSetLayout samplerDescriptorSetLayout;
    vk::DescriptorSet samplerDescriptorSet;

    vk::Format format{ vk::Format::eUndefined };

    VulkanBuffer stagingBuffer;
    
};

using MipData = ::std::pair<vk::Extent3D, vk::DeviceSize>;

void surface_create(VulkanContext& ctx, VulkanSurface& vulkanImage, const vk::ImageCreateInfo& imageCreateInfo, const vk::MemoryPropertyFlags& memoryPropertyFlags);
void surface_create(VulkanContext& ctx, VulkanSurface& vulkanImage, const glm::uvec2& size, vk::Format colorFormat, bool sampled, const std::string& name);
void surface_create_depth(VulkanContext& ctx, VulkanSurface& vulkanImage, const glm::uvec2& size, vk::Format depthFormat, bool sampled, const std::string& name);

// void surface_set_layout(VulkanContext& ctx, vk::CommandBuffer const& commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);
void surface_unmap(VulkanContext& ctx, VulkanSurface& img);
void surface_destroy(VulkanContext& ctx, VulkanSurface& img);

void surface_set_layout(VulkanContext& ctx, vk::CommandBuffer cmdbuffer, vk::Image image, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout, vk::ImageSubresourceRange subresourceRange);
void surface_set_layout(VulkanContext& ctx, vk::CommandBuffer cmdbuffer, vk::Image image, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);
void surface_set_layout(VulkanContext& ctx, vk::CommandBuffer cmdbuffer, vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);
void surface_set_layout(VulkanContext& ctx, vk::Image image, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout, vk::ImageSubresourceRange subresourceRange);
void surface_set_layout(VulkanContext& ctx, vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);

void surface_set_sampling(VulkanContext& ctx, VulkanSurface& image);

void surface_stage_to_device(VulkanContext& ctx, VulkanSurface& surface, vk::ImageCreateInfo imageCreateInfo, const vk::MemoryPropertyFlags& memoryPropertyFlags, vk::DeviceSize size, const void* data, const std::vector<MipData>& mipData = {}, const vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);
void surface_stage_to_device(VulkanContext& ctx, VulkanSurface& surface, const vk::ImageCreateInfo& imageCreateInfo, const vk::MemoryPropertyFlags& memoryPropertyFlags, const gli::texture2d& tex2D, const vk::ImageLayout& layout);

template <typename T>
void surface_stage_to_device(VulkanContext& ctx, VulkanSurface& surface, const vk::ImageCreateInfo& imageCreateInfo, const vk::MemoryPropertyFlags& memoryPropertyFlags, const std::vector<T>& data)
{
    surface_stage_to_device(ctx, surface, imageCreateInfo, memoryPropertyFlags, data.size() * sizeof(T), (void*)data.data());
}

template <typename T>
void surface_stage_to_device(VulkanContext& ctx, VulkanSurface& surface, const vk::ImageCreateInfo& imageCreateInfo, const std::vector<T>& data)
{
    surface_stage_to_device(ctx, surface, imageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal, data.size() * sizeof(T), (void*)data.data());
}

void surface_create_from_file(VulkanContext& ctx, VulkanSurface& surface, const fs::path& filename, vk::Format format = vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlags imageUsageFlags = vk::ImageUsageFlagBits::eSampled, vk::ImageLayout imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal, bool forceLinear = false);
void surface_update_from_audio(VulkanContext& ctx, VulkanSurface& surface, bool& surfaceChanged);

} // namespace vulkan
