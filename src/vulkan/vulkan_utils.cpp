// Copyright(c) 2019, NVIDIA CORPORATION. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#if defined(_MSC_VER)
// no need to ignore any warnings with MSVC
#elif defined(__clang__)
#pragma clang diagnostic ignored "-Wmissing-braces"
#elif defined(__GNUC__)
// no need to ignore any warnings with GCC
#else
// unknow compiler... just ignore the warnings for yourselves ;)
#endif

#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

#include <zest/logger/logger.h>

#include "vklive/vulkan/vulkan_context.h"
#include "vklive/vulkan/vulkan_surface.h"
#include "vklive/vulkan/vulkan_utils.h"

#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

namespace vulkan
{

uint32_t utils_memory_type(VulkanContext& ctx, vk::MemoryPropertyFlags properties, uint32_t type_bits)
{
    for (uint32_t i = 0; i < ctx.memoryProperties.memoryTypeCount; i++)
        if ((ctx.memoryProperties.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1 << i))
            return i;
    return 0xFFFFFFFF; // Unable to find memoryType
}

vk::ColorComponentFlags full_color_writemask()
{
    return vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
}

vk::ClearColorValue clear_color(const glm::vec4& v)
{
    vk::ClearColorValue result;
    memcpy(&result.float32, &v, sizeof(result.float32));
    return result;
}

vk::Viewport viewport(float width, float height, float minDepth, float maxDepth)
{
    vk::Viewport viewport;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
    return viewport;
}

vk::Viewport viewport(const glm::uvec2& size, float minDepth, float maxDepth)
{
    return viewport(static_cast<float>(size.x), static_cast<float>(size.y), minDepth, maxDepth);
}

vk::Viewport viewport(const vk::Extent2D& size, float minDepth, float maxDepth)
{
    return viewport(static_cast<float>(size.width), static_cast<float>(size.height), minDepth, maxDepth);
}

vk::Rect2D rect2d(uint32_t width, uint32_t height, int32_t offsetX, int32_t offsetY)
{
    vk::Rect2D rect2D;
    rect2D.extent.width = width;
    rect2D.extent.height = height;
    rect2D.offset.x = offsetX;
    rect2D.offset.y = offsetY;
    return rect2D;
}

vk::Rect2D rect2d(const glm::uvec2& size, const glm::ivec2& offset)
{
    return rect2d(size.x, size.y, offset.x, offset.y);
}

vk::Rect2D rect2d(const vk::Extent2D& size, const vk::Offset2D& offset)
{
    return rect2d(size.width, size.height, offset.x, offset.y);
}

vk::AccessFlags accessFlagsForLayout(vk::ImageLayout layout)
{
    switch (layout)
    {
    case vk::ImageLayout::ePreinitialized:
        return vk::AccessFlagBits::eHostWrite;
    case vk::ImageLayout::eTransferDstOptimal:
        return vk::AccessFlagBits::eTransferWrite;
    case vk::ImageLayout::eTransferSrcOptimal:
        return vk::AccessFlagBits::eTransferRead;
    case vk::ImageLayout::eColorAttachmentOptimal:
        return vk::AccessFlagBits::eColorAttachmentWrite;
    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        return vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    case vk::ImageLayout::eShaderReadOnlyOptimal:
        return vk::AccessFlagBits::eShaderRead;
    default:
        return vk::AccessFlags();
    }
}

inline vk::PipelineStageFlags pipelineStageForLayout(vk::ImageLayout layout)
{
    switch (layout)
    {
    case vk::ImageLayout::eTransferDstOptimal:
    case vk::ImageLayout::eTransferSrcOptimal:
        return vk::PipelineStageFlagBits::eTransfer;

    case vk::ImageLayout::eColorAttachmentOptimal:
        return vk::PipelineStageFlagBits::eColorAttachmentOutput;

    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        return vk::PipelineStageFlagBits::eEarlyFragmentTests;

    case vk::ImageLayout::eShaderReadOnlyOptimal:
        return vk::PipelineStageFlagBits::eFragmentShader;

    case vk::ImageLayout::ePreinitialized:
        return vk::PipelineStageFlagBits::eHost;

    case vk::ImageLayout::eUndefined:
        return vk::PipelineStageFlagBits::eTopOfPipe;

    default:
        return vk::PipelineStageFlagBits::eBottomOfPipe;
    }
}

inline vk::ClearColorValue clearColor(const glm::vec4& v)
{
    vk::ClearColorValue result;
    memcpy(&result.float32, &v, sizeof(result.float32));
    return result;
}

void utils_copy_to_memory(VulkanContext& ctx, const vk::DeviceMemory& memory, const void* data, vk::DeviceSize size, vk::DeviceSize offset)
{
    void* mapped = ctx.device.mapMemory(memory, offset, size, vk::MemoryMapFlags());
    memcpy(mapped, data, size);
    ctx.device.unmapMemory(memory);
}

template <typename T>
void utils_copy_to_memory(const vk::DeviceMemory& memory, const T& data, size_t offset = 0)
{
    utils_copy_to_memory(memory, &data, sizeof(T), offset);
}

template <typename T>
void utils_copy_to_memory(const vk::DeviceMemory& memory, const std::vector<T>& data, size_t offset = 0)
{
    utils_copy_to_memory(memory, data.data(), data.size() * sizeof(T), offset);
}

vk::AccessFlags utils_access_flags_for_layout(vk::ImageLayout layout)
{
    switch (layout)
    {
    case vk::ImageLayout::ePreinitialized:
        return vk::AccessFlagBits::eHostWrite;
    case vk::ImageLayout::eTransferDstOptimal:
        return vk::AccessFlagBits::eTransferWrite;
    case vk::ImageLayout::eTransferSrcOptimal:
        return vk::AccessFlagBits::eTransferRead;
    case vk::ImageLayout::eColorAttachmentOptimal:
        return vk::AccessFlagBits::eColorAttachmentWrite;
    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        return vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    case vk::ImageLayout::eShaderReadOnlyOptimal:
        return vk::AccessFlagBits::eShaderRead;
    default:
        return vk::AccessFlags();
    }
}

vk::PipelineStageFlags utils_pipeline_stage_flags_for_layout(vk::ImageLayout layout)
{
    switch (layout)
    {
    case vk::ImageLayout::eTransferDstOptimal:
    case vk::ImageLayout::eTransferSrcOptimal:
        return vk::PipelineStageFlagBits::eTransfer;

    case vk::ImageLayout::eColorAttachmentOptimal:
        return vk::PipelineStageFlagBits::eColorAttachmentOutput;

    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        return vk::PipelineStageFlagBits::eEarlyFragmentTests;

    case vk::ImageLayout::eShaderReadOnlyOptimal:
        return vk::PipelineStageFlagBits::eFragmentShader;

    case vk::ImageLayout::ePreinitialized:
        return vk::PipelineStageFlagBits::eHost;

    case vk::ImageLayout::eUndefined:
        return vk::PipelineStageFlagBits::eTopOfPipe;

    default:
        return vk::PipelineStageFlagBits::eBottomOfPipe;
    }
}

uint32_t utils_find_queue(VulkanContext& ctx, const vk::QueueFlags& desiredFlags, const vk::SurfaceKHR& presentSurface)
{
    auto queueFamilyProperties = ctx.physicalDevice.getQueueFamilyProperties();

    uint32_t bestMatch{ VK_QUEUE_FAMILY_IGNORED };
    VkQueueFlags bestMatchExtraFlags{ VK_QUEUE_FLAG_BITS_MAX_ENUM };
    size_t queueCount = queueFamilyProperties.size();
    for (uint32_t i = 0; i < queueCount; ++i)
    {
        auto currentFlags = queueFamilyProperties[i].queueFlags;
        // Doesn't contain the required flags, skip it
        if (!(currentFlags & desiredFlags))
        {
            continue;
        }

        if (presentSurface && VK_FALSE == ctx.physicalDevice.getSurfaceSupportKHR(i, presentSurface))
        {
            continue;
        }
        VkQueueFlags currentExtraFlags = (currentFlags & ~desiredFlags).operator VkQueueFlags();

        // If we find an exact match, return immediately
        if (0 == currentExtraFlags)
        {
            return i;
        }

        if (bestMatch == VK_QUEUE_FAMILY_IGNORED || currentExtraFlags < bestMatchExtraFlags)
        {
            bestMatch = i;
            bestMatchExtraFlags = currentExtraFlags;
        }
    }

    return bestMatch;
}

vk::Device utils_create_device(vk::PhysicalDevice const& physicalDevice, uint32_t queueFamilyIndex, std::vector<std::string> const& extensions, vk::PhysicalDeviceFeatures const* physicalDeviceFeatures, void const* pNext)
{
    std::vector<char const*> enabledExtensions;
    enabledExtensions.reserve(extensions.size());
    for (auto const& ext : extensions)
    {
        enabledExtensions.push_back(ext.data());
    }

    float queuePriority = 0.0f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo({}, queueFamilyIndex, 1, &queuePriority);
    vk::DeviceCreateInfo deviceCreateInfo({}, deviceQueueCreateInfo, {}, enabledExtensions, physicalDeviceFeatures);
    deviceCreateInfo.pNext = pNext;

    vk::Device device = physicalDevice.createDevice(deviceCreateInfo);
#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
    // initialize function pointers for instance
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
#endif

    debug_init_markers(device);

    return device;
}

std::vector<std::string> utils_get_device_extensions()
{
#ifdef __APPLE__
    return { VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset" };
#else
    return { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
#endif
}

vk::SurfaceFormatKHR utils_select_surface_format(VulkanContext& ctx, vk::SurfaceKHR surface, const std::vector<vk::Format>& request_formats, vk::ColorSpaceKHR request_color_space)
{
    assert(!request_formats.empty());

    // Per Spec Format and View Format are expected to be the same unless VK_IMAGE_CREATE_MUTABLE_BIT was set at image creation
    // Assuming that the default behavior is without setting this bit, there is no need for separate Swapchain image and image view format
    // Additionally several new color spaces were introduced with Vulkan Spec v1.0.40,
    // hence we must make sure that a format with the mostly available color space, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, is found and used.
    auto avail_formats = ctx.physicalDevice.getSurfaceFormatsKHR(surface);

    // First check if only one format, VK_FORMAT_UNDEFINED, is available, which would imply that any format is available
    if (avail_formats.size() == 1)
    {
        if (avail_formats[0].format == vk::Format::eUndefined)
        {
            vk::SurfaceFormatKHR ret;
            ret.format = request_formats[0];
            ret.colorSpace = request_color_space;
            return ret;
        }
        else
        {
            // No point in searching another format
            return avail_formats[0];
        }
    }
    else
    {
        // Request several formats, the first found will be used
        for (auto& request_format : request_formats)
        {
            for (auto& avail_format : avail_formats)
            {
                if (avail_format.format == request_format && avail_format.colorSpace == request_color_space)
                {
                    return avail_format;
                }
            }
        }

        // If none of the requested image formats could be found, use the first available
        return avail_formats[0];
    }
}

vk::PresentModeKHR utils_select_present_mode(VulkanContext& ctx, vk::SurfaceKHR& surface, std::vector<vk::PresentModeKHR>& request_modes)
{
    assert(!request_modes.empty());

    // Request a certain mode and confirm that it is available. If not use VK_PRESENT_MODE_FIFO_KHR which is mandatory
    auto modes = ctx.physicalDevice.getSurfacePresentModesKHR(surface);
    for (auto& request_mode : request_modes)
    {
        for (auto& mode : modes)
        {
            if (request_mode == mode)
            {
                return request_mode;
            }
        }
    }

    // Always available
    return vk::PresentModeKHR::eFifo;
}

int utils_get_min_image_count_from_present_mode(vk::PresentModeKHR present_mode)
{
    if (present_mode == vk::PresentModeKHR::eMailbox)
    {
        return 3;
    }

    if (present_mode == vk::PresentModeKHR::eFifo || present_mode == vk::PresentModeKHR::eFifoRelaxed)
    {
        return 2;
    }

    if (present_mode == vk::PresentModeKHR::eImmediate)
    {
        return 1;
    }
    assert(0);
    return 1;
}

vk::Format utils_format_to_vulkan(const Format& format)
{
    switch (format)
    {
    case Format::d32:
    case Format::default_depth_format:
        return vk::Format::eD32Sfloat;
    case Format::default_format:
    case Format::r8g8b8a8_unorm:
        return vk::Format::eR8G8B8A8Unorm;
    case Format::r16g16b16a16_sfloat:
        return vk::Format::eR16G16B16A16Sfloat;
    case Format::r32g32b32a32_sfloat:
        return vk::Format::eR32G32B32A32Sfloat;
    }
    assert(!"Unknown format?");
    return vk::Format::eR8G8B8A8Unorm;
}

bool vulkan_format_is_depth(const vk::Format& format)
{
    switch (format)
    {
    case vk::Format::eD16Unorm:
    case vk::Format::eD16UnormS8Uint:
    case vk::Format::eD24UnormS8Uint:
    case vk::Format::eD32Sfloat:
    case vk::Format::eD32SfloatS8Uint:
        return true;
    default:
        return false;
    }
}

uint32_t aligned_size(uint32_t value, uint32_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace vulkan
