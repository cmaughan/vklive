#include <gli/gli.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "vklive/vulkan/vulkan_buffer.h"
#include "vklive/vulkan/vulkan_command.h"
#include "vklive/vulkan/vulkan_context.h"
#include "vklive/vulkan/vulkan_surface.h"
#include "vklive/vulkan/vulkan_utils.h"

#include <zest/logger/logger.h>
#include <zest/memory/memory.h>

#include <zing/audio/audio_analysis.h>

namespace vulkan
{

std::string to_string(const VulkanSurface& surf)
{
    return fmt::format("Surface Key:{}, Image:{}, Extent:{},{}", surf.key.DebugName(), (void*)surf.image, surf.extent.width, surf.extent.height);
}

void surface_unmap(VulkanContext& ctx, VulkanSurface& img)
{
    ctx.device.unmapMemory(img.memory);
    img.mapped = nullptr;
}

void vulkan_surface_destroy(VulkanContext& ctx, VulkanSurface& img)
{
    if (img.stagingBuffer.buffer)
    {
        vulkan_buffer_destroy(ctx, img.stagingBuffer);
    }

    if (img.sampler)
    {
        LOG(DBG, "Destroy Sampler: " << img.sampler);
        ctx.device.destroySampler(img.sampler);
        img.sampler = nullptr;
    }

    if (img.mapped)
    {
        surface_unmap(ctx, img);
        img.mapped = nullptr;
    }

    if (img.view)
    {
        ctx.device.destroyImageView(img.view);
        img.view = nullptr;
    }

    if (img.image)
    {
        ctx.device.destroyImage(img.image);
        img.image = nullptr;
    }

    if (img.memory)
    {
        // Log here, because the memory is really the indicator that we have the surface
        LOG(DBG, "Destroy Surface: " << img);

        ctx.device.freeMemory(img.memory);
        img.memory = nullptr;
    }

    img.ImGuiDescriptorSet = nullptr;
};

void vulkan_surface_create(VulkanContext& ctx, VulkanSurface& vulkanSurface, const vk::ImageCreateInfo& imageCreateInfo, const vk::MemoryPropertyFlags& memoryPropertyFlags)
{
    vulkan_surface_destroy(ctx, vulkanSurface);

    vulkanSurface.image = ctx.device.createImage(imageCreateInfo);
    vulkanSurface.format = imageCreateInfo.format;
    vulkanSurface.extent = imageCreateInfo.extent;
    vk::MemoryRequirements memReqs = ctx.device.getImageMemoryRequirements(vulkanSurface.image);
    vk::MemoryAllocateInfo memAllocInfo;
    memAllocInfo.allocationSize = vulkanSurface.allocSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = utils_memory_type(ctx, memoryPropertyFlags, memReqs.memoryTypeBits);
    vulkanSurface.memory = ctx.device.allocateMemory(memAllocInfo);
    ctx.device.bindImageMemory(vulkanSurface.image, vulkanSurface.memory, 0);

    vulkanSurface.allocationState = VulkanAllocationState::Loaded;

    vulkanSurface.generation++;
}

void vulkan_surface_create(VulkanContext& ctx, VulkanSurface& vulkanSurface, const glm::uvec2& size, vk::Format colorFormat, bool sampled)
{
    vulkan_surface_destroy(ctx, vulkanSurface);

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
    if (vulkanSurface.pSurface->isRayTarget)
    {
        image.usage |= vk::ImageUsageFlagBits::eStorage;
    }
    image.format = colorFormat;
    vulkan_surface_create(ctx, vulkanSurface, image, vk::MemoryPropertyFlagBits::eDeviceLocal);

    if (sampled)
    {
        vk::ImageViewCreateInfo colorImageView;
        colorImageView.viewType = vk::ImageViewType::e2D;
        colorImageView.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        colorImageView.subresourceRange.levelCount = 1;
        colorImageView.subresourceRange.layerCount = 1;
        colorImageView.format = colorFormat;
        colorImageView.image = vulkanSurface.image;
        vulkanSurface.view = ctx.device.createImageView(colorImageView);
    }

    debug_set_surface_name(ctx.device, vulkanSurface, vulkanSurface.debugName);

    vulkanSurface.allocationState = VulkanAllocationState::Loaded;

    vulkanSurface.generation++;
}

void vulkan_surface_create_depth(VulkanContext& ctx, VulkanSurface& vulkanSurface, const glm::uvec2& size, vk::Format depthFormat, bool sampled)
{
    vulkan_surface_destroy(ctx, vulkanSurface);

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

    vulkan_surface_create(ctx, vulkanSurface, image, vk::MemoryPropertyFlagBits::eDeviceLocal);

    vk::ImageViewCreateInfo depthStencilView;
    depthStencilView.viewType = vk::ImageViewType::e2D;
    depthStencilView.format = depthFormat;
    depthStencilView.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    depthStencilView.subresourceRange.levelCount = 1;
    depthStencilView.subresourceRange.layerCount = 1;
    depthStencilView.image = vulkanSurface.image;
    vulkanSurface.view = ctx.device.createImageView(depthStencilView);

    debug_set_surface_name(ctx.device, vulkanSurface, vulkanSurface.debugName);

    vulkanSurface.allocationState = VulkanAllocationState::Loaded;

    vulkanSurface.generation++;
}

void surface_create_sampler(VulkanContext& ctx, VulkanSurface& surface)
{
    // Create sampler
    vk::SamplerCreateInfo samplerCreateInfo;
    // TODO: Obey user options
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
    debug_set_sampler_name(ctx.device, surface.sampler, surface.debugName);
}

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
    LOG(DBG, "Surface Set Layout");
    utils_with_command_buffer(ctx, [&](const auto& commandBuffer) {
        debug_set_commandbuffer_name(ctx.device, commandBuffer, "Buffer::ImageSetLayout");
        surface_set_layout(ctx, commandBuffer, image, oldImageLayout, newImageLayout, subresourceRange);
    });
}

// Fixed sub resource on first mip level and layer
void surface_set_layout(VulkanContext& ctx, vk::Image image, vk::ImageAspectFlags aspectMask, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout)
{
    LOG(DBG, "Surface Set Layout");
    utils_with_command_buffer(ctx, [&](const auto& commandBuffer) {
        debug_set_commandbuffer_name(ctx.device, commandBuffer, "Buffer::ImageSetLayout");
        surface_set_layout(ctx, commandBuffer, image, aspectMask, oldImageLayout, newImageLayout);
    });
}

void surface_stage_to_device(VulkanContext& ctx, VulkanSurface& surface, vk::ImageCreateInfo imageCreateInfo, const vk::MemoryPropertyFlags& memoryPropertyFlags, vk::DeviceSize size, const void* data, const std::vector<MipData>& mipData, const vk::ImageLayout layout)
{
    VulkanBuffer staging = buffer_create_staging(ctx, size, data);
    imageCreateInfo.usage = imageCreateInfo.usage | vk::ImageUsageFlagBits::eTransferDst;

    debug_set_buffer_name(ctx.device, staging.buffer, "Staging");

    vulkan_surface_create(ctx, surface, imageCreateInfo, memoryPropertyFlags);

    LOG(DBG, "Surface Stage To Device");
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
    vulkan_buffer_destroy(ctx, staging);
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

bool surface_create_from_memory(VulkanContext& ctx, VulkanSurface& vulkanSurface, const fs::path& filename, const char* pData, size_t data_size, vk::Format format, vk::ImageUsageFlags imageUsageFlags, vk::ImageLayout imageLayout, bool forceLinear)
{
    vulkan_surface_destroy(ctx, vulkanSurface);

    std::shared_ptr<gli::texture2d> tex2Dptr;

    if (filename.extension() == ".dds" || filename.extension() == ".ktx")
    {
        auto pTex = std::make_shared<gli::texture2d>(gli::load((char*)pData, data_size));
        if (!pTex)
        {
            // TODO: Error
            vulkanSurface.allocationState = VulkanAllocationState::Failed;
            return false;
        }

        vulkanSurface.extent.width = static_cast<uint32_t>(pTex->extent().x);
        vulkanSurface.extent.height = static_cast<uint32_t>(pTex->extent().y);
        vulkanSurface.extent.depth = 1;
        vulkanSurface.mipLevels = static_cast<uint32_t>(pTex->levels());
        vulkanSurface.layerCount = 1;

        // Create optimal tiled target image
        vk::ImageCreateInfo imageCreateInfo;
        imageCreateInfo.imageType = vk::ImageType::e2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = vulkanSurface.mipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.extent = vulkanSurface.extent;
        imageCreateInfo.usage = imageUsageFlags | vk::ImageUsageFlagBits::eTransferDst;

        // Will create the surface image
        surface_stage_to_device(ctx, vulkanSurface, imageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal, *pTex, imageLayout);
    }
    else
    {
        int x, y, n;
        auto loaded = stbi_load_from_memory((const stbi_uc*)pData, data_size, &x, &y, &n, 0);

        vulkanSurface.extent.width = static_cast<uint32_t>(x);
        vulkanSurface.extent.height = static_cast<uint32_t>(y);
        vulkanSurface.extent.depth = 1;
        vulkanSurface.mipLevels = 1;
        vulkanSurface.layerCount = 1;

        // Create optimal tiled target image
        vk::ImageCreateInfo imageCreateInfo;
        imageCreateInfo.imageType = vk::ImageType::e2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = vulkanSurface.mipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.extent = vulkanSurface.extent;
        imageCreateInfo.usage = imageUsageFlags | vk::ImageUsageFlagBits::eTransferDst;

        // Will create the surface image
        surface_stage_to_device(ctx, vulkanSurface, imageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal, x * y * n, static_cast<const void*>(loaded));
    }

    // Add sampler
    surface_create_sampler(ctx, vulkanSurface);

    // Create image view
    static const vk::ImageUsageFlags VIEW_USAGE_FLAGS = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment;

    if (imageUsageFlags & VIEW_USAGE_FLAGS)
    {
        vk::ImageViewCreateInfo viewCreateInfo;
        viewCreateInfo.viewType = vk::ImageViewType::e2D;
        viewCreateInfo.image = vulkanSurface.image;
        viewCreateInfo.format = format;
        viewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, vulkanSurface.mipLevels, 0, vulkanSurface.layerCount };
        vulkanSurface.view = ctx.device.createImageView(viewCreateInfo);
    }

    vulkanSurface.allocationState = VulkanAllocationState::Loaded;

    debug_set_surface_name(ctx.device, vulkanSurface, vulkanSurface.debugName);

    return true;
}

bool surface_create_from_file(VulkanContext& ctx, VulkanSurface& vulkanSurface, const fs::path& filename, vk::Format format, vk::ImageUsageFlags imageUsageFlags, vk::ImageLayout imageLayout, bool forceLinear)
{
    if (!fs::exists(filename))
    {
        vulkanSurface.allocationState = VulkanAllocationState::Failed;
        return false;
    }

    std::shared_ptr<gli::texture2d> tex2Dptr;

    auto data = Zest::file_read(filename);
    if (data.empty())
    {
        vulkanSurface.allocationState = VulkanAllocationState::Failed;
        return false;
    }

    return surface_create_from_memory(ctx, vulkanSurface, filename, data.c_str(), data.size(), format, imageUsageFlags, imageLayout, forceLinear);
}

void surface_update_from_audio(VulkanContext& ctx, VulkanSurface& surface, bool& surfaceChanged, vk::CommandBuffer& commandBuffer)
{
    PROFILE_SCOPE(surface_update_audio);
    auto& audioCtx = Zing::GetAudioContext();

    auto updateSurface = [&](auto width, auto height) {
        if (surface.extent.width != width || surface.extent.height != height)
        {
            vulkan_surface_destroy(ctx, surface);

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

            vulkan_surface_create(ctx, surface, imageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal);

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

            debug_set_surface_name(ctx.device, surface, "Audio Analysis");
        }
    };

    if (audioCtx.analysisChannels.empty() || audioCtx.analysisReadGeneration.load() == audioCtx.analysisWriteGeneration.load())
    {
        // Ensure a blank surface so the shader always gets something
        if (surface.extent.width == 0 || surface.extent.height == 0)
        {
            surface.extent.width = 256;
            surface.extent.height = 1;
        }
        updateSurface(surface.extent.width, surface.extent.height);
    }

    audioCtx.analysisReadGeneration.store(audioCtx.analysisWriteGeneration.load());

    static std::vector<float> uploadCache;

    size_t bufferWidth = 512; // default width if no data
    const auto Channels = std::max(audioCtx.analysisChannels.size(), size_t(1));
    const auto BufferTypes = 2; // Spectrum + Audio
    const auto BufferHeight = Channels * BufferTypes;

    for (auto [Id, pAnalysis] : audioCtx.analysisChannels)
    {
        std::shared_ptr<Zing::AudioAnalysisData> spNewData;
        while (pAnalysis->analysisData.try_dequeue(spNewData))
        {
            if (pAnalysis->uiDataCache)
            {
                // Return to the pool
                pAnalysis->analysisDataCache.enqueue(pAnalysis->uiDataCache);
            }
            pAnalysis->uiDataCache = spNewData;
        }

        // Haven't got anything yet
        if (!pAnalysis->uiDataCache)
        {
            continue;
        }

        const auto& spectrumBuckets = pAnalysis->uiDataCache->spectrumBuckets;
        const auto& audio = pAnalysis->uiDataCache->audio;

        // Stereo, Audio and Spectrum (4 rows total)
        if (spectrumBuckets.size() != 0)
        {
            bufferWidth = spectrumBuckets.size();
            uploadCache.resize(BufferHeight * spectrumBuckets.size());

            memcpy(&uploadCache[pAnalysis->thisChannel.second * spectrumBuckets.size()], &spectrumBuckets[0], spectrumBuckets.size() * sizeof(float));

            // Copy audio, note that we always make the audio at least as big as the spectrum
            assert(audio.size() >= spectrumBuckets.size());
            memcpy(&uploadCache[(pAnalysis->thisChannel.second + Channels) * spectrumBuckets.size()], &audio[0], spectrumBuckets.size() * sizeof(float));
        }
        else
        {
            // Make sure upload cache matches: it's OK that it might be empty
            uploadCache.resize(bufferWidth * BufferHeight, 0.0f);
            memset(&uploadCache[0], 0, uploadCache.size() * sizeof(float));
        }
    }

    // Left/Right FFT and Audio (4 rows, typically - though there is scope for more)
    updateSurface(bufferWidth, Channels * BufferTypes);

    utils_copy_to_memory(ctx, surface.stagingBuffer.memory, uploadCache);

    // TODO: This is a little inefficient; need to use the scene command buffer, or a copy queue and sync...
    // On the other hand, we aren't going for AAA game engine here.
    vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

    // Prepare for transfer
    surface_set_layout(ctx, commandBuffer, surface.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, range);

    // Prepare for transfer
    std::vector<vk::BufferImageCopy> bufferCopyRegions;
    {
        vk::BufferImageCopy bufferCopyRegion;
        bufferCopyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent = surface.extent;
        bufferCopyRegions.push_back(bufferCopyRegion);
    }
    commandBuffer.copyBufferToImage(surface.stagingBuffer.buffer, surface.image, vk::ImageLayout::eTransferDstOptimal, bufferCopyRegions);
}

} // namespace vulkan
