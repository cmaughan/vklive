#include <cstring>
#include <fmt/format.h>
#include <fstream>
#include <sstream>

// #define SPIRV_REFLECT_USE_SYSTEM_SPIRV_H
#include <spirv-reflect/spirv_reflect.h>

#include <zest/file/file.h>
#include <zest/file/runtree.h>
#include <zest/logger/logger.h>

#include "config_app.h"
#include <vklive/process/process.h>
#include <vklive/shader_compiler.h>
#include <vklive/vulkan/vulkan_reflect.h>
#include <vklive/vulkan/vulkan_shader.h>

namespace vulkan
{

ShaderBindingType spv_reflect_descriptor_type_to_shader_binding_type(SpvReflectDescriptorType type)
{
    switch (type)
    {
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
        return ShaderBindingType::Sampler;
    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return ShaderBindingType::CombinedImageSampler;
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return ShaderBindingType::SampledImage;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return ShaderBindingType::StorageImage;
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        return ShaderBindingType::UniformTexelBuffer;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return ShaderBindingType::StorageTexelBuffer;
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        return ShaderBindingType::UniformBuffer;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        return ShaderBindingType::StorageBuffer;
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        return ShaderBindingType::UniformBufferDynamic;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return ShaderBindingType::StorageBufferDynamic;
    case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return ShaderBindingType::InputAttachment;
    case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        return ShaderBindingType::AccelerationStructure;
    default:
        return ShaderBindingType::Unknown;
    }
}

ShaderStageFlags spv_reflect_shader_stage_to_shader_stage_flags(SpvReflectShaderStageFlagBits stage)
{
    switch (stage)
    {
    case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
        return static_cast<ShaderStageFlags>(ShaderStageBits::Vertex);
    case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
        return static_cast<ShaderStageFlags>(ShaderStageBits::Fragment);
    case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
        return static_cast<ShaderStageFlags>(ShaderStageBits::Geometry);
    case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
        return static_cast<ShaderStageFlags>(ShaderStageBits::Compute);
    case SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR:
        return static_cast<ShaderStageFlags>(ShaderStageBits::RayGen);
    case SPV_REFLECT_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        return static_cast<ShaderStageFlags>(ShaderStageBits::ClosestHit);
    case SPV_REFLECT_SHADER_STAGE_MISS_BIT_KHR:
        return static_cast<ShaderStageFlags>(ShaderStageBits::Miss);
    case SPV_REFLECT_SHADER_STAGE_ANY_HIT_BIT_KHR:
        return static_cast<ShaderStageFlags>(ShaderStageBits::AnyHit);
    default:
        return 0;
    }
}

bool shader_reflect(const std::string& spirv, VulkanShader& vulkanShader)
{
    SpvReflectShaderModule module = {};
    SpvReflectResult result = spvReflectCreateShaderModule(spirv.size(), spirv.c_str(), &module);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

#ifdef _DEBUG
    std::ostringstream str;
    const spv_reflect::ShaderModule mod(spirv.size(), spirv.c_str());
    WriteReflection(mod, false, str);
    LOG_SCOPE(DBG, str.str());
#endif

    uint32_t count = 0;
    result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    std::vector<SpvReflectDescriptorSet*> sets(count);
    result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        return false;
    }

    for (auto& set : sets)
    {
        for (uint32_t index = 0; index < set->binding_count; ++index)
        {
            auto& bindingReflect = *set->bindings[index];

            ShaderBinding shaderBinding;
            shaderBinding.binding = bindingReflect.binding;
            shaderBinding.type = spv_reflect_descriptor_type_to_shader_binding_type(bindingReflect.descriptor_type);
            shaderBinding.count = 1;
            for (uint32_t dim = 0; dim < bindingReflect.array.dims_count; dim++)
            {
                shaderBinding.count *= bindingReflect.array.dims[dim];
            }
            shaderBinding.stageFlags = spv_reflect_shader_stage_to_shader_stage_flags(module.shader_stage);
            vulkanShader.bindingSets[set->set].bindings[shaderBinding.binding] = shaderBinding;

            ShaderBindingMeta meta;
            meta.name = bindingReflect.name;
            meta.shaderPath = vulkanShader.pShader->path;
            // TODO: Can we provide the range here?
            // The reflection doesn't give us file offsets, so we would have to scan the file and find the declarations
            meta.line = 0;
            vulkanShader.bindingSets[set->set].bindingMeta[shaderBinding.binding] = meta;
        }
    }
    LOG_SCOPE(DBG, "Shader: " << vulkanShader.pShader->path.filename() << ", Bindings:");
    bindings_dump(vulkanShader.bindingSets);
    spvReflectDestroyShaderModule(&module);
    return true;
}

std::shared_ptr<VulkanShader> vulkan_shader_create(VulkanContext& ctx, VulkanScene& vulkanScene, Shader& shader)
{
    std::shared_ptr<VulkanShader> spShader = std::make_shared<VulkanShader>(&shader);

    auto out_path = fs::temp_directory_path() / "vklive";
    fs::create_directories(out_path);
    out_path = out_path / (shader.path.filename().string() + ".spirv");

    if (shader.path.extension().string() == ".vert")
    {
        spShader->shaderCreateInfo.stage = vk::ShaderStageFlagBits::eVertex;
    }
    else if (shader.path.extension().string() == ".frag")
    {
        spShader->shaderCreateInfo.stage = vk::ShaderStageFlagBits::eFragment;
    }
    else if (shader.path.extension().string() == ".geom")
    {
        spShader->shaderCreateInfo.stage = vk::ShaderStageFlagBits::eGeometry;
    }
    else if (shader.path.extension().string() == ".rchit")
    {
        spShader->shaderCreateInfo.stage = vk::ShaderStageFlagBits::eClosestHitKHR;
    }
    else if (shader.path.extension().string() == ".rgen")
    {
        spShader->shaderCreateInfo.stage = vk::ShaderStageFlagBits::eRaygenKHR;
    }
    else if (shader.path.extension().string() == ".rmiss")
    {
        spShader->shaderCreateInfo.stage = vk::ShaderStageFlagBits::eMissKHR;
    }
    else
    {
        scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Unknown shader type: {}", shader.path.filename().string()));
        return nullptr;
    }

    spShader->bindingSets.clear();
    spShader->shaderCreateInfo.module = nullptr;

    fs::path compiler_path;
    // fs::path cross_path;
#ifdef WIN32
    compiler_path = Zest::runtree_find_path("bin/win/glslangValidator.exe");
#elif defined(__APPLE__)
    compiler_path = Zest::runtree_find_path("bin/mac/glslangValidator");
#elif defined(__linux__)
    compiler_path = Zest::runtree_find_path("bin/linux/glslangValidator");
#endif
    std::vector<std::string> args{
        compiler_path.string(),
        "-V",
        shader.path.string(),
        "-o",
        out_path.string(),
        "-l",
        "-g",
        fmt::format("-I{}", fs::canonical(shader.path.parent_path()).string()),
        fmt::format("-I{}", fs::canonical(Zest::runtree_path() / "shaders/include").string()),
    };

    if (scene_is_raytracer(shader.path))
    {
        args.push_back("--target-env");
        args.push_back("vulkan1.2");
    }

    std::string output;
    auto ret = run_process(
        args,
        &output);
    if (ret)
    {
        LOG(DBG, "Could not run glslangConvertor");
        return nullptr;
    }

    if (shader_parse_output(output, shader.path, *vulkanScene.pScene))
    {
        return nullptr;
    }

    auto spirv = Zest::file_read(out_path);
    if (spirv.empty())
    {
        scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Could not get spirv for shader: {}", shader.path.filename().string()), shader.path);
        return nullptr;
    }

    if (!shader_reflect(spirv, *spShader))
    {
        scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Could not reflect spirv for shader: {}", shader.path.filename().string()), shader.path);
    }

    // Create the shader modules
    spShader->shaderCreateInfo.module = ctx.device.createShaderModule(
        vk::ShaderModuleCreateInfo({}, spirv.size(), (const uint32_t*)spirv.c_str()));

    debug_set_shadermodule_name(ctx.device,
        spShader->shaderCreateInfo.module,
        std::string("Shader::Module::") + shader.path.filename().string());

    if (spShader->shaderCreateInfo.module)
    {
        vulkanScene.shaderStages[shader.path] = spShader;
        spShader->shaderCreateInfo.pName = "main";
    }
    else
    {
        scene_report_error(*vulkanScene.pScene, MessageSeverity::Error, fmt::format("Could not create shader: {}", shader.path.filename().string()));
    }

    return spShader;
}

void vulkan_shader_destroy(VulkanContext& ctx, VulkanShader& shader)
{
    if (shader.shaderCreateInfo.module)
    {
        ctx.device.destroyShaderModule(shader.shaderCreateInfo.module);
    }
}

} // namespace vulkan
