#pragma once

#include <memory>

#include <vklive/vulkan/vulkan_shader.h>
#include <vklive/vulkan/vulkan_framebuffer.h>

#include <glm/glm.hpp>

struct Pass;

namespace vulkan
{

struct VulkanSurface;
struct VulkanPass;
struct VulkanScene;
struct VulkanPassSwapFrameData;

struct VulkanPassTargets
{
    // Image and depth for this pass
    std::map<std::string, VulkanSurface*> targets;
    //VulkanSurface* depth;

    // Increased every time the surfaces change
    std::map<VulkanSurface*, uint64_t> mapSurfaceGenerations;

    // Framebuffer size not necessarily same as target size
    glm::uvec2 currentFrameBufferSize = glm::uvec2(0);

    // Size of all targets (validated)
    glm::uvec2 targetSize = glm::uvec2(0);

    VulkanPassSwapFrameData* pFrameData = nullptr;

    // Framebuffer for these pass targets
    vk::Framebuffer frameBuffer;

    // Renderpass for this pass target
    // Note: this is dependent on the surface formats and their access masks, etc.
    // Probably not worth caching?  Just make one for each pass target
    // But this could be cached per access pattern style
    vk::RenderPass renderPass;

    // Debug name for these pass targets
    std::string debugName;
};

// Data associated with a given pass for a given swap frame.
// i.e. we allocate seperate resources for each flip buffer and for each pass
struct VulkanPassSwapFrameData
{
    VulkanPass* pVulkanPass = nullptr;

    // Pass targets for each ping pong....
    // This means we have 2 different sets of pass targets for each pass/swap.
    // We need this to ping/pong, but it means we have an extra layer of complexity to manage
    std::map<uint32_t, VulkanPassTargets> passTargets;

    VulkanBuffer vsUniform;

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
        alignas(16) glm::vec4 iSpectrumBands[2]; // 4 Audio spectrum bands, configured in the UI.
    
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
    
    vk::Pipeline geometryPipeline;
    vk::PipelineLayout geometryPipelineLayout;
    std::map<uint32_t, VulkanBindingSet> mergedBindingSets;

    // Descriptors built once
    bool builtDescriptors = false;
    std::map<uint32_t, vk::DescriptorSetLayout> descriptorSetLayouts;
    std::map<uint32_t, std::vector<vk::DescriptorSetLayoutBinding>> descriptorSetBindings;

    // Built each frame
    std::vector<vk::DescriptorSet> descriptorSets;

    // In flight stuff
    bool inFlight = false;
    vk::CommandBuffer commandBuffer;
    vk::CommandPool commandPool;
    vk::Fence fence;

    std::string debugName;
};

struct VulkanPass
{
    VulkanPass(VulkanScene& s, Pass& p)
        : vulkanScene(s),
        pass(p)
    {
    }

    VulkanScene& vulkanScene;
    Pass& pass;

    std::map<uint32_t, VulkanPassSwapFrameData> passFrameData;
};

std::shared_ptr<VulkanPass> vulkan_pass_create(VulkanScene& vulkanScene, Pass& pass);
void vulkan_pass_destroy(VulkanContext& ctx, VulkanPass& vulkanPass);
void vulkan_pass_wait(VulkanContext& ctx, VulkanPassSwapFrameData& passData);
bool vulkan_pass_draw(VulkanContext& ctx, VulkanPass& vulkanPass);

}