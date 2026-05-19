#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <cstring>
#include <exception>
#include <fmt/format.h>
#include <vector>

#include <spirv_msl.hpp>

#include <zest/file/file.h>
#include <zest/file/runtree.h>
#include <zest/logger/logger.h>

#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_scene.h>
#include <vklive/metal/metal_shader.h>
#include <vklive/process/process.h>
#include <vklive/shader_compiler.h>

namespace
{

template <typename T>
T bridge(void* object)
{
    return (__bridge T)object;
}

void release_obj(void*& object)
{
    if (object)
    {
        id releasedObject = CFBridgingRelease(object);
        releasedObject = nil;
        object = nullptr;
    }
}

void retain_obj(void*& storage, id object)
{
    release_obj(storage);
    storage = object ? (__bridge_retained void*)object : nullptr;
}

std::string ns_string(NSString* string)
{
    return string ? std::string([string UTF8String]) : std::string();
}

void report_shader_error(metal::MetalScene& scene, const std::string& text, const fs::path& path = fs::path())
{
    if (scene.pScene)
    {
        scene_report_error(*scene.pScene, MessageSeverity::Error, text, path);
    }
}

bool is_supported_stage(const fs::path& path)
{
    return path.extension() == ".vert" || path.extension() == ".frag";
}

metal::MetalShaderStage stage_from_path(const fs::path& path)
{
    return path.extension() == ".frag" ? metal::MetalShaderStage::Fragment : metal::MetalShaderStage::Vertex;
}

spv::ExecutionModel execution_model_from_stage(metal::MetalShaderStage stage)
{
    switch (stage)
    {
    case metal::MetalShaderStage::Vertex:
        return spv::ExecutionModelVertex;
    case metal::MetalShaderStage::Fragment:
        return spv::ExecutionModelFragment;
    default:
        return spv::ExecutionModelMax;
    }
}

bool binding_uses_buffer(ShaderBindingType type)
{
    switch (type)
    {
    case ShaderBindingType::UniformTexelBuffer:
    case ShaderBindingType::StorageTexelBuffer:
    case ShaderBindingType::UniformBuffer:
    case ShaderBindingType::StorageBuffer:
    case ShaderBindingType::UniformBufferDynamic:
    case ShaderBindingType::StorageBufferDynamic:
        return true;
    default:
        return false;
    }
}

bool binding_uses_texture(ShaderBindingType type)
{
    switch (type)
    {
    case ShaderBindingType::CombinedImageSampler:
    case ShaderBindingType::SampledImage:
    case ShaderBindingType::StorageImage:
    case ShaderBindingType::InputAttachment:
        return true;
    default:
        return false;
    }
}

bool binding_uses_sampler(ShaderBindingType type)
{
    switch (type)
    {
    case ShaderBindingType::Sampler:
    case ShaderBindingType::CombinedImageSampler:
        return true;
    default:
        return false;
    }
}

void assign_resource_bindings(metal::MetalShader& shader)
{
    // Slot 0 is reserved for vertex data in Metal raster passes; reflected buffers start after it.
    uint32_t nextBufferIndex = 1;
    uint32_t nextTextureIndex = 0;
    uint32_t nextSamplerIndex = 0;

    for (auto& [setIndex, bindingSet] : shader.bindingSets)
    {
        for (auto& [bindingIndex, reflectedBinding] : bindingSet.bindings)
        {
            metal::MetalShaderResourceBinding metalBinding;
            metalBinding.set = setIndex;
            metalBinding.binding = bindingIndex;
            metalBinding.type = reflectedBinding.type;

            // Descriptor set is part of the sorted key, so set/binding pairs map to stable dense Metal slots
            // without flattening different descriptor sets onto the same binding number.
            if (binding_uses_buffer(reflectedBinding.type))
            {
                metalBinding.bufferIndex = nextBufferIndex++;
            }
            if (binding_uses_texture(reflectedBinding.type))
            {
                metalBinding.textureIndex = nextTextureIndex++;
            }
            if (binding_uses_sampler(reflectedBinding.type))
            {
                metalBinding.samplerIndex = nextSamplerIndex++;
            }

            shader.resourceBindings[{ setIndex, bindingIndex }] = metalBinding;
        }
    }
}

void add_spirv_cross_resource_bindings(spirv_cross::CompilerMSL& compiler, const metal::MetalShader& shader)
{
    auto executionModel = execution_model_from_stage(shader.stage);
    for (auto& [_, metalBinding] : shader.resourceBindings)
    {
        spirv_cross::MSLResourceBinding resourceBinding;
        resourceBinding.stage = executionModel;
        resourceBinding.desc_set = metalBinding.set;
        resourceBinding.binding = metalBinding.binding;
        if (binding_uses_buffer(metalBinding.type))
        {
            resourceBinding.msl_buffer = metalBinding.bufferIndex;
        }
        if (binding_uses_texture(metalBinding.type))
        {
            resourceBinding.msl_texture = metalBinding.textureIndex;
        }
        if (binding_uses_sampler(metalBinding.type))
        {
            resourceBinding.msl_sampler = metalBinding.samplerIndex;
        }
        compiler.add_msl_resource_binding(resourceBinding);
    }
}

bool compile_glsl_to_spirv(metal::MetalScene& scene, Shader& shader, const fs::path& outPath)
{
    fs::path compilerPath;
#ifdef WIN32
    compilerPath = Zest::runtree_find_path("bin/win/glslangValidator.exe");
#elif defined(__APPLE__)
    compilerPath = Zest::runtree_find_path("bin/mac/glslangValidator");
#elif defined(__linux__)
    compilerPath = Zest::runtree_find_path("bin/linux/glslangValidator");
#endif

    std::vector<std::string> args{
        compilerPath.string(),
        "-V",
        shader.path.string(),
        "-o",
        outPath.string(),
        "-l",
        "-g",
        fmt::format("-I{}", fs::canonical(shader.path.parent_path()).string()),
        fmt::format("-I{}", fs::canonical(Zest::runtree_path() / "shaders/include").string()),
    };

    std::string output;
    auto ret = run_process(args, &output);
    bool outputErrors = shader_parse_output(output, shader.path, *scene.pScene);
    if (ret || outputErrors)
    {
        if (ret && !outputErrors)
        {
            report_shader_error(scene, fmt::format("Could not run glslangValidator for shader: {}", shader.path.filename().string()), shader.path);
        }
        LOG(DBG, "Could not run glslangValidator");
        return false;
    }

    return true;
}

bool read_spirv(metal::MetalScene& scene, Shader& shader, const fs::path& path, std::vector<uint32_t>& spirv)
{
    auto spirvBytes = Zest::file_read(path);
    if (spirvBytes.empty())
    {
        report_shader_error(scene, fmt::format("Could not get spirv for shader: {}", shader.path.filename().string()), shader.path);
        return false;
    }
    if ((spirvBytes.size() % sizeof(uint32_t)) != 0)
    {
        report_shader_error(scene, fmt::format("Invalid spirv byte size for shader: {}", shader.path.filename().string()), shader.path);
        return false;
    }

    spirv.resize(spirvBytes.size() / sizeof(uint32_t));
    std::memcpy(spirv.data(), spirvBytes.data(), spirvBytes.size());
    return true;
}

bool translate_spirv_to_msl(metal::MetalScene& scene, metal::MetalShader& shader, const std::vector<uint32_t>& spirv)
{
    try
    {
        spirv_cross::CompilerMSL compiler(spirv);

        auto options = compiler.get_msl_options();
        options.platform = spirv_cross::CompilerMSL::Options::macOS;
        options.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(2, 0);
        compiler.set_msl_options(options);

        shader.entryPointName = "main0";
        auto executionModel = execution_model_from_stage(shader.stage);
        compiler.rename_entry_point("main", shader.entryPointName, executionModel);
        compiler.set_entry_point(shader.entryPointName, executionModel);

        add_spirv_cross_resource_bindings(compiler, shader);

        shader.mslSource = compiler.compile();
        return true;
    }
    catch (const std::exception& ex)
    {
        report_shader_error(scene, fmt::format("Could not translate SPIR-V to MSL for shader '{}': {}", shader.pShader->path.filename().string(), ex.what()), shader.pShader->path);
        return false;
    }
}

bool compile_msl_library(metal::MetalContext& ctx, metal::MetalScene& scene, metal::MetalShader& shader)
{
    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!device)
    {
        report_shader_error(scene, "Could not compile Metal shader: Metal device is not available.", shader.pShader->path);
        return false;
    }

    NSError* error = nil;
    NSString* source = [NSString stringWithUTF8String:shader.mslSource.c_str()];
    MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
    id<MTLLibrary> library = [device newLibraryWithSource:source options:options error:&error];
    if (!library)
    {
        report_shader_error(scene, fmt::format("Could not compile Metal shader '{}': {}", shader.pShader->path.filename().string(), ns_string([error localizedDescription])), shader.pShader->path);
        return false;
    }

    NSString* functionName = [NSString stringWithUTF8String:shader.entryPointName.c_str()];
    id<MTLFunction> function = [library newFunctionWithName:functionName];
    if (!function)
    {
        report_shader_error(scene, fmt::format("Could not create Metal shader function '{}' for shader '{}'.", shader.entryPointName, shader.pShader->path.filename().string()), shader.pShader->path);
        return false;
    }

    retain_obj(shader.library, library);
    retain_obj(shader.function, function);
    return true;
}

} // namespace

namespace metal
{

std::shared_ptr<MetalShader> metal_shader_create(MetalContext& ctx, MetalScene& scene, Shader& shader)
{
    if (!is_supported_stage(shader.path))
    {
        report_shader_error(scene, fmt::format("Metal does not support shader stage '{}'. Only .vert and .frag shaders are accepted.", shader.path.filename().string()), shader.path);
        return nullptr;
    }

    auto spShader = std::make_shared<MetalShader>(&shader);
    spShader->stage = stage_from_path(shader.path);

    auto outPath = fs::temp_directory_path() / "vklive";
    fs::create_directories(outPath);
    outPath = outPath / (shader.path.filename().string() + ".metal.spirv");

    if (!compile_glsl_to_spirv(scene, shader, outPath))
    {
        return nullptr;
    }

    std::vector<uint32_t> spirv;
    if (!read_spirv(scene, shader, outPath, spirv))
    {
        return nullptr;
    }

    if (!shader_reflect_binding_sets(spirv, shader.path, spShader->bindingSets))
    {
        report_shader_error(scene, fmt::format("Could not reflect spirv for shader: {}", shader.path.filename().string()), shader.path);
        return nullptr;
    }
    assign_resource_bindings(*spShader);

    if (!translate_spirv_to_msl(scene, *spShader, spirv))
    {
        return nullptr;
    }

    if (!compile_msl_library(ctx, scene, *spShader))
    {
        metal_shader_destroy(ctx, *spShader);
        return nullptr;
    }

    scene.shaderStages[shader.path] = spShader;
    return spShader;
}

void metal_shader_destroy(MetalContext& ctx, MetalShader& shader)
{
    (void)ctx;
    release_obj(shader.function);
    release_obj(shader.library);
}

} // namespace metal
