#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#pragma warning(disable : 26812)
#include <vulkan/vulkan.hpp>
#pragma warning(default : 26812)

#include <vklive/IDevice.h>
#include <vklive/vulkan/vulkan_debug.h>
#include <vklive/vulkan/vulkan_descriptor.h>
#include <vklive/vulkan/vulkan_scene.h>
#include <vklive/vulkan/vulkan_window.h>

namespace vulkan
{

struct VulkanImGuiTexture;
struct VulkanContext : DeviceContext
{
    // Members
    vk::DynamicLoader dl;
    vk::AllocationCallbacks allocator;
    vk::Instance instance;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    uint32_t graphicsQueue = (uint32_t)-1;
    uint32_t presentQueue = (uint32_t)-1;

    glm::uvec2 frameBufferSize;

    vk::PipelineCache pipelineCache;

    std::shared_ptr<VulkanImGuiTexture> spFontTexture;

    // Currently used by IMGui for the font.  Maybe factor this out later
    uint64_t descriptorCacheIndex = 0;
    vk::DescriptorPool descriptorPool;

    vk::DeviceSize BufferMemoryAlignment = 256;

    vk::PhysicalDeviceMemoryProperties memoryProperties;

    VulkanWindow mainWindowData;
    vk::SampleCountFlagBits MSAASamples = vk::SampleCountFlagBits::e1;

    bool swapChainRebuild = false;
    uint32_t minImageCount = 0;

    std::unordered_map<uint32_t, DescriptorCache> descriptorCache;

#ifdef WIN32
    static __declspec(thread) vk::CommandPool commandPool;
    static __declspec(thread) vk::Queue queue;
#else
    static thread_local vk::CommandPool commandPool;
    static thread_local vk::Queue queue;
#endif
    std::map<Scene*, std::shared_ptr<VulkanScene>> mapVulkanScene;

    std::vector<vk::LayerProperties> supportedInstancelayerProperties;
    
    std::vector<vk::ExtensionProperties> supportedInstanceExtensions;
    std::vector<const char*> requestedInstanceExtensions;
    std::vector<const char*> instanceExtensionNames;

    std::vector<vk::ExtensionProperties> supportedDeviceExtensions;
    std::vector<std::string> requestedDeviceExtensions;
    std::vector<std::string> deviceExtensionNames;

    std::vector<const char*> layerNames;

    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};

    VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferDeviceAddresFeatures{};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTracingPipelineFeatures{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerationStructureFeatures{};
};

bool context_init(VulkanContext& ctx);
void context_destroy(VulkanContext& ctx);
vk::Queue& context_get_queue(VulkanContext& ctx);

} // namespace vulkan
