#pragma once

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

#include <glm/glm.hpp>
#include "vulkan_context.h"

namespace vulkan
{
// Utils
vk::Device utils_create_device(vk::PhysicalDevice const& physicalDevice, uint32_t queueFamilyIndex, std::vector<std::string> const& extensions = {}, vk::PhysicalDeviceFeatures const* physicalDeviceFeatures = nullptr, void const* pNext = nullptr);
std::vector<std::string> utils_get_device_extensions();

uint32_t utils_find_queue(VulkanContext& ctx, const vk::QueueFlags& desiredFlags, const vk::SurfaceKHR& presentSurface = nullptr);

vk::SurfaceFormatKHR utils_select_surface_format(VulkanContext& ctx, vk::SurfaceKHR surface, const std::vector<vk::Format>& request_formats, vk::ColorSpaceKHR request_color_space);
vk::AccessFlags utils_access_flags_for_layout(vk::ImageLayout layout);
vk::PipelineStageFlags utils_pipeline_stage_flags_for_layout(vk::ImageLayout layout);

vk::PresentModeKHR utils_select_present_mode(VulkanContext& ctx, vk::SurfaceKHR& surface, std::vector<vk::PresentModeKHR>& request_modes);
int utils_get_min_image_count_from_present_mode(vk::PresentModeKHR present_mode);


// Image
void utils_set_image_layout(VulkanContext& ctx, vk::CommandBuffer const& commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);

template <typename T>
VulkanImage image_stage_to_device(VulkanContext& ctx, const vk::ImageCreateInfo& imageCreateInfo, const vk::MemoryPropertyFlags& memoryPropertyFlags, const std::vector<T>& data)
{
    return image_stage_to_device(ctx, imageCreateInfo, memoryPropertyFlags, data.size() * sizeof(T), (void*)data.data());
}

template <typename T>
VulkanImage image_stage_to_device(VulkanContext& ctx, const vk::ImageCreateInfo& imageCreateInfo, const std::vector<T>& data)
{
    return image_stage_to_device(ctx, imageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal, data.size() * sizeof(T), (void*)data.data());
}

void utils_copy_to_memory(VulkanContext& ctx, const vk::DeviceMemory& memory, const void* data, vk::DeviceSize size, vk::DeviceSize offset = 0);

template <typename T>
void utils_copy_to_memory(VulkanContext& ctx, const vk::DeviceMemory& memory, const T& data, size_t offset = 0)
{
    utils_copy_to_memory(ctx, memory, &data, sizeof(T), offset);
}

template <typename T>
void utils_copy_to_memory(VulkanContext& ctx, const vk::DeviceMemory& memory, const std::vector<T>& data, size_t offset = 0)
{
    utils_copy_to_memory(ctx, memory, data.data(), data.size() * sizeof(T), offset);
}

uint32_t utils_memory_type(VulkanContext& ctx, vk::MemoryPropertyFlags properties, uint32_t type_bits);

// TODO
vk::Viewport viewport(float width, float height, float minDepth = 0, float maxDepth = 1);
vk::Viewport viewport(const glm::uvec2& size, float minDepth = 0, float maxDepth = 1);
vk::Viewport viewport(const vk::Extent2D& size, float minDepth = 0, float maxDepth = 1);
vk::Rect2D rect2d(uint32_t width, uint32_t height, int32_t offsetX = 0, int32_t offsetY = 0);
vk::Rect2D rect2d(const glm::uvec2& size, const glm::ivec2& offset = glm::ivec2(0));
vk::Rect2D rect2d(const vk::Extent2D& size, const vk::Offset2D& offset = vk::Offset2D());
vk::ColorComponentFlags full_color_writemask();
vk::ClearColorValue clear_color(const glm::vec4& v = glm::vec4(0));
} // namespace vulkan
