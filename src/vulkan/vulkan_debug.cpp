#include <iostream>
#include <sstream>

#include <concurrentqueue/concurrentqueue.h>

#include "vklive/vulkan/vulkan_context.h"

#include "vklive/scene.h"
#include "vklive/string/string_utils.h"
#include "vklive/validation.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <mutex>
#include <set>
#include "vklive/logger/logger.h"

std::mutex mut;
std::map<std::string, std::string> Names;

// Debug
PFN_vkCreateDebugUtilsMessengerEXT pfnvkCreateDebugUtilsMessengerEXT = nullptr;
PFN_vkDestroyDebugUtilsMessengerEXT pfnvkDestroyDebugUtilsMessengerEXT = nullptr;

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger)
{
    return pfnvkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, VkAllocationCallbacks const* pAllocator)
{
    return pfnvkDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageFunc(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData, void* /*pUserData*/)
{
    std::ostringstream message;

    message << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) << ": "
            << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)) << ":\n";
    message << "\t"
            << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
    message << "\t"
            << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
    message << "\t"
            << "message         = <" << pCallbackData->pMessage << ">\n";
    if (0 < pCallbackData->queueLabelCount)
    {
        message << "\t"
                << "Queue Labels:\n";
        for (uint32_t i = 0; i < pCallbackData->queueLabelCount; i++)
        {
            message << "\t\t"
                    << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
        }
    }
    if (0 < pCallbackData->cmdBufLabelCount)
    {
        message << "\t"
                << "CommandBuffer Labels:\n";
        for (uint32_t i = 0; i < pCallbackData->cmdBufLabelCount; i++)
        {
            message << "\t\t"
                    << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
        }
    }
    if (0 < pCallbackData->objectCount)
    {
        message << "\t"
                << "Objects:\n";
        for (uint32_t i = 0; i < pCallbackData->objectCount; i++)
        {
            message << "\t\t"
                    << "Object " << i << "\n";
            message << "\t\t\t"
                    << "objectType   = " << vk::to_string(static_cast<vk::ObjectType>(pCallbackData->pObjects[i].objectType)) << "\n";
            message << "\t\t\t"
                    << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
            if (pCallbackData->pObjects[i].pObjectName)
            {
                message << "\t\t\t"
                        << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
            }
        }
    }

    if (messageSeverity == VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        std::string text = pCallbackData->pMessage;
        string_replace_in_place(text, ":", "\n");
        string_replace_in_place(text, "|", "\n");
        validation_error(text);

    }

    LOG(INFO, message.str().c_str());

    return false;
}

namespace vulkan
{
vk::DebugUtilsMessengerEXT debugUtilsMessenger = nullptr;

bool active = false;

PFN_vkDebugMarkerSetObjectTagEXT pfnDebugMarkerSetObjectTag = VK_NULL_HANDLE;
PFN_vkDebugMarkerSetObjectNameEXT pfnDebugMarkerSetObjectName = VK_NULL_HANDLE;
PFN_vkCmdDebugMarkerBeginEXT pfnCmdDebugMarkerBegin = VK_NULL_HANDLE;
PFN_vkCmdDebugMarkerEndEXT pfnCmdDebugMarkerEnd = VK_NULL_HANDLE;
PFN_vkCmdDebugMarkerInsertEXT pfnCmdDebugMarkerInsert = VK_NULL_HANDLE;

void debug_init_markers(VkDevice device)
{
    pfnDebugMarkerSetObjectTag = reinterpret_cast<PFN_vkDebugMarkerSetObjectTagEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectTagEXT"));
    pfnDebugMarkerSetObjectName = reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectNameEXT"));
    pfnCmdDebugMarkerBegin = reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT"));
    pfnCmdDebugMarkerEnd = reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT"));
    pfnCmdDebugMarkerInsert = reinterpret_cast<PFN_vkCmdDebugMarkerInsertEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT"));

    // Set flag if at least one function pointer is present
    active = (pfnDebugMarkerSetObjectName != VK_NULL_HANDLE);
}

const char* debug_insert_name(const std::string& str)
{
    const char* pszRet;
    mut.lock();
    auto itr = Names.find(str);
    if (itr == Names.end())
    {
        itr = Names.insert(std::make_pair(str, str)).first;
    }

    pszRet = itr->second.c_str();
    mut.unlock();
    return pszRet;
}

void debug_set_object_name(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, const std::string& name)
{
    // Check for valid function pointer (may not be present if not running in a debugging application)
    if (pfnDebugMarkerSetObjectName)
    {
        VkDebugMarkerObjectNameInfoEXT nameInfo = {};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = objectType;
        nameInfo.object = object;
        nameInfo.pObjectName = debug_insert_name(name);
        auto ret = pfnDebugMarkerSetObjectName(device, &nameInfo);
        assert(ret == VkResult::VK_SUCCESS);
    }
}

void debug_set_object_tag(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag)
{
    // Check for valid function pointer (may not be present if not running in a debugging application)
    if (pfnDebugMarkerSetObjectTag)
    {
        VkDebugMarkerObjectTagInfoEXT tagInfo = {};
        tagInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT;
        tagInfo.objectType = objectType;
        tagInfo.object = object;
        tagInfo.tagName = name;
        tagInfo.tagSize = tagSize;
        tagInfo.pTag = tag;
        pfnDebugMarkerSetObjectTag(device, &tagInfo);
    }
}

void debug_begin_region(VkCommandBuffer cmdbuffer, const std::string& pMarkerName, glm::vec4 color)
{
    // Check for valid function pointer (may not be present if not running in a debugging application)
    if (pfnCmdDebugMarkerBegin)
    {
        VkDebugMarkerMarkerInfoEXT markerInfo = {};
        markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
        memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
        markerInfo.pMarkerName = debug_insert_name(pMarkerName);
        pfnCmdDebugMarkerBegin(cmdbuffer, &markerInfo);
    }
}

void debug_insert_marker(VkCommandBuffer cmdbuffer, const std::string& markerName, glm::vec4 color)
{
    // Check for valid function pointer (may not be present if not running in a debugging application)
    if (pfnCmdDebugMarkerInsert)
    {
        VkDebugMarkerMarkerInfoEXT markerInfo = {};
        markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
        memcpy(markerInfo.color, &color[0], sizeof(float) * 4);
        markerInfo.pMarkerName = debug_insert_name(markerName);
        pfnCmdDebugMarkerInsert(cmdbuffer, &markerInfo);
    }
}

void debug_end_region(VkCommandBuffer cmdBuffer)
{
    // Check for valid function (may not be present if not running in a debugging application)
    if (pfnCmdDebugMarkerEnd)
    {
        pfnCmdDebugMarkerEnd(cmdBuffer);
    }
}

void debug_set_commandbuffer_name(VkDevice device, VkCommandBuffer cmdBuffer, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)cmdBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT, name);
}

void debug_set_commandpool_name(VkDevice device, VkCommandPool cmdBuffer, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)cmdBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT, name);
}

void debug_set_imageview_name(VkDevice device, VkImageView imageView, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)imageView, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, name);
}

void debug_set_queue_name(VkDevice device, VkQueue queue, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)queue, VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT, name);
}

void debug_set_image_name(VkDevice device, VkImage image, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)image, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, name);
}

void debug_set_sampler_name(VkDevice device, VkSampler sampler, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)sampler, VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT, name);
}

void debug_set_buffer_name(VkDevice device, VkBuffer buffer, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)buffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, name);
}

void debug_set_device_name(VkDevice device, VkDevice d, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)d, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT, name);
}

void debug_set_instance_name(VkDevice device, VkInstance instance, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)instance, VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT, name);
}

void debug_set_devicememory_name(VkDevice device, VkDeviceMemory memory, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)memory, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT, name);
}

void debug_set_shadermodule_name(VkDevice device, VkShaderModule shaderModule, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)shaderModule, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, name);
}

void debug_set_pipeline_name(VkDevice device, VkPipeline pipeline, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)pipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, name);
}

void debug_set_pipelinelayout_name(VkDevice device, VkPipelineLayout pipelineLayout, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)pipelineLayout, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, name);
}

void debug_set_renderpass_name(VkDevice device, VkRenderPass renderPass, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)renderPass, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, name);
}

void debug_set_framebuffer_name(VkDevice device, VkFramebuffer framebuffer, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)framebuffer, VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, name);
}

void debug_set_descriptorsetlayout_name(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)descriptorSetLayout, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, name);
}

void debug_set_descriptorset_name(VkDevice device, VkDescriptorSet descriptorSet, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)descriptorSet, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT, name);
}

void debug_set_semaphore_name(VkDevice device, VkSemaphore semaphore, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)semaphore, VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT, name);
}

void debug_set_fence_name(VkDevice device, VkFence fence, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)fence, VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT, name);
}

void debug_set_event_name(VkDevice device, VkEvent _event, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)_event, VK_DEBUG_REPORT_OBJECT_TYPE_EVENT_EXT, name);
}

void debug_set_swapchain_name(VkDevice device, VkSwapchainKHR swap, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)swap, VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT, name);
}

void debug_set_pipelinecache_name(VkDevice device, VkPipelineCache cache, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)cache, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_CACHE_EXT, name);
}

void debug_set_physicaldevice_name(VkDevice device, VkPhysicalDevice physicalDevice, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)physicalDevice, VK_DEBUG_REPORT_OBJECT_TYPE_PHYSICAL_DEVICE_EXT, name);
}

void debug_set_descriptorpool_name(VkDevice device, VkDescriptorPool descriptorPool, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)descriptorPool, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT, name);
}

void debug_set_surface_name(VkDevice device, VkSurfaceKHR surface, const std::string& name)
{
    debug_set_object_name(device, (uint64_t)surface, VK_DEBUG_REPORT_OBJECT_TYPE_SURFACE_KHR_EXT, name);
}

bool debug_init(VulkanContext& ctx)
{
    try
    {
        /* VULKAN_KEY_START */

        std::vector<vk::ExtensionProperties> props = vk::enumerateInstanceExtensionProperties();

        auto propertyIterator = std::find_if(
            props.begin(), props.end(), [](vk::ExtensionProperties const& ep) { return strcmp(ep.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0; });
        if (propertyIterator == props.end())
        {
            std::cout << "Something went very wrong, cannot find " << VK_EXT_DEBUG_UTILS_EXTENSION_NAME << " extension" << std::endl;
            return false;
        }

        pfnvkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(ctx.instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));
        if (!pfnvkCreateDebugUtilsMessengerEXT)
        {
            std::cout << "GetInstanceProcAddr: Unable to find pfnvkCreateDebugUtilsMessengerEXT function." << std::endl;
            return false;
        }

        pfnvkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(ctx.instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
        if (!pfnvkDestroyDebugUtilsMessengerEXT)
        {
            std::cout << "GetInstanceProcAddr: Unable to find pfnvkDestroyDebugUtilsMessengerEXT function." << std::endl;
            return false;
        }

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
        debugUtilsMessenger = ctx.instance.createDebugUtilsMessengerEXT(vk::DebugUtilsMessengerCreateInfoEXT({}, severityFlags, messageTypeFlags, &debugMessageFunc));

        /* VULKAN_KEY_END */
    }
    catch (vk::SystemError& err)
    {
        LOG(DBG, "vk::SystemError: " << err.what());
    }
    catch (std::exception& err)
    {
        LOG(DBG, "std::exception: " << err.what());
    }
    catch (...)
    {
        LOG(DBG, "unknown error");
    }
    return 0;
}

void debug_destroy(VulkanContext& ctx)
{
    if (debugUtilsMessenger)
    {
        ctx.instance.destroyDebugUtilsMessengerEXT(debugUtilsMessenger);
    }
}
} // namespace vulkan
