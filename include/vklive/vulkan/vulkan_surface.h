#pragma once

#include <glm/glm.hpp>
#include <gli/gli.hpp>

#include <zest/file/file.h>

#pragma warning(disable : 26812)
#include <vulkan/vulkan.hpp>
#pragma warning(default : 26812)
#include <vklive/vulkan/vulkan_context.h>
#include <vklive/vulkan/vulkan_buffer.h>

struct Surface;

namespace vulkan
{

struct VulkanContext;
struct VulkanScene;

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
    //void* mapped = nullptr;
    vk::MemoryPropertyFlags memoryPropertyFlags;
};

namespace VulkanSurfaceFlags
{
enum 
{
    Sampled = (1 << 0),
    Uploadable = (1 << 1)
};
}

// These structures mirror the scene structures and add the vulkan specific bits
// The vulkan objects should not live longer than the scene!
struct VulkanSurface : Allocation
{
    explicit VulkanSurface(Surface* pS)
        : pSurface(pS)
    {
    }

    Surface* pSurface = nullptr;
    std::string debugName;

    vk::Image image;

    vk::Image uploadImage;
    vk::DeviceMemory uploadMemory;
    vk::DeviceSize uploadAllocSize{ 0 };
    bool isBlitUpload = false;

    vk::Extent3D extent;
    vk::ImageView view;
    uint32_t mipLevels = 1;
    uint32_t layerCount = 1;

    // For sampling the image in a shader
    vk::Sampler sampler;

    vk::Format format{ vk::Format::eUndefined };

    VulkanBuffer stagingBuffer;

    uint64_t generation = 0;

    SurfaceKey key;

    // For UI read of this surface
    vk::DescriptorSet ImGuiDescriptorSet = nullptr;
    
};

inline std::ostream& operator<<(std::ostream& os, const VulkanSurface& surf)
{
    os << surf.key << " Image: " << surf.image << " (" << surf.extent.width << ", " << surf.extent.height << ")";
    return os;
}

std::string to_string(const VulkanSurface& surf);

using MipData = ::std::pair<vk::Extent3D, vk::DeviceSize>;

void vulkan_surface_create(VulkanContext& ctx, VulkanSurface& vulkanImage, const glm::uvec2& size, vk::Format colorFormat, uint32_t flags);
void vulkan_surface_create_depth(VulkanContext& ctx, VulkanSurface& vulkanImage, const glm::uvec2& size, vk::Format depthFormat);
void vulkan_surface_destroy(VulkanContext& ctx, VulkanSurface& img);

void surface_set_layout(VulkanContext& ctx, vk::CommandBuffer cmdbuffer, vk::Image image, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout, vk::ImageSubresourceRange subresourceRange);
void surface_set_layout(VulkanContext& ctx, vk::CommandBuffer cmdbuffer, vk::Image image, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);
void surface_set_layout(VulkanContext& ctx, vk::CommandBuffer cmdbuffer, vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);
void surface_set_layout(VulkanContext& ctx, vk::Image image, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout, vk::ImageSubresourceRange subresourceRange);
void surface_set_layout(VulkanContext& ctx, vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);

// Aren't these the same thing?
void surface_create_sampler(VulkanContext& ctx, VulkanSurface& surface);
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

bool surface_create_from_file(VulkanContext& ctx, VulkanSurface& surface, const fs::path& filename, vk::Format format = vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlags imageUsageFlags = vk::ImageUsageFlagBits::eSampled, vk::ImageLayout imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal, bool forceLinear = false);
bool surface_create_from_memory(VulkanContext& ctx, VulkanSurface& surface, const fs::path& filename, const char* pData, size_t data_size, vk::Format format = vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlags imageUsageFlags = vk::ImageUsageFlagBits::eSampled, vk::ImageLayout imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal, bool forceLinear = false);
void surface_update_from_audio(VulkanContext& ctx, VulkanSurface& surface, bool& surfaceChanged, vk::CommandBuffer& commandBuffer);

} // namespace vulkan
