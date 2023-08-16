#include <iostream>

#include <zest/logger/logger.h>

#include "imgui_impl_sdl2.h"
#include "vklive/vulkan/vulkan_context.h"
#include "vklive/vulkan/vulkan_utils.h"

#include "SDL2/SDL_vulkan.h"

#define IMGUI_VULKAN_DEBUG_REPORT

namespace vulkan
{
#ifdef WIN32
__declspec(thread) vk::CommandPool VulkanContext::commandPool;
__declspec(thread) vk::Queue VulkanContext::queue;
#else
thread_local vk::CommandPool VulkanContext::commandPool;
thread_local vk::Queue VulkanContext::queue;
#endif

bool context_init(VulkanContext& ctx)
{
    ctx.layerNames.clear();
    ctx.instanceExtensionNames.clear();

    // Setup Vulkan
    uint32_t extensions_count = 0;
    SDL_Vulkan_GetInstanceExtensions(ctx.window, &extensions_count, NULL);
    ctx.requestedInstanceExtensions.resize(extensions_count);
    SDL_Vulkan_GetInstanceExtensions(ctx.window, &extensions_count, ctx.requestedInstanceExtensions.data());

    static std::string AppName = "Demo";
    static std::string EngineName = "VkLive";

    ctx.supportedInstanceExtensions = vk::enumerateInstanceExtensionProperties();
    ctx.supportedInstancelayerProperties = vk::enumerateInstanceLayerProperties();

    // sort the extensions alphabetically

    std::sort(ctx.supportedInstanceExtensions.begin(),
        ctx.supportedInstanceExtensions.end(),
        [](vk::ExtensionProperties const& a, vk::ExtensionProperties const& b) { return strcmp(a.extensionName, b.extensionName) < 0; });

    LOG(DBG, "Instance Extensions:");
    for (auto const& ep : ctx.supportedInstanceExtensions)
    {
        LOG(DBG, ep.extensionName << ":");
        LOG(DBG, "\tVersion: " << ep.specVersion);
    }

    LOG(DBG, "Layer Properties:");
    for (auto const& l : ctx.supportedInstancelayerProperties)
    {
        LOG(DBG, l.layerName);
    }

    vk::InstanceCreateFlags flags;

    // initialize the vk::ApplicationInfo structure
    vk::ApplicationInfo applicationInfo(AppName.c_str(), 1, EngineName.c_str(), 1, VK_API_VERSION_1_2);

#ifdef __APPLE__
    flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
    ctx.extensionNames.push_back("VK_KHR_portability_enumeration");
#endif

#ifdef IMGUI_VULKAN_DEBUG_REPORT
    ctx.layerNames.push_back("VK_LAYER_KHRONOS_validation");
    ctx.requestedInstanceExtensions.push_back("VK_EXT_debug_utils");
#endif

    // Pick instance extensions
    for (auto& ext : ctx.requestedInstanceExtensions)
    {
        auto itr = std::find_if(ctx.supportedInstanceExtensions.begin(), ctx.supportedInstanceExtensions.end(), [&](auto val) {
            return (strcmp(val.extensionName, ext) == 0);
        });
        if (itr != ctx.supportedInstanceExtensions.end())
        {
            ctx.instanceExtensionNames.push_back(ext);
        }
        else
        {
            LOG(DBG, "Instance extension not available: " << ext);
        }
    }

    // create an Instance
    ctx.instance = vk::createInstance(vk::InstanceCreateInfo(flags, &applicationInfo, ctx.layerNames, ctx.instanceExtensionNames));

#ifdef IMGUI_VULKAN_DEBUG_REPORT
    debug_init(ctx);
#endif

    ctx.physicalDevice = ctx.instance.enumeratePhysicalDevices().front();
    for (auto& device : ctx.instance.enumeratePhysicalDevices())
    {
        if ((VkPhysicalDeviceType)device.getProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            // std::cerr << "Selected: " << device.getProperties().deviceName << "\n";
            ctx.physicalDevice = device;
        }
    }

    ctx.supportedDeviceExtensions = ctx.physicalDevice.enumerateDeviceExtensionProperties();
    LOG(DBG, "Device Extensions:");
    for (auto const& ep : ctx.supportedDeviceExtensions)
    {
        LOG(DBG, ep.extensionName << ":");
        LOG(DBG, "\tVersion: " << ep.specVersion);
    }

    // Ray tracing related extensions we'd like
    ctx.requestedDeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    ctx.requestedDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

    // Required by VK_KHR_acceleration_structure
    ctx.requestedDeviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    ctx.requestedDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    ctx.requestedDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

    // Required for VK_KHR_ray_tracing_pipeline
    ctx.requestedDeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);

    // Required by VK_KHR_spirv_1_4
    ctx.requestedDeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

    ctx.physicalDevice.getMemoryProperties(&ctx.memoryProperties);

    ctx.graphicsQueue = utils_find_queue(ctx, vk::QueueFlagBits::eGraphics);

    // create a Device
    float queuePriority = 0.0f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), static_cast<uint32_t>(ctx.graphicsQueue), 1, &queuePriority);

    vk::PhysicalDeviceFeatures features;
    // features.
#if WIN32
    features.geometryShader = true;
#endif

    auto required = utils_get_device_extensions();
    ctx.requestedDeviceExtensions.insert(ctx.requestedDeviceExtensions.end(), required.begin(), required.end());

    for (auto& ext : ctx.requestedDeviceExtensions)
    {
        auto itr = std::find_if(ctx.supportedDeviceExtensions.begin(), ctx.supportedDeviceExtensions.end(), [&](auto val) {
            return (strcmp(val.extensionName, ext.c_str()) == 0);
        });
        if (itr != ctx.supportedDeviceExtensions.end())
        {
            ctx.deviceExtensionNames.push_back(ext);
        }
        else
        {
            LOG(DBG, "Device extension not available: " << ext);
        }
    }

    // std::cerr << "Creating Device...";
    ctx.device = utils_create_device(ctx.physicalDevice, ctx.graphicsQueue, ctx.deviceExtensionNames, &features);

    debug_set_device_name(ctx.device, ctx.device, "Context::Device");
    debug_set_physicaldevice_name(ctx.device, ctx.physicalDevice, "Context::PhysicalDevice");
    debug_set_instance_name(ctx.device, ctx.instance, "Context::Instance");

    ctx.pipelineCache = ctx.device.createPipelineCache(vk::PipelineCacheCreateInfo());
    debug_set_pipelinecache_name(ctx.device, ctx.pipelineCache, "Context::PipelineCache");

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

    // Get ray tracing pipeline properties, which will be used later on
    ctx.rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 deviceProperties2{};
    deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties2.pNext = &ctx.rayTracingPipelineProperties;
    vkGetPhysicalDeviceProperties2(ctx.physicalDevice, &deviceProperties2);

    // Get acceleration structure properties, which will be used later on
    ctx.accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &ctx.accelerationStructureFeatures;
    vkGetPhysicalDeviceFeatures2(ctx.physicalDevice, &deviceFeatures2);
    
    // Get the ray tracing and accelertion structure related function pointers required by this sample
    ctx.vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(ctx.device, "vkGetBufferDeviceAddressKHR"));
    ctx.vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(ctx.device, "vkCmdBuildAccelerationStructuresKHR"));
    ctx.vkBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(ctx.device, "vkBuildAccelerationStructuresKHR"));
    ctx.vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(ctx.device, "vkCreateAccelerationStructureKHR"));
    ctx.vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(ctx.device, "vkDestroyAccelerationStructureKHR"));
    ctx.vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(ctx.device, "vkGetAccelerationStructureBuildSizesKHR"));
    ctx.vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(ctx.device, "vkGetAccelerationStructureDeviceAddressKHR"));
    ctx.vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(ctx.device, "vkCmdTraceRaysKHR"));
    ctx.vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(ctx.device, "vkGetRayTracingShaderGroupHandlesKHR"));
    ctx.vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(ctx.device, "vkCreateRayTracingPipelinesKHR"));

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
    ctx.instanceExtensionNames.clear();
}

vk::Queue& context_get_queue(VulkanContext& ctx)
{
    if (!ctx.queue)
    {
        ctx.queue = ctx.device.getQueue(ctx.graphicsQueue, 0);
        debug_set_queue_name(ctx.device, ctx.queue, "Context::Queue");
    }
    return ctx.queue;
}

} // namespace vulkan
