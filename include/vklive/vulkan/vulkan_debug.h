#pragma once

#include <string>
#include <glm/glm.hpp>

#include <zest/file/file.h>

#pragma warning(disable : 26812)
#include <vulkan/vulkan.hpp>
#pragma warning(default : 26812)

struct Scene;

namespace vulkan
{
struct VulkanContext;
struct VulkanPass;
struct VulkanSurface;

bool debug_init(VulkanContext& context);
void debug_destroy(VulkanContext& context);

void debug_init_markers(VkDevice device);
void debug_begin_region(VkCommandBuffer cmdbuffer, const std::string& pMarkerName, glm::vec4 color);
void debug_end_region(VkCommandBuffer cmdBuffer);
void debug_insert_marker(VkCommandBuffer cmdbuffer, const std::string& markerName, glm::vec4 color);
void debug_set_buffer_name(VkDevice device, VkBuffer buffer, const std::string& name);
void debug_set_commandbuffer_name(VkDevice device, VkCommandBuffer cmdBuffer, const std::string& name);
void debug_set_commandpool_name(VkDevice device, VkCommandPool cmdPool, const std::string& name);
void debug_set_descriptorpool_name(VkDevice device, VkDescriptorPool descriptorPool, const std::string& name);
void debug_set_descriptorset_name(VkDevice device, VkDescriptorSet descriptorSet, const std::string& name);
void debug_set_descriptorsetlayout_name(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const std::string& name);
void debug_set_device_name(VkDevice device, VkDevice d, const std::string& name);
void debug_set_devicememory_name(VkDevice device, VkDeviceMemory cache, const std::string& name);
void debug_set_event_name(VkDevice device, VkEvent _event, const std::string& name);
void debug_set_fence_name(VkDevice device, VkFence fence, const std::string& name);
void debug_set_framebuffer_name(VkDevice device, VkFramebuffer framebuffer, const std::string& name);
void debug_set_image_name(VkDevice device, VkImage image, const std::string& name);
void debug_set_imageview_name(VkDevice device, VkImageView imageView, const std::string& name);
void debug_set_instance_name(VkDevice device, VkInstance instance, const std::string& name);
void debug_set_object_name(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, const std::string& name);
void debug_set_object_tag(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag);
void debug_set_physicaldevice_name(VkDevice device, VkPhysicalDevice physicalDevice, const std::string& name);
void debug_set_pipeline_name(VkDevice device, VkPipeline pipeline, const std::string& name);
void debug_set_pipelinecache_name(VkDevice device, VkPipelineCache cache, const std::string& name);
void debug_set_pipelinelayout_name(VkDevice device, VkPipelineLayout pipelineLayout, const std::string& name);
void debug_set_queue_name(VkDevice device, VkQueue queue, const std::string& name);
void debug_set_renderpass_name(VkDevice device, VkRenderPass renderPass, const std::string& name);
void debug_set_sampler_name(VkDevice device, VkSampler sampler, const std::string& name);
void debug_set_semaphore_name(VkDevice device, VkSemaphore semaphore, const std::string& name);
void debug_set_shadermodule_name(VkDevice device, VkShaderModule shaderModule, const std::string& name);
void debug_set_surface_name(VkDevice device, VkSurfaceKHR surface, const std::string& name);
void debug_set_swapchain_name(VkDevice device, VkSwapchainKHR swap, const std::string& name);
void debug_set_surface_name(VkDevice device, VulkanSurface& surface, const std::string& name);

// Naming engine specific things
// Name a pass
std::string debug_get_pass_name(VulkanPass& vulkanPass, const std::string& postfix);

} // namespace vulkan
