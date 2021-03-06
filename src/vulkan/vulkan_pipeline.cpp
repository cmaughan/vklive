#include <fmt/format.h>

#include "vklive/vulkan/vulkan_pipeline.h"
#include "vklive/file/file.h"
#include "vklive/vulkan/vulkan_utils.h"
#include "vklive/logger/logger.h"

namespace vulkan
{

vk::Pipeline pipeline_create(VulkanContext& ctx, const VertexLayout& vertexLayout, const vk::PipelineLayout& layout, const vk::RenderPass& renderPass, const std::vector<vk::PipelineShaderStageCreateInfo>& shaders)
{
    try
    {
        // Binding
        std::vector<vk::VertexInputBindingDescription> bindingDescriptions;
        uint32_t binding = 0;
        bindingDescriptions.emplace_back(binding, layout_size(vertexLayout), vk::VertexInputRate::eVertex); // stride?

        // Attribute descriptions
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
        auto componentsSize = vertexLayout.components.size();
        attributeDescriptions.reserve(attributeDescriptions.size() + componentsSize);
        auto attributeIndexOffset = (uint32_t)attributeDescriptions.size();
        for (uint32_t i = 0; i < componentsSize; ++i)
        {
            const auto& component = vertexLayout.components[i];
            const auto format = component_format(component);
            const auto offset = layout_offset(vertexLayout, i);
            attributeDescriptions.emplace_back(attributeIndexOffset + i, binding, format, offset);
        }

        auto vertex_info = vk::PipelineVertexInputStateCreateInfo({}, bindingDescriptions, attributeDescriptions);

        vk::PipelineInputAssemblyStateCreateInfo ia_info;
        ia_info.topology = vk::PrimitiveTopology::eTriangleList;

        vk::PipelineViewportStateCreateInfo viewport_info;
        viewport_info.viewportCount = 1;
        viewport_info.scissorCount = 1;

        vk::PipelineRasterizationStateCreateInfo raster_info;
        raster_info.polygonMode = vk::PolygonMode::eFill;
        raster_info.cullMode = vk::CullModeFlagBits::eBack;
        raster_info.frontFace = vk::FrontFace::eClockwise;
        raster_info.lineWidth = 1.0f;

        vk::PipelineMultisampleStateCreateInfo ms_info;
        ms_info.rasterizationSamples = ctx.MSAASamples;

        vk::PipelineColorBlendAttachmentState color_attachment[1];
        color_attachment[0].blendEnable = 1;
        color_attachment[0].srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        color_attachment[0].dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        color_attachment[0].colorBlendOp = vk::BlendOp::eAdd;
        color_attachment[0].srcAlphaBlendFactor = vk::BlendFactor::eOne;
        color_attachment[0].dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        color_attachment[0].alphaBlendOp = vk::BlendOp::eAdd;
        color_attachment[0].colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

        vk::PipelineDepthStencilStateCreateInfo depth_info;
        depth_info.depthTestEnable = VK_TRUE;
        depth_info.depthWriteEnable = VK_TRUE;
        depth_info.depthCompareOp = vk::CompareOp::eLessOrEqual;

        vk::PipelineColorBlendStateCreateInfo blend_info;
        blend_info.attachmentCount = 1;
        blend_info.pAttachments = color_attachment;

        auto dynamic_states = std::vector<vk::DynamicState>{ vk::DynamicState::eViewport, vk::DynamicState::eScissor };
        vk::PipelineDynamicStateCreateInfo dynamic_state;
        dynamic_state.dynamicStateCount = (uint32_t)(dynamic_states.size());
        dynamic_state.pDynamicStates = dynamic_states.data();

        vk::GraphicsPipelineCreateInfo info(vk::PipelineCreateFlags(), shaders);
        info.pVertexInputState = &vertex_info;
        info.pInputAssemblyState = &ia_info;
        info.pViewportState = &viewport_info;
        info.pRasterizationState = &raster_info;
        info.pMultisampleState = &ms_info;
        info.pDepthStencilState = &depth_info;
        info.pColorBlendState = &blend_info;
        info.pDynamicState = &dynamic_state;
        info.layout = layout;
        info.renderPass = renderPass;
        info.subpass = 0;
        return ctx.device.createGraphicsPipelines(ctx.pipelineCache, info).value[0];
    }
    catch(std::exception& ex)
    {
        LOG(DBG, fmt::format("Error creating pipeline: {}", ex.what()));
    }
    return nullptr;
}

} // namespace vulkan
