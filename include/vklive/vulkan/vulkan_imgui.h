#pragma once

#include <vklive/vulkan/vulkan_context.h>
#include <imgui.h>

struct SceneGraph;
struct ImDrawData;

namespace vulkan
{

// For multi-viewport support:
// Helper structure we store in the void* RenderUserData field of each ImGuiViewport to easily retrieve our backend data.
struct ImGuiViewportData
{
    bool windowOwned = false;
    VulkanWindow window; // Used by secondary viewports only
    WindowRenderBuffers renderBuffers; // Used by all viewports

    ImGuiViewportData() {}
    ~ImGuiViewportData() {}
};

struct ImGuiContext : IContextData
{
    vk::RenderPass renderPass;
    vk::DescriptorSetLayout descriptorSetLayout;
    vk::PipelineLayout pipelineLayout;
    vk::Pipeline pipeline;
    uint32_t subpass = 0;
    vk::ShaderModule shaderModuleVert;
    vk::ShaderModule shaderModuleFrag;

    // Font data
    vk::Sampler fontSampler;
    vk::DeviceMemory fontMemory;
    vk::Image fontImage;
    vk::ImageView fontView;
    vk::DescriptorSet fontDescriptorSet;
    vk::DeviceMemory uploadBufferMemory;
    vk::Buffer uploadBuffer;

    // Render buffers for main window
    WindowRenderBuffers mainWindowRenderBuffers;

    ImGuiContext()
    {
    }
};

// Imgui
VulkanContext& GetVulkanDevice();
std::shared_ptr<ImGuiContext> imgui_context(VulkanContext& ctx);

bool imgui_init(VulkanContext& ctx, const std::string& iniPath, bool viewports = false);
void imgui_render(VulkanContext& ctx, VulkanWindow* wd, ImDrawData* drawData);
void imgui_create_shaders(VulkanContext& ctx);
void imgui_destroy(VulkanContext& ctx);
void imgui_destroy_device_objects(VulkanContext& ctx);
void imgui_shutdown(VulkanContext& ctx);
void imgui_setup_renderstate(VulkanContext& ctx, ImDrawData* draw_data, vk::Pipeline pipeline, vk::CommandBuffer command_buffer, FrameRenderBuffers* rb, int fb_width, int fb_height);
void imgui_render_drawdata(VulkanContext& ctx, ImDrawData* draw_data, vk::CommandBuffer command_buffer, vk::Pipeline pipeline = nullptr);
void imgui_upload_font(VulkanContext& ctx);
void imgui_destroy_font_upload_objects(VulkanContext& ctx);
void imgui_viewport_destroy_all(VulkanContext& ctx);

// Viewports
void imgui_viewport_create(ImGuiViewport* viewport);
void imgui_viewport_destroy(ImGuiViewport* viewport);
void imgui_viewport_set_size(ImGuiViewport* viewport, ImVec2 size);
void imgui_viewport_render(ImGuiViewport* viewport, void*);
void imgui_viewport_swap_buffers(ImGuiViewport* viewport, void*);

void imgui_viewport_destroy_all(VulkanContext& ctx);

void imgui_render_3d(VulkanContext& ctx, SceneGraph& scene, bool background, const std::function<IDeviceSurface*(const glm::vec2&)>& fnDrawScene);

} // namespace vulkan
