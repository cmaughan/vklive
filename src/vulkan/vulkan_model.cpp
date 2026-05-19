#include <fmt/format.h>

#include <algorithm>
#include <array>

#include <zest/file/runtree.h>

#include "vklive/vulkan/vulkan_buffer.h"
#include "vklive/vulkan/vulkan_descriptor.h"
#include "vklive/vulkan/vulkan_model.h"
#include "vklive/vulkan/vulkan_surface.h"
#include "vklive/vulkan/vulkan_utils.h"

namespace vulkan
{

namespace
{

vk::DescriptorImageInfo vulkan_model_descriptor_for_texture(const VulkanModelTexture& texture)
{
    return vk::DescriptorImageInfo(texture.surface.sampler, texture.surface.view, vk::ImageLayout::eShaderReadOnlyOptimal);
}

VulkanModelTexture vulkan_model_create_solid_texture(VulkanContext& ctx, const std::string& debugName, const std::array<uint8_t, 4>& color, vk::Format format = vk::Format::eR8G8B8A8Unorm)
{
    VulkanModelTexture texture;
    texture.surface.debugName = debugName;
    texture.fallback = true;

    texture.surface.extent = vk::Extent3D(1, 1, 1);
    texture.surface.mipLevels = 1;
    texture.surface.layerCount = 1;

    vk::ImageCreateInfo imageCreateInfo;
    imageCreateInfo.imageType = vk::ImageType::e2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent = texture.surface.extent;
    imageCreateInfo.mipLevels = texture.surface.mipLevels;
    imageCreateInfo.arrayLayers = texture.surface.layerCount;
    imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
    imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
    imageCreateInfo.usage = vk::ImageUsageFlagBits::eSampled;
    imageCreateInfo.sharingMode = vk::SharingMode::eExclusive;
    imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;

    surface_stage_to_device(ctx, texture.surface, imageCreateInfo, vk::MemoryPropertyFlagBits::eDeviceLocal, color.size(), color.data());
    surface_create_sampler(ctx, texture.surface);

    vk::ImageViewCreateInfo viewCreateInfo;
    viewCreateInfo.viewType = vk::ImageViewType::e2D;
    viewCreateInfo.image = texture.surface.image;
    viewCreateInfo.format = format;
    viewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, texture.surface.mipLevels, 0, texture.surface.layerCount };
    texture.surface.view = ctx.device.createImageView(viewCreateInfo);

    debug_set_surface_name(ctx.device, texture.surface, texture.surface.debugName);
    texture.descriptor = vulkan_model_descriptor_for_texture(texture);
    return texture;
}

vk::Format vulkan_model_texture_format(const ModelTextureSlot& slot)
{
    return slot.srgb ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
}

vk::DescriptorImageInfo vulkan_model_load_texture_slot(VulkanContext& ctx,
    VulkanModel& model,
    const ModelTextureSlot& slot,
    const VulkanModelTexture& fallback,
    const std::string& debugPrefix)
{
    if (slot.embeddedTexture && !slot.embeddedTexture->data.empty())
    {
        VulkanModelTexture texture;
        texture.surface.debugName = fmt::format("{}:{}", debugPrefix, slot.pathName);
        texture.sourcePath = slot.pathName;
        texture.fallback = false;
        if (surface_create_from_memory(ctx,
                texture.surface,
                fs::path(slot.pathName),
                reinterpret_cast<const char*>(slot.embeddedTexture->data.data()),
                slot.embeddedTexture->data.size(),
                vulkan_model_texture_format(slot),
                vk::ImageUsageFlagBits::eSampled,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                false,
                slot.flipY))
        {
            texture.descriptor = vulkan_model_descriptor_for_texture(texture);
            model.materialTextures.push_back(texture);
            return model.materialTextures.back().descriptor;
        }
    }

    if (!slot.resolvedPath.empty())
    {
        VulkanModelTexture texture;
        texture.surface.debugName = fmt::format("{}:{}", debugPrefix, slot.resolvedPath.filename().string());
        texture.sourcePath = slot.resolvedPath;
        texture.fallback = false;
        if (surface_create_from_file(ctx, texture.surface, slot.resolvedPath, vulkan_model_texture_format(slot),
                vk::ImageUsageFlagBits::eSampled,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                false,
                slot.flipY))
        {
            texture.descriptor = vulkan_model_descriptor_for_texture(texture);
            model.materialTextures.push_back(texture);
            return model.materialTextures.back().descriptor;
        }
    }

    return fallback.descriptor;
}

void vulkan_model_destroy_material_resources(VulkanContext& ctx, VulkanModel& model)
{
    vulkan_buffer_destroy(ctx, model.materialsBuffer);

    for (auto& texture : model.materialTextures)
    {
        vulkan_surface_destroy(ctx, texture.surface);
    }
    model.materialTextures.clear();

    vulkan_surface_destroy(ctx, model.fallbackBaseColorTexture.surface);
    vulkan_surface_destroy(ctx, model.fallbackNormalTexture.surface);
    vulkan_surface_destroy(ctx, model.fallbackMetallicRoughnessTexture.surface);
    vulkan_surface_destroy(ctx, model.fallbackEmissiveTexture.surface);
    vulkan_surface_destroy(ctx, model.fallbackOcclusionTexture.surface);

    model.gpuMaterials.clear();
    model.baseColorTextureDescriptors.clear();
    model.normalTextureDescriptors.clear();
    model.metallicRoughnessTextureDescriptors.clear();
    model.emissiveTextureDescriptors.clear();
    model.occlusionTextureDescriptors.clear();
    model.materialDescriptorSet = nullptr;
    model.materialDescriptorSetLayout = nullptr;
}

} // namespace

std::shared_ptr<VulkanModel> vulkan_model_load(VulkanContext& ctx, const ModelCreateInfo& createInfo)
{
    // Call the model class
    auto itr = VulkanModel::ModelCache.find(createInfo);
    if (itr != VulkanModel::ModelCache.end())
    {
        model_load(*itr->second, createInfo);
        return itr->second;
    }

    auto pModel = std::make_shared<VulkanModel>();
    model_load(*pModel, createInfo);
    VulkanModel::ModelCache[createInfo] = pModel;
    return pModel;
}

void vulkan_model_stage(VulkanContext& ctx, VulkanModel& model)
{
    if (!model.vertexData.empty() && !model.indices.buffer)
    {
        // Vertex buffer
        // Index buffer
        model.vertices = buffer_stage_to_device(ctx, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer, model.vertexData);
        model.indices = buffer_stage_to_device(ctx, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer, model.indexData);

        debug_set_buffer_name(ctx.device, (VkBuffer)model.vertices.buffer, fmt::format("{}:Vertices", model.debugName));
        debug_set_buffer_name(ctx.device, (VkBuffer)model.indices.buffer, fmt::format("{}:Indices", model.debugName));

        debug_set_devicememory_name(ctx.device, model.vertices.memory, fmt::format("{}:VerticesMemory", model.debugName));
        debug_set_devicememory_name(ctx.device, model.indices.memory, fmt::format("{}:IndicesMemory", model.debugName));

        model.verticesDescriptor = vk::DescriptorBufferInfo(model.vertices.buffer, 0, VK_WHOLE_SIZE);
        model.indicesDescriptor = vk::DescriptorBufferInfo(model.indices.buffer, 0, VK_WHOLE_SIZE);

        // TODO: This is heavy handed, but we need to wait for vertices to be staged to the device
        //ctx.device.waitIdle();
    }

    vulkan_model_prepare_materials(ctx, model);
}

void vulkan_model_prepare_materials(VulkanContext& ctx, VulkanModel& model)
{
    if (model.materialsBuffer.buffer || model.vertexData.empty())
    {
        return;
    }

    model.fallbackBaseColorTexture = vulkan_model_create_solid_texture(ctx, fmt::format("{}:FallbackBaseColor", model.debugName), { 255, 255, 255, 255 }, vk::Format::eR8G8B8A8Srgb);
    model.fallbackNormalTexture = vulkan_model_create_solid_texture(ctx, fmt::format("{}:FallbackNormal", model.debugName), { 128, 128, 255, 255 });
    model.fallbackMetallicRoughnessTexture = vulkan_model_create_solid_texture(ctx, fmt::format("{}:FallbackMetallicRoughness", model.debugName), { 0, 255, 0, 255 });
    model.fallbackEmissiveTexture = vulkan_model_create_solid_texture(ctx, fmt::format("{}:FallbackEmissive", model.debugName), { 0, 0, 0, 255 }, vk::Format::eR8G8B8A8Srgb);
    model.fallbackOcclusionTexture = vulkan_model_create_solid_texture(ctx, fmt::format("{}:FallbackOcclusion", model.debugName), { 255, 255, 255, 255 });

    model.baseColorTextureDescriptors.assign(VulkanMaxMaterials, model.fallbackBaseColorTexture.descriptor);
    model.normalTextureDescriptors.assign(VulkanMaxMaterials, model.fallbackNormalTexture.descriptor);
    model.metallicRoughnessTextureDescriptors.assign(VulkanMaxMaterials, model.fallbackMetallicRoughnessTexture.descriptor);
    model.emissiveTextureDescriptors.assign(VulkanMaxMaterials, model.fallbackEmissiveTexture.descriptor);
    model.occlusionTextureDescriptors.assign(VulkanMaxMaterials, model.fallbackOcclusionTexture.descriptor);

    model.gpuMaterials.resize(VulkanMaxMaterials);

    for (uint32_t i = 0; i < VulkanMaxMaterials; ++i)
    {
        VulkanGpuMaterial gpuMaterial;
        if (i < model.materials.size())
        {
            const auto& material = model.materials[i];
            gpuMaterial.baseColorFactor = material.baseColorFactor;
            gpuMaterial.emissiveFactor = material.emissiveFactor;
            gpuMaterial.metallicRoughnessOcclusion = glm::vec4(material.metallicFactor, material.roughnessFactor, material.occlusionStrength, 0.0f);
            gpuMaterial.textureIndices = glm::ivec4(static_cast<int32_t>(i));

            const auto debugPrefix = fmt::format("{}:{}", model.debugName, material.name);
            model.baseColorTextureDescriptors[i] = vulkan_model_load_texture_slot(ctx, model, material.textures.baseColor, model.fallbackBaseColorTexture, debugPrefix);
            model.normalTextureDescriptors[i] = vulkan_model_load_texture_slot(ctx, model, material.textures.normal, model.fallbackNormalTexture, debugPrefix);
            model.metallicRoughnessTextureDescriptors[i] = vulkan_model_load_texture_slot(ctx, model, material.textures.metallicRoughness, model.fallbackMetallicRoughnessTexture, debugPrefix);
            model.emissiveTextureDescriptors[i] = vulkan_model_load_texture_slot(ctx, model, material.textures.emissive, model.fallbackEmissiveTexture, debugPrefix);
            model.occlusionTextureDescriptors[i] = vulkan_model_load_texture_slot(ctx, model, material.textures.occlusion, model.fallbackOcclusionTexture, debugPrefix);
        }

        model.gpuMaterials[i] = gpuMaterial;
    }

    model.materialsBuffer = buffer_stage_to_device(ctx, vk::BufferUsageFlagBits::eStorageBuffer, model.gpuMaterials);
    model.materialsDescriptor = vk::DescriptorBufferInfo(model.materialsBuffer.buffer, 0, VK_WHOLE_SIZE);
}

bool vulkan_model_prepare_material_descriptors(VulkanContext& ctx, VulkanModel& model, vk::DescriptorSetLayout layout)
{
    if (!layout)
    {
        return false;
    }

    vulkan_model_prepare_materials(ctx, model);
    if (!model.materialsBuffer.buffer ||
        model.baseColorTextureDescriptors.size() < VulkanMaxMaterials ||
        model.normalTextureDescriptors.size() < VulkanMaxMaterials ||
        model.metallicRoughnessTextureDescriptors.size() < VulkanMaxMaterials ||
        model.emissiveTextureDescriptors.size() < VulkanMaxMaterials ||
        model.occlusionTextureDescriptors.size() < VulkanMaxMaterials)
    {
        return false;
    }

    vk::DescriptorSet descriptorSet;
    if (!descriptor_allocate(ctx, descriptor_get_cache(ctx), &descriptorSet, layout))
    {
        return false;
    }

    std::array<vk::WriteDescriptorSet, 6> writes{};

    writes[0].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[0].descriptorCount = 1;
    writes[0].dstSet = descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].pBufferInfo = &model.materialsDescriptor;

    writes[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[1].descriptorCount = VulkanMaxMaterials;
    writes[1].dstSet = descriptorSet;
    writes[1].dstBinding = 1;
    writes[1].pImageInfo = model.baseColorTextureDescriptors.data();

    writes[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[2].descriptorCount = VulkanMaxMaterials;
    writes[2].dstSet = descriptorSet;
    writes[2].dstBinding = 2;
    writes[2].pImageInfo = model.normalTextureDescriptors.data();

    writes[3].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[3].descriptorCount = VulkanMaxMaterials;
    writes[3].dstSet = descriptorSet;
    writes[3].dstBinding = 3;
    writes[3].pImageInfo = model.metallicRoughnessTextureDescriptors.data();

    writes[4].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[4].descriptorCount = VulkanMaxMaterials;
    writes[4].dstSet = descriptorSet;
    writes[4].dstBinding = 4;
    writes[4].pImageInfo = model.emissiveTextureDescriptors.data();

    writes[5].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writes[5].descriptorCount = VulkanMaxMaterials;
    writes[5].dstSet = descriptorSet;
    writes[5].dstBinding = 5;
    writes[5].pImageInfo = model.occlusionTextureDescriptors.data();

    ctx.device.updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    model.materialDescriptorSet = descriptorSet;
    model.materialDescriptorSetLayout = layout;
    debug_set_descriptorset_name(ctx.device, model.materialDescriptorSet, fmt::format("{}:MaterialDescriptorSet", model.debugName));
    return true;
}

vk::Format component_format(Component component)
{
    switch (component)
    {
    case VERTEX_COMPONENT_UV:
        return vk::Format::eR32G32Sfloat;
    case VERTEX_COMPONENT_DUMMY_FLOAT:
        return vk::Format::eR32Sfloat;
    case VERTEX_COMPONENT_DUMMY_INT:
        return vk::Format::eR32Sint;
    case VERTEX_COMPONENT_DUMMY_VEC4:
        return vk::Format::eR32G32B32A32Sfloat;
    case VERTEX_COMPONENT_DUMMY_INT4:
        return vk::Format::eR32G32B32A32Sint;
    case VERTEX_COMPONENT_DUMMY_UINT4:
        return vk::Format::eR32G32B32A32Uint;
    default:
        return vk::Format::eR32G32B32Sfloat;
    }
}

void vulkan_model_destroy(VulkanContext& ctx, VulkanModel& model)
{
    model.refCount--;
    if (model.refCount <= 0)
    {
        vulkan_buffer_destroy(ctx, model.vertices);
        vulkan_buffer_destroy(ctx, model.indices);
        vulkan_model_destroy_material_resources(ctx, model);

        // AS
        for (auto& as : model.accelerationStructures)
        {
            vulkan_buffer_destroy(ctx, as.buffer);
            if (as.handle)
            {
                ctx.device.destroyAccelerationStructureKHR(as.handle);
            }
        }

        VulkanModel::ModelCache.erase(model.createInfo);
    }
}

std::shared_ptr<VulkanModel> vulkan_model_create(VulkanContext& ctx,
    VulkanScene& vulkanScene,
    const Geometry& geom)
{
    auto spVulkanModel = std::make_shared<VulkanModel>();

    // The geometry may be user geom or a model loaded from run tree; so just check it is available
    fs::path loadPath;
    if (geom.type == GeometryType::Model)
    {
        loadPath = geom.path;
    }
    else if (geom.type == GeometryType::Rect)
    {
        loadPath = Zest::runtree_find_path("models/quad.obj");
        if (loadPath.empty())
        {
            scene_report_error(*vulkanScene.pScene, MessageSeverity::Error,
                fmt::format("Could not load default asset: {}", "runtree/models/quad.obj"));
            return spVulkanModel;
        }
    }

    ModelCreateInfo createInfo
    {
        .filename = loadPath.string(),
        .scale = geom.loadScale,
        .uvOrigin = geom.uvOrigin,
        .buildAS = geom.buildAS
    };
    spVulkanModel = vulkan_model_load(ctx, createInfo);
    spVulkanModel->debugName = fmt::format("Model:{}", loadPath.filename().string());

    // Success?
    if (spVulkanModel->vertexData.empty())
    {
        auto txt = fmt::format("Could not load model: {}", loadPath.string());
        if (!spVulkanModel->errors.empty())
        {
            txt += "\n" + spVulkanModel->errors;
        }
        scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, txt);
    }
    else
    {
        // Store at original path, even though we may have subtituted geometry for preset paths
        spVulkanModel->refCount++;
        vulkanScene.models[geom.path] = spVulkanModel;
    }

    return spVulkanModel;
}

} // namespace vulkan
