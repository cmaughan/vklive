#pragma once

#include <map>
#include <vector>
#include <mutex>
#include <memory>

#pragma warning(disable : 26812)
#include <vulkan/vulkan.hpp>
#pragma warning(default : 26812)

#include <vklive/vulkan/vulkan_window.h>
#include <vklive/vulkan/vulkan_debug.h>
#include <vklive/vulkan/vulkan_scene.h>
#include <vklive/vulkan/vulkan_descriptor.h>
#include <vklive/IDevice.h>

namespace vulkan
{

struct VulkanContext : DeviceContext
{
    // Members
    std::vector<const char*> extensionNames;
    std::vector<const char*> layerNames;

    vk::AllocationCallbacks allocator;
    vk::Instance instance;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::Queue queue;
    uint32_t graphicsQueue = (uint32_t)-1;
    uint32_t presentQueue = (uint32_t)-1;

    vk::PipelineCache pipelineCache;
    vk::DescriptorPool descriptorPool;

    vk::DeviceSize BufferMemoryAlignment = 256;

    vk::PhysicalDeviceMemoryProperties memoryProperties;

    VulkanWindow mainWindowData;
    vk::SampleCountFlagBits MSAASamples = vk::SampleCountFlagBits::e1;

    bool swapChainRebuild = false;
    uint32_t minImageCount = 0;

#ifdef WIN32
    static __declspec(thread) vk::CommandPool commandPool;
#else
    static thread_local vk::CommandPool commandPool;
#endif
    std::map<SceneGraph*, std::shared_ptr<VulkanScene>> mapVulkanScene;
};

bool context_init(VulkanContext& ctx);
void context_destroy(VulkanContext& ctx);

} // namespace vulkan
