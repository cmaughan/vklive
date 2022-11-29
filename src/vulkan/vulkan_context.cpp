#include <iostream>

#include "imgui_impl_sdl.h"
#include "vklive/vulkan/vulkan_context.h"
#include "vklive/vulkan/vulkan_utils.h"

#include "SDL2/SDL_vulkan.h"

#include "vklive/logger/logger.h"

#define IMGUI_VULKAN_DEBUG_REPORT

namespace vulkan
{
#ifdef WIN32
__declspec(thread) vk::CommandPool VulkanContext::commandPool;
#else
thread_local vk::CommandPool VulkanContext::commandPool;
#endif

bool context_init(VulkanContext& ctx)
{
    // Setup Vulkan
    uint32_t extensions_count = 0;
    SDL_Vulkan_GetInstanceExtensions(ctx.window, &extensions_count, NULL);
    ctx.extensionNames.resize(extensions_count);
    SDL_Vulkan_GetInstanceExtensions(ctx.window, &extensions_count, ctx.extensionNames.data());

    static std::string AppName = "Demo";
    static std::string EngineName = "VkLive";

    std::vector<vk::ExtensionProperties> extensionProperties = vk::enumerateInstanceExtensionProperties();
    std::vector<vk::LayerProperties> layerProperties = vk::enumerateInstanceLayerProperties();

    // sort the extensions alphabetically

    std::sort(extensionProperties.begin(),
        extensionProperties.end(),
        [](vk::ExtensionProperties const& a, vk::ExtensionProperties const& b) { return strcmp(a.extensionName, b.extensionName) < 0; });

    LOG(DBG, "Instance Extensions:");
    for (auto const& ep : extensionProperties)
    {
        LOG(DBG, ep.extensionName << ":");
        LOG(DBG, "\tVersion: " << ep.specVersion);
    }

    LOG(DBG, "Layer Properties:");
    for (auto const& l : layerProperties)
    {
        LOG(DBG, l.layerName);
    }

    ctx.layerNames.clear();

    vk::InstanceCreateFlags flags;

    // initialize the vk::ApplicationInfo structure
    vk::ApplicationInfo applicationInfo(AppName.c_str(), 1, EngineName.c_str(), 1, VK_API_VERSION_1_2);
    
#ifdef __APPLE__
    flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
    ctx.extensionNames.push_back("VK_KHR_portability_enumeration");
#endif

#ifdef IMGUI_VULKAN_DEBUG_REPORT
    ctx.layerNames.push_back("VK_LAYER_KHRONOS_validation");
    ctx.extensionNames.push_back("VK_EXT_debug_utils");
#endif

    // create an Instance
    ctx.instance = vk::createInstance(vk::InstanceCreateInfo(flags, &applicationInfo, ctx.layerNames, ctx.extensionNames));

#ifdef IMGUI_VULKAN_DEBUG_REPORT
    debug_init(ctx);
#endif

    ctx.physicalDevice = ctx.instance.enumeratePhysicalDevices().front();
    for (auto& device : ctx.instance.enumeratePhysicalDevices())
    {
        if ((VkPhysicalDeviceType)device.getProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            //std::cerr << "Selected: " << device.getProperties().deviceName << "\n";
            ctx.physicalDevice = device;
        }
    }

    ctx.physicalDevice.getMemoryProperties(&ctx.memoryProperties);

    ctx.graphicsQueue = utils_find_queue(ctx, vk::QueueFlagBits::eGraphics);

    // create a Device
    float queuePriority = 0.0f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), static_cast<uint32_t>(ctx.graphicsQueue), 1, &queuePriority);

    vk::PhysicalDeviceFeatures features;
    //features.
#if WIN32
    features.geometryShader = true;
#endif
            
    //std::cerr << "Creating Device...";
    ctx.device = utils_create_device(ctx.physicalDevice, ctx.graphicsQueue, utils_get_device_extensions(), &features);

    debug_set_device_name(ctx.device, ctx.device, "Context::Device");
    debug_set_physicaldevice_name(ctx.device, ctx.physicalDevice, "Context::PhysicalDevice");
    debug_set_instance_name(ctx.device, ctx.instance, "Context::Instance");

    ctx.pipelineCache = ctx.device.createPipelineCache(vk::PipelineCacheCreateInfo());
    debug_set_pipelinecache_name(ctx.device, ctx.pipelineCache, "Context::PipelineCache");

    ctx.queue = ctx.device.getQueue(ctx.graphicsQueue, 0);
    debug_set_queue_name(ctx.device, ctx.queue, "Context::Queue");

    // Create Descriptor Pool
    {
        std::vector<vk::DescriptorPoolSize> pool_sizes = {
            { vk::DescriptorType::eSampler, 1000 },
            { vk::DescriptorType::eSampledImage, 1000 },
            { vk::DescriptorType::eCombinedImageSampler, 1000 },
            { vk::DescriptorType::eStorageImage, 1000 },
            { vk::DescriptorType::eUniformTexelBuffer, 1000 },
            { vk::DescriptorType::eStorageTexelBuffer, 1000 },
            { vk::DescriptorType::eUniformBuffer, 1000 },
            { vk::DescriptorType::eStorageImage, 1000 },
            { vk::DescriptorType::eStorageBuffer, 1000 },
            { vk::DescriptorType::eUniformBufferDynamic, 1000 },
            { vk::DescriptorType::eStorageBufferDynamic, 1000 },
            { vk::DescriptorType::eInputAttachment, 1000 }
        };
        vk::DescriptorPoolCreateInfo pool_info(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1000 * pool_sizes.size(), pool_sizes);
        ctx.descriptorPool = ctx.device.createDescriptorPool(pool_info);
        debug_set_descriptorpool_name(ctx.device, ctx.descriptorPool, "Context::DescriptorPool(ImGui)");
    }

    return true;
}

void context_destroy(VulkanContext& ctx)
{
    ctx.device.destroyDescriptorPool(ctx.descriptorPool);
    ctx.descriptorPool = nullptr;
    
    ctx.device.destroyPipelineCache(ctx.pipelineCache);
    ctx.pipelineCache = nullptr;

    ctx.device.destroyCommandPool(ctx.commandPool);
    ctx.commandPool = nullptr;

#ifdef IMGUI_VULKAN_DEBUG_REPORT
    debug_destroy(ctx);
#endif // IMGUI_VULKAN_DEBUG_REPORT

    ctx.physicalDevice = nullptr;

    ctx.graphicsQueue = 0;
    ctx.queue = nullptr;

    ctx.device.destroy();
    ctx.device = nullptr;

    ctx.instance.destroy();
    ctx.instance = nullptr;
    
    ctx.layerNames.clear();
    ctx.extensionNames.clear();
}

} // namespace vulkan
