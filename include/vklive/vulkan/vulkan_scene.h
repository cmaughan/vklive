#pragma once

#include <vklive/scene.h>
#include <vklive/IDevice.h>
#include <vklive/vulkan/vulkan_context.h>
#include <vklive/vulkan/vulkan_framebuffer.h>
#include <vklive/vulkan/vulkan_model.h>
#include <vklive/vulkan/vulkan_descriptor.h>

#include <vklive/camera.h>
namespace vulkan
{

struct RenderContext;

// These structures mirror the scene structures and add the vulkan specific bits
// The vulkan objects should not live longer than the scene!
struct VulkanSurface : IDeviceSurface
{
    VulkanSurface(const Surface* pS)
        : IDeviceSurface(pS)
    {
    }
    VulkanImage image;
    std::string debugName;
    glm::uvec2 currentSize;
};

struct VulkanGeometry
{
    VulkanGeometry(Geometry* pG)
        : pGeometry(pG)
    {
    }
    Geometry* pGeometry;
    VulkanModel model;
    std::string debugVertexName;
    std::string debugIndexName;
};

struct VulkanBindingSet
{
    std::map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
};

struct VulkanShader
{
    VulkanShader(Shader* pS)
        : pShader(pS)
    {
    }
    Shader* pShader;

    std::map<uint32_t, VulkanBindingSet> bindingSets;
    vk::PipelineShaderStageCreateInfo shaderCreateInfo;
};

struct VulkanPass
{
    VulkanPass(Pass* pP)
        : pPass(pP)
    {
    }

    Pass* pPass;

    VulkanFrameBuffer frameBuffer;

    glm::uvec2 currentFrameBufferSize = glm::uvec2(0);
    glm::uvec2 targetSize = glm::uvec2(0);

    vk::RenderPass renderPass;
    
    vk::Pipeline geometryPipeline;
    vk::PipelineLayout geometryPipelineLayout;

    std::vector<vk::DescriptorSet> descriptorSets;
    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;

    VulkanBuffer vsUniform;

    // Image and depth for this pass
    std::vector<VulkanImage*> colorImages;
    VulkanImage* pDepthImage = nullptr;

    struct Channel {
        alignas(16) glm::vec4 resolution;
        alignas(4) float time;
    };

    struct UBO
    {
        alignas(4) float iTime;            // Elapsed 
        alignas(4) float iGlobalTime;      // Elapsed (same as iTime)
        alignas(4) float iTimeDelta;       // Delta since last frame
        alignas(4) float iFrame;           // Number of frames drawn since begin
        alignas(4) float iFrameRate;       // 1 / Elapsed
        alignas(4) float iSampleRate;      // Sound sample rate
        alignas(16) glm::vec4 iResolution;  // Resolution of current target
        alignas(16) glm::vec4 iMouse;       // Mouse coords in pixels
        alignas(16) glm::vec4 iDate;        // Year, Month, Day, Seconds since epoch
    
        // Note originally an array of 4 floats; std140 alignment makes this tricky
        alignas(16) glm::vec4 iChannelTime; // Time for an input channel

        alignas(16) glm::vec4 iChannelResolution[4];    // Resolution for an input channel
        alignas(16) glm::vec4 ifFragCoordOffsetUniform; // ?
        alignas(16) glm::vec4 eye;                      // The eye in world space
        
        alignas(16) glm::mat4 model;                    // Transforms for camera based rendering
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 projection;
        alignas(16) glm::mat4 modelViewProjection;

        Channel iChannel[4];                // Packed version
        
    };
    UBO vsUBO;
};

struct VulkanScene
{
    VulkanScene(SceneGraph* pS)
        : pScene(pS)
    {
    }

    SceneGraph* pScene = nullptr;
    std::map<std::string, std::shared_ptr<VulkanSurface>> surfaces;
    std::map<fs::path, std::shared_ptr<VulkanGeometry>> geometries;
    std::map<fs::path, std::shared_ptr<VulkanShader>> shaderStages;
    std::map<std::string, std::shared_ptr<VulkanPass>> passes;
  
    DescriptorCache descriptorCache;

    bool inFlight = false;
    vk::CommandBuffer commandBuffer;
    vk::CommandPool commandPool;
    vk::Fence fence;
};

void vulkan_scene_init(VulkanContext& ctx, SceneGraph& scene);
void vulkan_scene_destroy(VulkanContext& ctx, SceneGraph& scene);
void vulkan_scene_render(VulkanContext& ctx, RenderContext& renderContext, SceneGraph& scene);
void vulkan_scene_prepare(VulkanContext& ctx, RenderContext& renderContext, SceneGraph& scene);
vk::Format vulkan_scene_format_to_vulkan(const Format& format);

} // namespace vulkan
