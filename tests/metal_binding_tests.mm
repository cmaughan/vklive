#import <Metal/Metal.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include <zest/file/runtree.h>
#include <zest/logger/logger.h>

#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_pass.h>
#include <vklive/metal/metal_scene.h>
#include <vklive/metal/metal_shader.h>
#include <vklive/scene.h>
#include <vklive/validation.h>

namespace Zest
{
Logger logger{ false, LT::DBG };
bool Log::disabled = false;
}

namespace
{
bool require(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << message << "\n";
        return false;
    }
    return true;
}

bool write_shader(const fs::path& shaderPath, const std::string& source)
{
    std::ofstream shaderFile(shaderPath);
    if (!shaderFile)
    {
        std::cerr << "could not write shader: " << shaderPath << "\n";
        return false;
    }
    shaderFile << source;
    return true;
}

bool require_binding(const metal::MetalShader& shader, const std::pair<uint32_t, uint32_t>& key, const char* name)
{
    return require(shader.resourceBindings.find(key) != shader.resourceBindings.end(), std::string("missing binding for ") + name);
}

std::shared_ptr<metal::MetalShader> create_shader(metal::MetalContext& ctx, metal::MetalScene& metalScene, Shader& shader)
{
    return metal::metal_shader_create(ctx, metalScene, shader);
}

bool message_contains(const Message& message, const std::string& text)
{
    return message.text.find(text) != std::string::npos;
}
}

int main(int argc, char** argv)
{
    @autoreleasepool
    {
        if (argc != 2)
        {
            std::cerr << "usage: vklive_metal_binding_tests <source-root>\n";
            return EXIT_FAILURE;
        }

        const fs::path sourceRoot = argv[1];
        Zest::runtree_init(sourceRoot.string().c_str(), sourceRoot.string().c_str());

        auto device = MTLCreateSystemDefaultDevice();
        if (!device)
        {
            std::cerr << "Metal device unavailable\n";
            return EXIT_FAILURE;
        }

        const fs::path shaderDir = fs::temp_directory_path() / "vklive_metal_binding_tests";
        fs::create_directories(shaderDir);
        bool ok = true;

        const fs::path shaderPath = shaderDir / "bindings.frag";
        ok &= write_shader(shaderPath, R"(#version 450
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform UBO { float iTime; } ubo;
layout(set = 1, binding = 0) uniform sampler2D texA;
layout(set = 1, binding = 1) uniform sampler2D texB;

void main()
{
    outColor = texture(texA, vec2(0.5)) + texture(texB, vec2(0.25)) + vec4(ubo.iTime);
}
)");

        const fs::path arrayShaderPath = shaderDir / "binding_arrays.frag";
        ok &= write_shader(arrayShaderPath, R"(#version 450
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D texArray[2];
layout(set = 1, binding = 1) uniform sampler2D texAfter;

void main()
{
    outColor = texture(texArray[0], vec2(0.5)) + texture(texArray[1], vec2(0.25)) + texture(texAfter, vec2(0.75));
}
)");

        metal::MetalContext ctx;
        ctx.device = (__bridge void*)device;

        Scene scene(shaderDir);
        metal::MetalScene metalScene(&scene);

        Shader fragmentShader(shaderPath);
        auto shader = create_shader(ctx, metalScene, fragmentShader);
        ok &= require(shader != nullptr, "fragment shader should compile and reflect");
        if (shader)
        {
            ok &= require_binding(*shader, { 0, 0 }, "UBO");
            ok &= require_binding(*shader, { 1, 0 }, "texA");
            ok &= require_binding(*shader, { 1, 1 }, "texB");

            if (ok)
            {
                ok &= require(shader->resourceBindings[{ 0, 0 }].bufferIndex == 1, "UBO buffer index should follow vertex buffer slot");
                ok &= require(shader->resourceBindings[{ 0, 0 }].count == 1, "UBO descriptor count mismatch");
                ok &= require(shader->resourceBindings[{ 1, 0 }].textureIndex == 0, "texA texture index mismatch");
                ok &= require(shader->resourceBindings[{ 1, 0 }].samplerIndex == 0, "texA sampler index mismatch");
                ok &= require(shader->resourceBindings[{ 1, 1 }].textureIndex == 1, "texB texture index mismatch");
                ok &= require(shader->resourceBindings[{ 1, 1 }].samplerIndex == 1, "texB sampler index mismatch");
            }

            metal::metal_shader_destroy(ctx, *shader);
        }

        Shader arrayFragmentShader(arrayShaderPath);
        auto arrayShader = create_shader(ctx, metalScene, arrayFragmentShader);
        ok &= require(arrayShader != nullptr, "array fragment shader should compile and reflect");
        if (arrayShader)
        {
            ok &= require_binding(*arrayShader, { 1, 0 }, "texArray");
            ok &= require_binding(*arrayShader, { 1, 1 }, "texAfter");

            if (ok)
            {
                ok &= require(arrayShader->resourceBindings[{ 1, 0 }].count == 2, "texArray descriptor count mismatch");
                ok &= require(arrayShader->resourceBindings[{ 1, 0 }].textureIndex == 0, "texArray texture index mismatch");
                ok &= require(arrayShader->resourceBindings[{ 1, 0 }].samplerIndex == 0, "texArray sampler index mismatch");
                ok &= require(arrayShader->resourceBindings[{ 1, 1 }].textureIndex == 2, "texAfter texture index should skip array slots");
                ok &= require(arrayShader->resourceBindings[{ 1, 1 }].samplerIndex == 2, "texAfter sampler index should skip array slots");
            }

            metal::metal_shader_destroy(ctx, *arrayShader);
        }

        validation_clear_error_state();
        Scene validationScene(shaderDir);
        validationScene.sceneGraphPath = shaderDir / "validation.scenegraph";
        metal::MetalScene validationMetalScene(&validationScene);

        Shader validationVertexShader(shaderDir / "validation.vert");
        Shader validationFragmentShader(shaderDir / "validation.frag");
        auto vertexMetalShader = std::make_shared<metal::MetalShader>(&validationVertexShader);
        vertexMetalShader->stage = metal::MetalShaderStage::Vertex;
        vertexMetalShader->function = reinterpret_cast<void*>(1);

        auto fragmentMetalShader = std::make_shared<metal::MetalShader>(&validationFragmentShader);
        fragmentMetalShader->stage = metal::MetalShaderStage::Fragment;
        fragmentMetalShader->function = reinterpret_cast<void*>(1);
        fragmentMetalShader->bindingSets[1].bindings[0] = ShaderBinding{ 0, ShaderBindingType::CombinedImageSampler, 1, static_cast<ShaderStageFlags>(ShaderStageBits::Fragment) };
        fragmentMetalShader->bindingSets[1].bindingMeta[0] = ShaderBindingMeta{ "texUnsupported", validationFragmentShader.path, 12 };

        metal::MetalShaderResourceBinding unsupportedBinding;
        unsupportedBinding.set = 1;
        unsupportedBinding.binding = 0;
        unsupportedBinding.type = ShaderBindingType::CombinedImageSampler;
        unsupportedBinding.count = 1;
        unsupportedBinding.textureIndex = 0;
        unsupportedBinding.samplerIndex = 0;
        fragmentMetalShader->resourceBindings[{ 1, 0 }] = unsupportedBinding;

        validationMetalScene.shaderStages[validationVertexShader.path] = vertexMetalShader;
        validationMetalScene.shaderStages[validationFragmentShader.path] = fragmentMetalShader;

        Pass validationPass(validationScene, "validation_pass");
        validationPass.passType = PassType::Standard;
        validationPass.models.push_back("dummy.obj");
        validationPass.shaders.push_back(validationVertexShader.path);
        validationPass.shaders.push_back(validationFragmentShader.path);

        auto metalPass = metal::metal_pass_create(validationMetalScene, validationPass);
        ok &= require(!metal::metal_pass_draw(ctx, *metalPass, glm::uvec2(64, 64)), "unsupported reflected binding should reject draw");
        ok &= require(!validationScene.errors.empty(), "unsupported reflected binding should report a scene error");
        if (!validationScene.errors.empty())
        {
            const auto& error = validationScene.errors.front();
            ok &= require(message_contains(error, "texUnsupported"), "unsupported binding error should include resource name");
            ok &= require(message_contains(error, "set 1, binding 0"), "unsupported binding error should include shader location");
            ok &= require(message_contains(error, "CombinedImageSampler"), "unsupported binding error should include binding type");
            ok &= require(error.path == validationFragmentShader.path, "unsupported binding error should use shader path");
            ok &= require(error.line == 12, "unsupported binding error should use reflected line");
        }

        validation_clear_error_state();
        Scene uboArrayScene(shaderDir);
        uboArrayScene.sceneGraphPath = shaderDir / "ubo_array.scenegraph";
        metal::MetalScene uboArrayMetalScene(&uboArrayScene);

        Shader uboArrayVertexShader(shaderDir / "ubo_array.vert");
        Shader uboArrayFragmentShader(shaderDir / "ubo_array.frag");
        auto uboArrayVertexMetalShader = std::make_shared<metal::MetalShader>(&uboArrayVertexShader);
        uboArrayVertexMetalShader->stage = metal::MetalShaderStage::Vertex;
        uboArrayVertexMetalShader->function = reinterpret_cast<void*>(1);

        auto uboArrayFragmentMetalShader = std::make_shared<metal::MetalShader>(&uboArrayFragmentShader);
        uboArrayFragmentMetalShader->stage = metal::MetalShaderStage::Fragment;
        uboArrayFragmentMetalShader->function = reinterpret_cast<void*>(1);
        uboArrayFragmentMetalShader->bindingSets[0].bindings[0] = ShaderBinding{ 0, ShaderBindingType::UniformBuffer, 2, static_cast<ShaderStageFlags>(ShaderStageBits::Fragment) };
        uboArrayFragmentMetalShader->bindingSets[0].bindingMeta[0] = ShaderBindingMeta{ "uboArray", uboArrayFragmentShader.path, 8 };

        metal::MetalShaderResourceBinding uboArrayBinding;
        uboArrayBinding.set = 0;
        uboArrayBinding.binding = 0;
        uboArrayBinding.type = ShaderBindingType::UniformBuffer;
        uboArrayBinding.count = 2;
        uboArrayBinding.bufferIndex = 1;
        uboArrayFragmentMetalShader->resourceBindings[{ 0, 0 }] = uboArrayBinding;

        uboArrayMetalScene.shaderStages[uboArrayVertexShader.path] = uboArrayVertexMetalShader;
        uboArrayMetalScene.shaderStages[uboArrayFragmentShader.path] = uboArrayFragmentMetalShader;

        Pass uboArrayPass(uboArrayScene, "ubo_array_pass");
        uboArrayPass.passType = PassType::Standard;
        uboArrayPass.models.push_back("dummy.obj");
        uboArrayPass.shaders.push_back(uboArrayVertexShader.path);
        uboArrayPass.shaders.push_back(uboArrayFragmentShader.path);

        auto uboArrayMetalPass = metal::metal_pass_create(uboArrayMetalScene, uboArrayPass);
        ok &= require(!metal::metal_pass_draw(ctx, *uboArrayMetalPass, glm::uvec2(64, 64)), "default UBO array should reject draw");
        ok &= require(!uboArrayScene.errors.empty(), "default UBO array should report a scene error");
        if (!uboArrayScene.errors.empty())
        {
            const auto& error = uboArrayScene.errors.front();
            ok &= require(message_contains(error, "uboArray"), "default UBO array error should include resource name");
            ok &= require(message_contains(error, "set 0, binding 0"), "default UBO array error should include shader location");
            ok &= require(message_contains(error, "UniformBuffer"), "default UBO array error should include binding type");
            ok &= require(message_contains(error, "count 2"), "default UBO array error should include descriptor count");
            ok &= require(error.path == uboArrayFragmentShader.path, "default UBO array error should use shader path");
            ok &= require(error.line == 8, "default UBO array error should use reflected line");
        }

        Zest::runtree_destroy();
        return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }
}
