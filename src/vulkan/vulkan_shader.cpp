#include <fmt/format.h>
#include <fstream>
#include <sstream>

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

bool shader_reflect(const std::string& spirv, VulkanShader& vulkanShader)
{
#ifdef _DEBUG
    std::ostringstream str;
    const spv_reflect::ShaderModule mod(spirv.size(), spirv.c_str());
    WriteReflection(mod, false, str);
    LOG_SCOPE(DBG, str.str());
#endif

    if (!shader_reflect_binding_sets(spirv.c_str(), spirv.size(), vulkanShader.pShader->path, vulkanShader.bindingSets))
    {
        return false;
    }

    LOG_SCOPE(DBG, "Shader: " << vulkanShader.pShader->path.filename() << ", Bindings:");
    bindings_dump(vulkanShader.bindingSets);
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
