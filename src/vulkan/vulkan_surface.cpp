#include <gli/gli.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "vklive/vulkan/vulkan_buffer.h"
#include "vklive/vulkan/vulkan_command.h"
#include "vklive/vulkan/vulkan_context.h"
#include "vklive/vulkan/vulkan_surface.h"
#include "vklive/vulkan/vulkan_utils.h"

#include <vklive/audio/audio_analysis.h>
#include <vklive/memory.h>

namespace vulkan
{
void surface_unmap(VulkanContext& ctx, VulkanSurface& img)
{
    ctx.device.unmapMemory(img.memory);
    img.mapped = nullptr;
}

void surface_destroy(VulkanContext& ctx, VulkanSurface& img)
{
    if (img.stagingBuffer.buffer)
    {
        buffer_destroy(ctx, img.stagingBuffer);
    }

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
        // ctx.device.freeDescriptorSets(ctx.descriptorPool, img.samplerDescriptorSet);
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

void surface_create_sampler(VulkanContext& ctx, VulkanSurface& surface)
{
    // Create sampler
    vk::SamplerCreateInfo samplerCreateInfo;
    samplerCreateInfo.magFilter = vk::Filter::eLinear;
    samplerCreateInfo.minFilter = vk::Filter::eLinear;
    samplerCreateInfo.addressModeU = vk::SamplerAddressMode::eMirroredRepeat;
    samplerCreateInfo.addressModeV = vk::SamplerAddressMode::eMirroredRepeat;
    samplerCreateInfo.addressModeW = vk::SamplerAddressMode::eMirroredRepeat;

    samplerCreateInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    // Max level-of-detail should match mip level count
    samplerCreateInfo.maxLod = static_cast<float>(surface.mipLevels);
    // Only enable anisotropic filtering if enabled on the devicec
    // TODO
    // samplerCreateInfo.maxAnisotropy = ctx.deviceFeatures.samplerAnisotropy ? ctx.deviceProperties.limits.maxSamplerAnisotropy : 1.0f;
    // samplerCreateInfo.anisotropyEnable = ctx.deviceFeatures.samplerAnisotropy;
    samplerCreateInfo.borderColor = vk::BorderColor::eFloatTransparentBlack; // OpaqueWhite;

    surface.sampler = ctx.device.createSampler(samplerCreateInfo);
}

void surface_set_sampling(VulkanContext& ctx, VulkanSurface& surface)
{
    surface_create_sampler(ctx, surface);

    auto binding = vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, &surface.sampler);
    surface.samplerDescriptorSetLayout = ctx.device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), binding));
    surface.samplerDescriptorSet = ctx.device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(ctx.descriptorPool, surface.samplerDescriptorSetLayout))[0];

    // Update the Descriptor Set:
    {
        vk::DescriptorImageInfo desc_image;
        desc_image.sampler = surface.sampler;
        desc_image.imageView = surface.view;
        desc_image.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        vk::WriteDescriptorSet write_desc;
        write_desc.dstSet = surface.samplerDescriptorSet;
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

void surface_stage_to_device(VulkanContext& ctx, VulkanSurface& surface, vk::ImageCreateInfo imageCreateInfo, const vk::MemoryPropertyFlags& memoryPropertyFlags, vk::DeviceSize size, const void* data, const std::vector<MipData>& mipData, const vk::ImageLayout layout)
{
    VulkanBuffer staging = buffer_create_staging(ctx, size, data);
    imageCreateInfo.usage = imageCreateInfo.usage | vk::ImageUsageFlagBits::eTransferDst;

    surface_create(ctx, surface, imageCreateInfo, memoryPropertyFlags);

    utils_with_command_buffer(ctx, [&](const vk::CommandBuffer& copyCmd) {
        debug_set_commandbuffer_name(ctx.device, copyCmd, "Buffer::StageToDevice");
        vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, imageCreateInfo.mipLevels, 0, 1);

        // Prepare for transfer
        surface_set_layout(ctx, copyCmd, surface.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, range);

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
        copyCmd.copyBufferToImage(staging.buffer, surface.image, vk::ImageLayout::eTransferDstOptimal, bufferCopyRegions);
        // Prepare for shader read
        surface_set_layout(ctx, copyCmd, surface.image, vk::ImageLayout::eTransferDstOptimal, layout, range);
    });
    buffer_destroy(ctx, staging);
}

void surface_stage_to_device(VulkanContext& ctx, VulkanSurface& surface, const vk::ImageCreateInfo& imageCreateInfo, const vk::MemoryPropertyFlags& memoryPropertyFlags, const gli::texture2d& tex2D, const vk::ImageLayout& layout)
{
    std::vector<MipData> mips;
    for (size_t i = 0; i < imageCreateInfo.mipLevels; ++i)
    {
        const auto& mip = tex2D[i];
        const auto dims = mip.extent();
        mips.push_back({ vk::Extent3D{ (uint32_t)dims.x, (uint32_t)dims.y, 1 }, (uint32_t)mip.size() });
    }
    surface_stage_to_device(ctx, surface, imageCreateInfo, memoryPropertyFlags, (vk::DeviceSize)tex2D.size(), tex2D.data(), mips, layout);
}

void surface_create_from_file(VulkanContext& ctx, VulkanSurface& surface, const fs::path& filename, vk::Format format, vk::ImageUsageFlags imageUsageFlags, vk::ImageLayout imageLayout, bool forceLinear)
{
    surface_destroy(ctx, surface);

    if (!fs::exists(filename))
    {
        return;
    }

    std::shared_ptr<gli::texture2d> tex2Dptr;

    auto data = file_read(filename);

    if (filename.extension() == ".dds" || filename.extension() == ".ktx")
    {
        auto pTex = std::make_shared<gli::texture2d>(gli::load(data.c_str(), data.size()));
        if (!pTex)
        {
            // TODO: Error
            return;
        }

        surface.extent.width = static_cast<uint32_t>(pTex->extent().x);
        surface.extent.height = static_cast<uint32_t>(pTex->extent().y);
        surface.extent.depth = 1;
        surface.mipLevels = static_cast<uint32_t>(pTex->levels());
        surface.layerCount = 1;

        // Create optimal tiled target image
        vk::ImageCreateInfo imageCreateInfo;
        imageCreateInfo.imageType = vk::ImageType::e2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = surface.mipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.extent = surface.extent;
        imageCreateInfo.usage = imageUsageFlags | vk::ImageUsageFlagBits::eTransferDst;

        // Will create the surface image
        surface_stage_to_device(ctx, surface, imageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal, *pTex, imageLayout);
    }
    else
    {
        int x, y, n;
        auto loaded = stbi_load_from_memory((const stbi_uc*)data.c_str(), int(data.size()), &x, &y, &n, 0);

        surface.extent.width = static_cast<uint32_t>(x);
        surface.extent.height = static_cast<uint32_t>(y);
        surface.extent.depth = 1;
        surface.mipLevels = 1;
        surface.layerCount = 1;

        // Create optimal tiled target image
        vk::ImageCreateInfo imageCreateInfo;
        imageCreateInfo.imageType = vk::ImageType::e2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = surface.mipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.extent = surface.extent;
        imageCreateInfo.usage = imageUsageFlags | vk::ImageUsageFlagBits::eTransferDst;

        // Will create the surface image
        surface_stage_to_device(ctx, surface, imageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal, x * y * n, static_cast<const void*>(loaded));
    }

    // Add sampler
    surface_create_sampler(ctx, surface);

    // Create image view
    static const vk::ImageUsageFlags VIEW_USAGE_FLAGS = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment;

    if (imageUsageFlags & VIEW_USAGE_FLAGS)
    {
        vk::ImageViewCreateInfo viewCreateInfo;
        viewCreateInfo.viewType = vk::ImageViewType::e2D;
        viewCreateInfo.image = surface.image;
        viewCreateInfo.format = format;
        viewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, surface.mipLevels, 0, surface.layerCount };
        surface.view = ctx.device.createImageView(viewCreateInfo);
    }
}

void surface_update_from_audio(VulkanContext& ctx, VulkanSurface& surface, bool& surfaceChanged)
{
    auto& audioContext = Audio::GetAudioContext();

    auto updateSurface = [&](auto width, auto height) {
        if (surface.extent.width != width || surface.extent.height != height)
        {
            surfaceChanged = true;
            surface.extent.width = width;
            surface.extent.height = height;
            surface.extent.depth = 1;
            surface.mipLevels = 1;
            surface.layerCount = 1;

            vk::Format format = vk::Format::eR32Sfloat;
            vk::ImageUsageFlags imageUsageFlags = vk::ImageUsageFlagBits::eSampled;
            vk::ImageLayout imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

            // Create optimal tiled target image
            vk::ImageCreateInfo imageCreateInfo;
            imageCreateInfo.imageType = vk::ImageType::e2D;
            imageCreateInfo.format = format;
            imageCreateInfo.mipLevels = surface.mipLevels;
            imageCreateInfo.arrayLayers = 1;
            imageCreateInfo.extent = surface.extent;
            imageCreateInfo.usage = imageUsageFlags | vk::ImageUsageFlagBits::eTransferDst;

            // Need this?
            ctx.device.waitIdle();

            surface_create(ctx, surface, imageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal);

            // Add the staging buffer for transfers
            surface.stagingBuffer = buffer_create_staging(ctx, width * height * sizeof(float));

            // Add sampler for using this surface in a shader
            surface_create_sampler(ctx, surface);

            // Create image view
            static const vk::ImageUsageFlags VIEW_USAGE_FLAGS = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment;

            if (imageUsageFlags & VIEW_USAGE_FLAGS)
            {
                vk::ImageViewCreateInfo viewCreateInfo;
                viewCreateInfo.viewType = vk::ImageViewType::e2D;
                viewCreateInfo.image = surface.image;
                viewCreateInfo.format = format;
                viewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, surface.mipLevels, 0, surface.layerCount };
                surface.view = ctx.device.createImageView(viewCreateInfo);
            }
        }
    };
        
    if (audioContext.analysisChannels.empty())
    {
        // Ensure a blank surface
        if (surface.extent.width == 0 || surface.extent.height == 0)
        {
            updateSurface(surface.extent.width, surface.extent.height);
        }
        return;
    }

    static std::vector<float> uploadCache;

    size_t bufferWidth = 512; // default width if no data
    const auto Channels = std::max(audioContext.analysisChannels.size(), size_t(1));
    const auto BufferTypes = 2; // Spectrum + Audio
    const auto BufferHeight = Channels * BufferTypes;

    for (int channel = 0; channel < Channels; channel++)
    {
        auto& analysis = audioContext.analysisChannels[channel];
        
        ConsumerMemLock memLock(analysis->analysisData);
        auto& processData = memLock.Data();
        auto currentBuffer = 1 - processData.currentBuffer;

        auto& spectrumBuckets = processData.spectrumBuckets[currentBuffer];
        auto& audio = processData.audio[currentBuffer];

        // Stereo, Audio and Spectrum (4 rows total)
        if (spectrumBuckets.size() != 0)
        {
            bufferWidth = spectrumBuckets.size();
            uploadCache.resize(BufferHeight * spectrumBuckets.size());

            memcpy(&uploadCache[channel * spectrumBuckets.size()], &spectrumBuckets[0], spectrumBuckets.size() * sizeof(float));
           
            // Copy audio, note that we always make the audio at least as big as the spectrum
            assert(audio.size() >= spectrumBuckets.size());
            memcpy(&uploadCache[(channel + Channels) * spectrumBuckets.size()], &audio[0], spectrumBuckets.size() * sizeof(float));
        }
        else
        {
            // Make sure upload cache matches: it's OK that it might be empty
            uploadCache.resize(bufferWidth * BufferHeight, 0.0f);
            memset(&uploadCache[0], 0, uploadCache.size() * sizeof(float));
        }
    }
   
    // Left/Right FFT and Audio (4 rows)
    updateSurface(bufferWidth, Channels * BufferTypes);

    utils_copy_to_memory(ctx, surface.stagingBuffer.memory, uploadCache);

    // TODO: This is a little inefficient; need to use the scene command buffer, or a copy queue and sync...
    // On the other hand, we aren't going for AAA game engine here.
    utils_with_command_buffer(ctx, [&](const vk::CommandBuffer& copyCmd) {
        debug_set_commandbuffer_name(ctx.device, copyCmd, "Buffer::StageAudioToDevice");
        vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

        // Prepare for transfer
        surface_set_layout(ctx, copyCmd, surface.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, range);

        // Prepare for transfer
        std::vector<vk::BufferImageCopy> bufferCopyRegions;
        {
            vk::BufferImageCopy bufferCopyRegion;
            bufferCopyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent = surface.extent;
            bufferCopyRegions.push_back(bufferCopyRegion);
        }
        copyCmd.copyBufferToImage(surface.stagingBuffer.buffer, surface.image, vk::ImageLayout::eTransferDstOptimal, bufferCopyRegions);
        
        // Prepare for shader read
        surface_set_layout(ctx, copyCmd, surface.image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, range);
    });
}

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
