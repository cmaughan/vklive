#pragma once

#include "vulkan_surface.h"

namespace vulkan
{
struct VulkanFrameSemaphores
{
    vk::Semaphore imageAcquiredSemaphore;
    vk::Semaphore renderCompleteSemaphore;
};

struct VulkanSwapFrame
{
    vk::CommandPool commandPool;
    vk::CommandBuffer commandBuffer;
    vk::Fence fence;
    std::vector<VulkanSurface> colorBuffers; // For multiple color target rendering
    VulkanSurface depthbuffer; // Might be null

    vk::Framebuffer framebuffer;
};

// Reusable buffers used for rendering 1 current in-flight frame, for ImGui2_ImplVulkan_RenderDrawData()
// [Please zero-clear before use!]
struct FrameRenderBuffers
{
    vk::DeviceMemory vertexBufferMemory;
    vk::DeviceMemory indexBufferMemory;
    vk::DeviceSize vertexBufferSize;
    vk::DeviceSize indexBufferSize;
    vk::Buffer vertexBuffer;
    vk::Buffer indexBuffer;
};

// Each viewport will hold 1 ImGui2_ImplVulkanH_WindowRenderBuffers
// [Please zero-clear before use!]
struct WindowRenderBuffers
{
    uint32_t index = 0;
    uint32_t count = 0;
    FrameRenderBuffers* frameRenderBuffers = nullptr;
};

struct VulkanWindow
{
    int width = 0;
    int height = 0;
    vk::SwapchainKHR swapchain;
    vk::SurfaceKHR surface;
    vk::SurfaceFormatKHR surfaceFormat;
    vk::PresentModeKHR presentMode = vk::PresentModeKHR::eImmediate;
    vk::RenderPass renderPass;
    vk::Pipeline pipeline; // The window pipeline may uses a different vk::RenderPass than the one passed in ImGui2_ImplVulkan_InitInfo
    bool clearEnable = true;
    vk::ClearValue clearValue;
    uint32_t frameIndex = 0; // Current frame being rendered to (0 <= FrameIndex < FrameInFlightCount)
    uint32_t imageCount = 0; // Number of simultaneous in-flight frames (returned by vkGetSwapchainImagesKHR, usually derived from min_image_count)
    uint32_t semaphoreCount = 0; // Number of simultaneous in-flight frames (returned by vkGetSwapchainImagesKHR, usually derived from min_image_count)
    uint32_t semaphoreIndex = 0; // Current set of swapchain wait semaphores we're using (needs to be distinct from per frame data)
    VulkanSwapFrame* frames = nullptr;
    VulkanFrameSemaphores* frameSemaphores = nullptr;

    VulkanWindow()
    {
    }
};

struct VulkanContext;

bool main_window_init(VulkanContext& ctx);
void main_window_validate_swapchain(VulkanContext& ctx);
void main_window_present(VulkanContext& ctx);
VulkanSurface* main_window_current_swap_image(VulkanContext& ctx);

void window_destroy(VulkanContext& ctx, VulkanWindow* wd);

void window_destroy_frame(VulkanContext& ctx, VulkanSwapFrame* fd);
void window_destroy_frame_semaphores(VulkanContext& ctx, VulkanFrameSemaphores* fsd);
void window_destroy_frame_renderbuffers(VulkanContext& ctx, FrameRenderBuffers* buffers);
void window_destroy_renderbuffers(VulkanContext& ctx, WindowRenderBuffers* buffers);
void window_create_or_resize(VulkanContext& ctx, VulkanWindow* wd, int width, int height);
} // namespace vulkan
