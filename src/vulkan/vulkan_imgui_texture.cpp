#include <cassert>
#include <vklive/vulkan/vulkan_imgui_texture.h>
#include <vklive/vulkan/vulkan_imgui.h>
#include <zest/ui/fonts.h>


//#include <imgui_impl_vulkan.h>

namespace
{
static uint32_t GetMemoryType(VkPhysicalDevice physicalDevice, VkMemoryPropertyFlags properties, uint32_t type_bits)
{
    VkPhysicalDeviceMemoryProperties prop;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &prop);
    for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
        if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1 << i))
            return i;
    return 0xFFFFFFFF; // Unable to find memoryType
}
} // namespace

namespace vulkan
{
VulkanImGuiTexture::VulkanImGuiTexture(VulkanContext& ctx, VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, VkDescriptorPool pool)
    : m_physicalDevice(physicalDevice)
    , m_device(device)
    , m_queue(queue)
    , m_pool(pool)
    , m_ctx(ctx)
{
}

int VulkanImGuiTexture::UpdateTexture(int image, int x, int y, int updateWidth, int updateHeight, const unsigned char* data)
{
    auto itr = m_mapFonts.find(image);
    if (itr == m_mapFonts.end())
    {
        assert(!"Texture not found?");
        return 0;
    }

    auto& textureInfo = *itr->second;
    size_t upload_size = textureInfo.width * textureInfo.height * sizeof(uint32_t);

    if (true) //! textureInfo.init)
    {
        textureInfo.init = true;
        x = 0;
        y = 0;
        updateWidth = textureInfo.width;
        updateHeight = textureInfo.height;
    }

    // Upload to Buffer:
    uint32_t* map = NULL;
    auto err = vkMapMemory(m_device, textureInfo.uploadMemory, 0, upload_size, 0, (void**)(&map));
    assert(err == VK_SUCCESS);

    for (uint32_t yy = y; yy < uint32_t(y + updateHeight); yy++)
    {
        for (uint32_t xx = x; xx < uint32_t(x + updateWidth); xx++)
        {
            auto val = uint32_t(data[(yy - y) * textureInfo.width + (xx - x)]);
            map[(yy * (textureInfo.width)) + xx] = (val | val << 24 | val << 16 | val << 8);
        }
    }

    // memcpy(map, data, upload_size);
    VkMappedMemoryRange range[1] = {};
    range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range[0].memory = textureInfo.uploadMemory;
    range[0].size = upload_size;
    err = vkFlushMappedMemoryRanges(m_device, 1, range);
    assert(err == VK_SUCCESS);
    vkUnmapMemory(m_device, textureInfo.uploadMemory);

    err = vkResetCommandPool(m_device, textureInfo.commandPool, 0);
    assert(err == VK_SUCCESS);

    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(textureInfo.commandBuffer, &info);
    assert(err == VK_SUCCESS);

    // Copy to Image:
    VkImageMemoryBarrier copy_barrier[1] = {};
    copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    copy_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    copy_barrier[0].image = textureInfo.image;
    copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_barrier[0].subresourceRange.levelCount = 1;
    copy_barrier[0].subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(textureInfo.commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, copy_barrier);

    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = textureInfo.width;
    region.imageExtent.height = textureInfo.height;
    region.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(textureInfo.commandBuffer, textureInfo.uploadBuffer, textureInfo.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier use_barrier[1] = {};
    use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    use_barrier[0].image = textureInfo.image;
    use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    use_barrier[0].subresourceRange.levelCount = 1;
    use_barrier[0].subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(textureInfo.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, use_barrier);

    err = vkEndCommandBuffer(textureInfo.commandBuffer);

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &textureInfo.commandBuffer;
    assert(err == VK_SUCCESS);

    err = vkQueueSubmit(m_queue, 1, &end_info, VK_NULL_HANDLE);
    assert(err == VK_SUCCESS);

    return image;
}

void VulkanImGuiTexture::DeleteTexture(int image)
{
    auto itr = m_mapFonts.find(image);
    if (itr == m_mapFonts.end())
    {
        assert(!"Texture not found?");
        return;
    }

    m_mapOldFonts[m_frameIndex].push_back(itr->second);
    m_mapFonts.erase(itr);
}

void VulkanImGuiTexture::Flush()
{
    std::vector<uint64_t> victims;
    for (auto& [frameIndex, fonts] : m_mapOldFonts)
    {
        if ((frameIndex + 3) >= m_frameIndex)
        {
            continue;
        }

        victims.push_back(frameIndex);

        for (auto& spInfo : fonts)
        {
            auto& fontInfo = *spInfo;
            if (fontInfo.imageView)
            {
                vkDestroyImageView(m_device, fontInfo.imageView, nullptr);
            }
            if (fontInfo.image)
            {
                vkDestroyImage(m_device, fontInfo.image, nullptr);
            }
            if (fontInfo.memory)
            {
                vkFreeMemory(m_device, fontInfo.memory, nullptr);
            }
            if (fontInfo.sampler)
            {
                vkDestroySampler(m_device, fontInfo.sampler, nullptr);
            }
            if (fontInfo.uploadBuffer)
            {
                vkDestroyBuffer(m_device, fontInfo.uploadBuffer, nullptr);
            }
            if (fontInfo.uploadMemory)
            {
                vkFreeMemory(m_device, fontInfo.uploadMemory, nullptr);
            }
            // TODO: Use shared or engine command buffer
            if (fontInfo.commandBuffer)
            {
                vkDestroyCommandPool(m_device, fontInfo.commandPool, nullptr);
            }
        }
    }
    for (auto& victim : victims)
    {
        m_mapOldFonts.erase(victim);
    }
}

int VulkanImGuiTexture::CreateTexture(int width, int height, const unsigned char* data)
{
    size_t upload_size = width * height * sizeof(uint32_t);

    VkResult err;

    auto spFontInfo = std::make_shared<FontInfo>();
    spFontInfo->width = width;
    spFontInfo->height = height;

    // Sampler
    VkSamplerCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.minLod = -1000;
    info.maxLod = 1000;
    info.maxAnisotropy = 1.0f;
    err = vkCreateSampler(m_device, &info, nullptr, &spFontInfo->sampler);
    assert(err == VK_SUCCESS);

    // Create the Image:
    {
        VkImageCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = VK_FORMAT_R8G8B8A8_UNORM;
        info.extent.width = width;
        info.extent.height = height;
        info.extent.depth = 1;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        err = vkCreateImage(m_device, &info, nullptr, &spFontInfo->image);
        assert(err == VK_SUCCESS);
        // check_vk_result(err);
        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(m_device, spFontInfo->image, &req);
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;
        alloc_info.memoryTypeIndex = GetMemoryType(m_physicalDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
        err = vkAllocateMemory(m_device, &alloc_info, nullptr, &spFontInfo->memory);
        assert(err == VK_SUCCESS);
        err = vkBindImageMemory(m_device, spFontInfo->image, spFontInfo->memory, 0);
        assert(err == VK_SUCCESS);
    }

    // Create the Image View:
    { 
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = spFontInfo->image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = VK_FORMAT_R8G8B8A8_UNORM;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
        err = vkCreateImageView(m_device, &info, nullptr, &spFontInfo->imageView);
        assert(err == VK_SUCCESS);
    }

    // Create the Descriptor Set:
    auto descriptor_set = m_ctx.device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo(m_ctx.descriptorPool, imgui_get_texture_layout(m_ctx)));
    spFontInfo->descriptorSet = descriptor_set[0];

    // Update the Descriptor Set:
    {
        vk::DescriptorImageInfo desc_image;
        desc_image.sampler = spFontInfo->sampler;
        desc_image.imageView = spFontInfo->imageView;
        desc_image.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        vk::WriteDescriptorSet write_desc;
        write_desc.dstSet = (vk::DescriptorSet)descriptor_set[0];
        write_desc.descriptorCount = 1;
        write_desc.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write_desc.setImageInfo(desc_image);
        m_ctx.device.updateDescriptorSets(write_desc, {});
    }

    // Create the Upload Buffer:
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = upload_size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    err = vkCreateBuffer(m_device, &buffer_info, nullptr, &spFontInfo->uploadBuffer);
    assert(err == VK_SUCCESS);
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(m_device, spFontInfo->uploadBuffer, &req);
    spFontInfo->memoryAlignment = (spFontInfo->memoryAlignment > req.alignment) ? spFontInfo->memoryAlignment : req.alignment;
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = GetMemoryType(m_physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
    err = vkAllocateMemory(m_device, &alloc_info, nullptr, &spFontInfo->uploadMemory);
    assert(err == VK_SUCCESS);
    err = vkBindBufferMemory(m_device, spFontInfo->uploadBuffer, spFontInfo->uploadMemory, 0);
    assert(err == VK_SUCCESS);

    uint32_t queueFamily = 0;
    {
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &count, NULL);
        std::vector<VkQueueFamilyProperties> queues(count);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &count, &queues[0]);
        for (uint32_t i = 0; i < count; i++)
        {
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                queueFamily = i;
                break;
            }
        }
    }

    {
        VkCommandPoolCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        info.queueFamilyIndex = queueFamily;
        err = vkCreateCommandPool(m_device, &info, nullptr, &spFontInfo->commandPool);
        assert(err == VK_SUCCESS);
    }

    {
        VkCommandBufferAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandPool = spFontInfo->commandPool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = 1;
        err = vkAllocateCommandBuffers(m_device, &info, &spFontInfo->commandBuffer);
        assert(err == VK_SUCCESS);
    }

    // Store our identifier
    // io.Fonts->SetTexID((ImTextureID)bd->FontDescriptorSet);

    spFontInfo->textureId = m_currentTextureId++;
    m_mapFonts[spFontInfo->textureId] = spFontInfo;

    return spFontInfo->textureId;
}

void VulkanImGuiTexture::GetTextureSize(int image, int* w, int* h)
{
    if (!w || !h)
    {
        assert(!"!Must request dimensions?");
        return;
    }

    *w = *h = 0;

    auto itr = m_mapFonts.find(image);
    if (itr == m_mapFonts.end())
    {
        return;
    }

    *w = itr->second->width;
    *h = itr->second->height;
}

std::vector<void*> VulkanImGuiTexture::GetTextures()
{
    std::vector<void*> vals;
    for (auto& [id, t] : m_mapFonts)
    {
        vals.push_back((void*)t->descriptorSet);
    }
    return vals;
}

void* VulkanImGuiTexture::GetTexture(int image)
{
    auto itr = m_mapFonts.find(image);
    if (itr == m_mapFonts.end())
    {
        return nullptr;
    }
    return (void*)itr->second->descriptorSet;
}

void VulkanImGuiTexture::BeginFrame()
{
    Flush();
}

void VulkanImGuiTexture::EndFrame()
{
    m_frameIndex++;
}

} // namespace vulkan
