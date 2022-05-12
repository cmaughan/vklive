#pragma once

#include <vklive/scene.h>
#include <vklive/vulkan/vulkan_context.h>
#include <vklive/vulkan/vulkan_framebuffer.h>
#include <vklive/vulkan/vulkan_model.h>

#include <vklive/camera.h>
namespace vulkan
{

struct RenderContext;
// These structures mirror the scene structures and add the vulkan specific bits
// The vulkan objects should not live longer than the scene!
struct VulkanSurface
{
    VulkanSurface(Surface* pS)
        : pSurface(pS)
    {
    }
    Surface* pSurface;
    VulkanImage image;
    std::string debugName;
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

struct VulkanShader
{
    VulkanShader(Shader* pS)
        : pShader(pS)
    {
    }
    Shader* pShader;

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
    glm::uvec2 currentSize = glm::uvec2(0);
    vk::RenderPass renderPass;
    
    vk::Pipeline geometryPipeline;
    vk::PipelineLayout geometryPipelineLayout;

    vk::DescriptorSetLayout descriptorSetLayout;
    vk::DescriptorSet descriptorSet;

    VulkanBuffer vsUniform;

    struct UBO
    {
        glm::vec4 time;
        glm::mat4 projection;
        glm::mat4 model;
        glm::vec4 lightPos = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    };
    UBO vsUBO;
};

struct VulkanScene
{
    VulkanScene(Scene* pS)
        : pScene(pS)
    {
    }

    Scene* pScene = nullptr;
    std::map<std::string, std::shared_ptr<VulkanSurface>> surfaces;
    std::map<fs::path, std::shared_ptr<VulkanGeometry>> geometries;
    std::map<fs::path, std::shared_ptr<VulkanShader>> shaderStages;
    
    std::map<std::string, std::shared_ptr<VulkanPass>> passes;
};

void scene_init(VulkanContext& ctx, Scene& scene);
void scene_destroy(VulkanContext& ctx, Scene& scene);
void scene_render(VulkanContext& ctx, RenderContext& renderContext, Scene& scene);
void scene_prepare(VulkanContext& ctx, RenderContext& renderContext, Scene& scene);

} // namespace vulkan
