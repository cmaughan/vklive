#include "vklive/vulkan/vulkan_surface.h"
#include "vklive/vulkan/vulkan_context.h"
#include "vklive/vulkan/vulkan_utils.h"
#include "vklive/vulkan/vulkan_buffer.h"
#include "vklive/vulkan/vulkan_command.h"

namespace vulkan
{
void surface_unmap(VulkanContext& ctx, VulkanSurface& img)
{
    ctx.device.unmapMemory(img.memory);
    img.mapped = nullptr;
}

void surface_destroy(VulkanContext& ctx, VulkanSurface& img)
{
    if (img.sampler)
    {
        ctx.device.destroySampler(img.sampler);
        img.sampler = vk::Sampler();
    }
    if (img.samplerDescriptorSetLayout)
    {
        ctx.device.destroyDescriptorSetLayout(img.samplerDescriptorSetLayout);
        img.samplerDescriptorSetLayout = nullptr;
    }

    // We don't free these here, but do take account of the fact they are no longer valid
    if (img.samplerDescriptorSet)
    {
        //ctx.device.freeDescriptorSets(ctx.descriptorPool, img.samplerDescriptorSet);
        img.samplerDescriptorSet = nullptr;
    }
    if (img.mapped)
    {
        surface_unmap(ctx, img);
        img.mapped = nullptr;
    }
    if (img.view)
    {
        ctx.device.destroyImageView(img.view);
        img.view = vk::ImageView();
    }
    if (img.image)
    {
        ctx.device.destroyImage(img.image);
        img.image = vk::Image();
    }
    if (img.memory)
    {
        ctx.device.freeMemory(img.memory);
        img.memory = vk::DeviceMemory();
    }
};

void surface_create(VulkanContext& ctx, VulkanSurface& vulkanImage, const vk::ImageCreateInfo& imageCreateInfo, const vk::MemoryPropertyFlags& memoryPropertyFlags)
{
    surface_destroy(ctx, vulkanImage);

    vulkanImage.image = ctx.device.createImage(imageCreateInfo);
    vulkanImage.format = imageCreateInfo.format;
    vulkanImage.extent = imageCreateInfo.extent;
    vk::MemoryRequirements memReqs = ctx.device.getImageMemoryRequirements(vulkanImage.image);
    vk::MemoryAllocateInfo memAllocInfo;
    memAllocInfo.allocationSize = vulkanImage.allocSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = utils_memory_type(ctx, memoryPropertyFlags, memReqs.memoryTypeBits);
    vulkanImage.memory = ctx.device.allocateMemory(memAllocInfo);
    ctx.device.bindImageMemory(vulkanImage.image, vulkanImage.memory, 0);
}

void surface_create(VulkanContext& ctx, VulkanSurface& vulkanImage, const glm::uvec2& size, vk::Format colorFormat, bool sampled, const std::string& name)
{
    surface_destroy(ctx, vulkanImage);

    vk::ImageUsageFlags colorUsage = sampled ? vk::ImageUsageFlagBits::eSampled : vk::ImageUsageFlagBits();

    // Color attachment
    vk::ImageCreateInfo image;
    image.imageType = vk::ImageType::e2D;
    image.extent.width = size.x;
    image.extent.height = size.y;
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = vk::SampleCountFlagBits::e1;
    image.tiling = vk::ImageTiling::eOptimal;
    image.usage = vk::ImageUsageFlagBits::eColorAttachment | colorUsage;
    image.format = colorFormat;
    surface_create(ctx, vulkanImage, image, vk::MemoryPropertyFlagBits::eDeviceLocal);
    debug_set_image_name(ctx.device, vulkanImage.image, name + ":ColorImage");
    debug_set_devicememory_name(ctx.device, vulkanImage.memory, name + ":ColorImageMemory");

    if (sampled)
    {
        vk::ImageViewCreateInfo colorImageView;
        colorImageView.viewType = vk::ImageViewType::e2D;
        colorImageView.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        colorImageView.subresourceRange.levelCount = 1;
        colorImageView.subresourceRange.layerCount = 1;
        colorImageView.format = colorFormat;
        colorImageView.image = vulkanImage.image;
        vulkanImage.view = ctx.device.createImageView(colorImageView);
        debug_set_imageview_name(ctx.device, vulkanImage.view, name + ":ColorImageView");
    }
}

void surface_create_depth(VulkanContext& ctx, VulkanSurface& vulkanImage, const glm::uvec2& size, vk::Format depthFormat, bool sampled, const std::string& name)
{
    surface_destroy(ctx, vulkanImage);

    vk::ImageUsageFlags depthUsage = vk::ImageUsageFlags();

    assert(depthFormat != vk::Format::eUndefined);

    // Color attachment
    vk::ImageCreateInfo image;
    image.imageType = vk::ImageType::e2D;
    image.extent.width = size.x;
    image.extent.height = size.y;
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = vk::SampleCountFlagBits::e1;
    image.tiling = vk::ImageTiling::eOptimal;
    image.format = depthFormat;
    image.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | depthUsage;
     
    surface_create(ctx, vulkanImage, image, vk::MemoryPropertyFlagBits::eDeviceLocal);
    debug_set_image_name(ctx.device, vulkanImage.image, name + ":DepthImage");
    debug_set_devicememory_name(ctx.device, vulkanImage.memory, name + ":DepthImageMemory");

    vk::ImageViewCreateInfo depthStencilView;
    depthStencilView.viewType = vk::ImageViewType::e2D;
    depthStencilView.format = depthFormat;
    depthStencilView.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    depthStencilView.subresourceRange.levelCount = 1;
    depthStencilView.subresourceRange.layerCount = 1;
    depthStencilView.image = vulkanImage.image;
    vulkanImage.view = ctx.device.createImageView(depthStencilView);
    debug_set_imageview_name(ctx.device, vulkanImage.view, name + ":DepthImageView");
}

void surface_set_sampling(VulkanContext& ctx, VulkanSurface& image)
{
    image.sampler = ctx.device.createSampler(vk::SamplerCreateInfo({}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear));

    auto binding = vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, &image.sampler);
    image.samplerDescriptorSetLayout = ctx.device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), binding));
    image.samplerDescriptorSet = ctx.device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(ctx.descriptorPool, image.samplerDescriptorSetLayout))[0];

    // Update the Descriptor Set:
    {
        vk::DescriptorImageInfo desc_image;
        desc_image.sampler = image.sampler;
        desc_image.imageView = image.view;
        desc_image.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        vk::WriteDescriptorSet write_desc;
        write_desc.dstSet = image.samplerDescriptorSet;
        write_desc.descriptorCount = 1;
        write_desc.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write_desc.setImageInfo(desc_image);
        ctx.device.updateDescriptorSets(write_desc, {});
    }
}

/*
void surface_set_layout(VulkanContext& ctx, vk::CommandBuffer const& commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout)
{
    vk::AccessFlags sourceAccessMask;
    switch (oldImageLayout)
    {
    case vk::ImageLayout::eTransferDstOptimal:
        sourceAccessMask = vk::AccessFlagBits::eTransferWrite;
        break;
    case vk::ImageLayout::ePreinitialized:
        sourceAccessMask = vk::AccessFlagBits::eHostWrite;
        break;
    case vk::ImageLayout::eGeneral: // sourceAccessMask is empty
    case vk::ImageLayout::eUndefined:
        break;
    default:
        assert(false);
        break;
    }

    vk::PipelineStageFlags sourceStage;
    switch (oldImageLayout)
    {
    case vk::ImageLayout::eGeneral:
    case vk::ImageLayout::ePreinitialized:
        sourceStage = vk::PipelineStageFlagBits::eHost;
        break;
    case vk::ImageLayout::eTransferDstOptimal:
        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        break;
    case vk::ImageLayout::eUndefined:
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        break;
    default:
        assert(false);
        break;
    }

    vk::AccessFlags destinationAccessMask;
    switch (newImageLayout)
    {
    case vk::ImageLayout::eColorAttachmentOptimal:
        destinationAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        break;
    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        destinationAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        break;
    case vk::ImageLayout::eGeneral: // empty destinationAccessMask
    case vk::ImageLayout::ePresentSrcKHR:
        break;
    case vk::ImageLayout::eShaderReadOnlyOptimal:
        destinationAccessMask = vk::AccessFlagBits::eShaderRead;
        break;
    case vk::ImageLayout::eTransferSrcOptimal:
        destinationAccessMask = vk::AccessFlagBits::eTransferRead;
        break;
    case vk::ImageLayout::eTransferDstOptimal:
        destinationAccessMask = vk::AccessFlagBits::eTransferWrite;
        break;
    default:
        assert(false);
        break;
    }

    vk::PipelineStageFlags destinationStage;
    switch (newImageLayout)
    {
    case vk::ImageLayout::eColorAttachmentOptimal:
        destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        break;
    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
        destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
        break;
    case vk::ImageLayout::eGeneral:
        destinationStage = vk::PipelineStageFlagBits::eHost;
        break;
    case vk::ImageLayout::ePresentSrcKHR:
        destinationStage = vk::PipelineStageFlagBits::eBottomOfPipe;
        break;
    case vk::ImageLayout::eShaderReadOnlyOptimal:
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
        break;
    case vk::ImageLayout::eTransferDstOptimal:
    case vk::ImageLayout::eTransferSrcOptimal:
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
        break;
    default:
        assert(false);
        break;
    }

    vk::ImageAspectFlags aspectMask;
    if (newImageLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    {
        aspectMask = vk::ImageAspectFlagBits::eDepth;
        if (format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint)
        {
            aspectMask |= vk::ImageAspectFlagBits::eStencil;
        }
    }
    else
    {
        aspectMask = vk::ImageAspectFlagBits::eColor;
    }

    vk::ImageSubresourceRange imageSubresourceRange(aspectMask, 0, 1, 0, 1);
    vk::ImageMemoryBarrier imageMemoryBarrier(sourceAccessMask,
        destinationAccessMask,
        oldImageLayout,
        newImageLayout,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        image,
        imageSubresourceRange);
    return commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, imageMemoryBarrier);
}*/

// Create an image memory barrier for changing the layout of
// an image and put it into an active command buffer
// See chapter 11.4 "vk::Image Layout" for details
void surface_set_layout(VulkanContext& ctx, vk::CommandBuffer cmdbuffer, vk::Image image, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout, vk::ImageSubresourceRange subresourceRange)
{
    // Create an image barrier object
    vk::ImageMemoryBarrier imageMemoryBarrier;
    imageMemoryBarrier.oldLayout = oldImageLayout;
    imageMemoryBarrier.newLayout = newImageLayout;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange = subresourceRange;
    imageMemoryBarrier.srcAccessMask = utils_access_flags_for_layout(oldImageLayout);
    imageMemoryBarrier.dstAccessMask = utils_access_flags_for_layout(newImageLayout);
    vk::PipelineStageFlags srcStageMask = utils_pipeline_stage_flags_for_layout(oldImageLayout);
    vk::PipelineStageFlags destStageMask = utils_pipeline_stage_flags_for_layout(newImageLayout);
    // Put barrier on top
    // Put barrier inside setup command buffer
    cmdbuffer.pipelineBarrier(srcStageMask, destStageMask, vk::DependencyFlags(), nullptr, nullptr, imageMemoryBarrier);
}

// Fixed sub resource on first mip level and layer
void surface_set_layout(VulkanContext& ctx, vk::CommandBuffer cmdbuffer, vk::Image image, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout)
{
    surface_set_layout(ctx, cmdbuffer, image, vk::ImageAspectFlagBits::eColor, oldImageLayout, newImageLayout);
}

// Fixed sub resource on first mip level and layer
void surface_set_layout(VulkanContext& ctx, vk::CommandBuffer cmdbuffer, vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout)
{
    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask = aspectMask;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;
    surface_set_layout(ctx, cmdbuffer, image, oldImageLayout, newImageLayout, subresourceRange);
}

void surface_set_layout(VulkanContext& ctx, vk::Image image, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout, vk::ImageSubresourceRange subresourceRange)
{
    utils_with_command_buffer(ctx, [&](const auto& commandBuffer) {
        debug_set_commandbuffer_name(ctx.device, commandBuffer, "Buffer::ImageSetLayout");
        surface_set_layout(ctx, commandBuffer, image, oldImageLayout, newImageLayout, subresourceRange);
    });
}

// Fixed sub resource on first mip level and layer
void surface_set_layout(VulkanContext& ctx, vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout)
{
    utils_with_command_buffer(ctx, [&](const auto& commandBuffer) {
        debug_set_commandbuffer_name(ctx.device, commandBuffer, "Buffer::ImageSetLayout");
        surface_set_layout(ctx, commandBuffer, image, aspectMask, oldImageLayout, newImageLayout);
    });
}

/*
VulkanSurface surface_stage_to_device(VulkanContext& ctx, vk::ImageCreateInfo imageCreateInfo, const vk::MemoryPropertyFlags& memoryPropertyFlags, vk::DeviceSize size, const void* data, const std::vector<MipData>& mipData, const vk::ImageLayout layout)
{
    VulkanBuffer staging = buffer_create_staging(ctx, size, data);
    imageCreateInfo.usage = imageCreateInfo.usage | vk::ImageUsageFlagBits::eTransferDst;
    
    VulkanSurface result;
    surface_create(ctx, result, imageCreateInfo, memoryPropertyFlags);

    utils_with_command_buffer(ctx, [&](const vk::CommandBuffer& copyCmd) {
        debug_set_commandbuffer_name(ctx.device, copyCmd, "Buffer::StageToDevice");
        vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, imageCreateInfo.mipLevels, 0, 1);

        // Prepare for transfer
        surface_set_layout(ctx, copyCmd, result.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, range);

        // Prepare for transfer
        std::vector<vk::BufferImageCopy> bufferCopyRegions;
        {
            vk::BufferImageCopy bufferCopyRegion;
            bufferCopyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            if (!mipData.empty())
            {
                for (uint32_t i = 0; i < imageCreateInfo.mipLevels; i++)
                {
                    bufferCopyRegion.imageSubresource.mipLevel = i;
                    bufferCopyRegion.imageExtent = mipData[i].first;
                    bufferCopyRegions.push_back(bufferCopyRegion);
                    bufferCopyRegion.bufferOffset += mipData[i].second;
                }
            }
            else
            {
                bufferCopyRegion.imageExtent = imageCreateInfo.extent;
                bufferCopyRegions.push_back(bufferCopyRegion);
            }
        }
        copyCmd.copyBufferToImage(staging.buffer, result.image, vk::ImageLayout::eTransferDstOptimal, bufferCopyRegions);
        // Prepare for shader read
        surface_set_layout(ctx, copyCmd, result.image, vk::ImageLayout::eTransferDstOptimal, layout, range);
    });
    buffer_destroy(ctx, staging);
    return result;
}
*/
/*
inline void copy(size_t size, const void* data, VkDeviceSize offset = 0) const
{
    memcpy(static_cast<uint8_t*>(mapped) + offset, data, size);
}
template <typename T>
inline void copy(const T& data, VkDeviceSize offset = 0) const
{
    copy(sizeof(T), &data, offset);
}
template <typename T>
inline void copy(const std::vector<T>& data, VkDeviceSize offset = 0) const
{
    copy(sizeof(T) * data.size(), data.data(), offset);
}

// Flush a memory range of the buffer to make it visible to the device
void flush(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0)
{
    return device.flushMappedMemoryRanges(vk::MappedMemoryRange{ memory, offset, size });
}

void invalidate(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0)
{
    return device.invalidateMappedMemoryRanges(vk::MappedMemoryRange{ memory, offset, size });
}
*/
} // namespace vulkan
