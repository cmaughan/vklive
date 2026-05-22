#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <cstring>
#include <exception>
#include <fmt/format.h>
#include <limits>
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

const fs::path& shader_path(const metal::MetalShader& shader)
{
    if (!shader.path.empty())
    {
        return shader.path;
    }
    static const fs::path emptyPath;
    return shader.pShader ? shader.pShader->path : emptyPath;
}

spv::ExecutionModel execution_model_from_stage(metal::MetalShaderStage stage)
{
    switch (stage)
    {
    case metal::MetalShaderStage::Vertex:
        return spv::ExecutionModelVertex;
    case metal::MetalShaderStage::Fragment:
        return spv::ExecutionModelFragment;
    case metal::MetalShaderStage::RayCompute:
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

spirv_cross::SPIRType::BaseType binding_base_type(ShaderBindingType type)
{
    switch (type)
    {
    case ShaderBindingType::CombinedImageSampler:
        return spirv_cross::SPIRType::SampledImage;
    case ShaderBindingType::SampledImage:
    case ShaderBindingType::StorageImage:
    case ShaderBindingType::InputAttachment:
        return spirv_cross::SPIRType::Image;
    case ShaderBindingType::Sampler:
        return spirv_cross::SPIRType::Sampler;
    case ShaderBindingType::UniformTexelBuffer:
    case ShaderBindingType::StorageTexelBuffer:
    case ShaderBindingType::UniformBuffer:
    case ShaderBindingType::StorageBuffer:
    case ShaderBindingType::UniformBufferDynamic:
    case ShaderBindingType::StorageBufferDynamic:
        return spirv_cross::SPIRType::Struct;
    case ShaderBindingType::AccelerationStructure:
        return spirv_cross::SPIRType::AccelerationStructure;
    case ShaderBindingType::Unknown:
    default:
        return spirv_cross::SPIRType::Unknown;
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
            metalBinding.count = reflectedBinding.count;
            const uint32_t descriptorCount = reflectedBinding.count > 0 ? reflectedBinding.count : 1;

            // Descriptor set is part of the sorted key, so set/binding pairs map to stable dense Metal slots
            // without flattening different descriptor sets onto the same binding number.
            if (binding_uses_buffer(reflectedBinding.type))
            {
                metalBinding.bufferIndex = nextBufferIndex;
                nextBufferIndex += descriptorCount;
            }
            if (binding_uses_texture(reflectedBinding.type))
            {
                metalBinding.textureIndex = nextTextureIndex;
                nextTextureIndex += descriptorCount;
            }
            if (binding_uses_sampler(reflectedBinding.type))
            {
                metalBinding.samplerIndex = nextSamplerIndex;
                nextSamplerIndex += descriptorCount;
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
        if (metalBinding.set >= 2)
        {
            continue;
        }

        spirv_cross::MSLResourceBinding resourceBinding;
        resourceBinding.stage = executionModel;
        resourceBinding.desc_set = metalBinding.set;
        resourceBinding.binding = metalBinding.binding;
        resourceBinding.basetype = binding_base_type(metalBinding.type);
        resourceBinding.count = metalBinding.count;
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

void update_automatic_resource_binding(spirv_cross::CompilerMSL& compiler, metal::MetalShader& shader, const spirv_cross::Resource& resource)
{
    const auto set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
    const auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
    auto itrBinding = shader.resourceBindings.find({ set, binding });
    if (itrBinding == shader.resourceBindings.end())
    {
        return;
    }

    auto& metalBinding = itrBinding->second;
    const auto primary = compiler.get_automatic_msl_resource_binding(resource.id);
    const auto secondary = compiler.get_automatic_msl_resource_binding_secondary(resource.id);
    constexpr auto invalid = std::numeric_limits<uint32_t>::max();

    if (binding_uses_buffer(metalBinding.type) && primary != invalid)
    {
        metalBinding.bufferIndex = primary;
    }
    if (binding_uses_texture(metalBinding.type) && primary != invalid)
    {
        metalBinding.textureIndex = primary;
    }
    if (metalBinding.type == ShaderBindingType::CombinedImageSampler && secondary != invalid)
    {
        metalBinding.samplerIndex = secondary;
    }
    else if (metalBinding.type == ShaderBindingType::Sampler && primary != invalid)
    {
        metalBinding.samplerIndex = primary;
    }
}

void update_automatic_resource_bindings(spirv_cross::CompilerMSL& compiler, metal::MetalShader& shader)
{
    auto resources = compiler.get_shader_resources();
    for (const auto& resource : resources.uniform_buffers)
    {
        update_automatic_resource_binding(compiler, shader, resource);
    }
    for (const auto& resource : resources.storage_buffers)
    {
        update_automatic_resource_binding(compiler, shader, resource);
    }
    for (const auto& resource : resources.sampled_images)
    {
        update_automatic_resource_binding(compiler, shader, resource);
    }
    for (const auto& resource : resources.separate_images)
    {
        update_automatic_resource_binding(compiler, shader, resource);
    }
    for (const auto& resource : resources.separate_samplers)
    {
        update_automatic_resource_binding(compiler, shader, resource);
    }
    for (const auto& resource : resources.storage_images)
    {
        update_automatic_resource_binding(compiler, shader, resource);
    }
    for (const auto& resource : resources.subpass_inputs)
    {
        update_automatic_resource_binding(compiler, shader, resource);
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
        options.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(3, 0);
        options.argument_buffers = true;
        options.argument_buffers_tier = spirv_cross::CompilerMSL::Options::ArgumentBuffersTier::Tier2;
        compiler.set_msl_options(options);

        shader.entryPointName = "main0";
        auto executionModel = execution_model_from_stage(shader.stage);
        compiler.rename_entry_point("main", shader.entryPointName, executionModel);
        compiler.set_entry_point(shader.entryPointName, executionModel);

        add_spirv_cross_resource_bindings(compiler, shader);
        compiler.add_discrete_descriptor_set(0);
        compiler.add_discrete_descriptor_set(1);

        shader.mslSource = compiler.compile();
        update_automatic_resource_bindings(compiler, shader);
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
    const auto& path = shader_path(shader);
    auto device = bridge<id<MTLDevice>>(ctx.device);
    if (!device)
    {
        report_shader_error(scene, "Could not compile Metal shader: Metal device is not available.", path);
        return false;
    }

    NSError* error = nil;
    NSString* source = [NSString stringWithUTF8String:shader.mslSource.c_str()];
    MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
    id<MTLLibrary> library = [device newLibraryWithSource:source options:options error:&error];
    if (!library)
    {
        report_shader_error(scene, fmt::format("Could not compile Metal shader '{}': {}", path.filename().string(), ns_string([error localizedDescription])), path);
        return false;
    }

    NSString* functionName = [NSString stringWithUTF8String:shader.entryPointName.c_str()];
    id<MTLFunction> function = [library newFunctionWithName:functionName];
    if (!function)
    {
        report_shader_error(scene, fmt::format("Could not create Metal shader function '{}' for shader '{}'.", shader.entryPointName, path.filename().string()), path);
        return false;
    }

    retain_obj(shader.library, library);
    retain_obj(shader.function, function);
    return true;
}

std::shared_ptr<metal::MetalShader> create_native_metal_shader(metal::MetalContext& ctx, metal::MetalScene& scene, const fs::path& path, metal::MetalShaderStage stage, const std::string& entryPointName)
{
    if (!fs::exists(path))
    {
        report_shader_error(scene, fmt::format("Native Metal shader missing: {}", path.filename().string()), path);
        return nullptr;
    }

    auto source = Zest::file_read(path);
    if (source.empty())
    {
        report_shader_error(scene, fmt::format("Could not read native Metal shader: {}", path.filename().string()), path);
        return nullptr;
    }

    auto spShader = std::make_shared<metal::MetalShader>(nullptr);
    spShader->path = path;
    spShader->stage = stage;
    spShader->entryPointName = entryPointName;
    spShader->mslSource = source;

    if (!compile_msl_library(ctx, scene, *spShader))
    {
        metal::metal_shader_destroy(ctx, *spShader);
        return nullptr;
    }

    scene.shaderStages[path] = spShader;
    return spShader;
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
    spShader->path = shader.path;
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

std::shared_ptr<MetalShader> metal_shader_create_native_ray(MetalContext& ctx, MetalScene& scene, const fs::path& path)
{
    return create_native_metal_shader(ctx, scene, path, MetalShaderStage::RayCompute, "vklive_ray_trace");
}

void metal_shader_destroy(MetalContext& ctx, MetalShader& shader)
{
    (void)ctx;
    release_obj(shader.function);
    release_obj(shader.library);
}

} // namespace metal
