#include <algorithm>
#include <cstdint>
#include <exception>
#include <mutex>
#include <system_error>

#include <fmt/format.h>
#include <lodepng.h>

#include <zest/time/timer.h>

#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_geometry_compat.h>
#include <vklive/metal/metal_model.h>
#include <vklive/metal/metal_pass.h>
#include <vklive/metal/metal_scene.h>
#include <vklive/metal/metal_shader.h>
#include <vklive/scene.h>
#include <vklive/validation.h>

namespace
{

ImTextureID metal_imgui_texture_id(void* texture)
{
    return (ImTextureID)(intptr_t)texture;
}

void report_metal_scene_error(Scene& scene, const std::string& text, const fs::path& path = fs::path())
{
    scene_report_error(scene, MessageSeverity::Error, text, path.empty() ? scene.sceneGraphPath : path);
    validation_error(text);
}

bool metal_shader_extension_supported(const fs::path& path)
{
    return path.extension() == ".vert" || path.extension() == ".frag";
}

bool metal_geometry_shader_extension(const fs::path& path)
{
    return path.extension() == ".geom";
}

bool metal_native_ray_extension_supported(const fs::path& path)
{
    return path.extension() == ".metal";
}

bool metal_ray_shader_extension(const fs::path& path)
{
    return path.extension() == ".rgen" || path.extension() == ".rmiss" || path.extension() == ".rchit";
}

bool metal_validate_standard_shader(Scene& scene, const Pass& pass, const fs::path& shaderPath)
{
    if (metal_shader_extension_supported(shaderPath))
    {
        return true;
    }

    if (metal_geometry_shader_extension(shaderPath))
    {
        const auto compatibility = metal::metal_geometry_classify(shaderPath);
        if (compatibility == metal::MetalGeometryCompatibility::NormalLineVisualizer)
        {
            report_metal_scene_error(scene,
                fmt::format("Metal pass '{}' geometry shader '{}' matches Metal's normal-line geometry compatibility pattern, but the Metal fallback renderer is not implemented yet.",
                    pass.name,
                    shaderPath.filename().string()),
                shaderPath);
        }
        else
        {
            report_metal_scene_error(scene,
                fmt::format("Metal pass '{}' does not support geometry shader '{}'. Metal does not support arbitrary geometry shaders; only recognized compatibility patterns can be considered.",
                    pass.name,
                    shaderPath.filename().string()),
                shaderPath);
        }
        return false;
    }

    report_metal_scene_error(scene, fmt::format("Metal pass '{}' does not support shader stage '{}'. Only .vert and .frag shaders are accepted.", pass.name, shaderPath.filename().string()), shaderPath);
    return false;
}

bool metal_validate_pass(Scene& scene, Pass& pass)
{
    bool valid = true;

    if (pass.passType == PassType::RayTracing)
    {
        if (pass.metalRayKernel.empty())
        {
            report_metal_scene_error(scene,
                fmt::format("Metal pass '{}' is a ray tracing pass. Metal can build acceleration structures for build_as models, but Vulkan ray shader stages (.rgen/.rmiss/.rchit) cannot be translated to Metal; native Metal ray shaders and dispatch are required.",
                    pass.name));
            return false;
        }
        if (!metal_native_ray_extension_supported(pass.metalRayKernel))
        {
            report_metal_scene_error(scene, fmt::format("Metal pass '{}' native ray kernel '{}' must use a .metal source file.", pass.name, pass.metalRayKernel.filename().string()), pass.metalRayKernel);
            return false;
        }
        if (!fs::exists(pass.metalRayKernel))
        {
            report_metal_scene_error(scene, fmt::format("Metal pass '{}' native ray kernel '{}' is missing.", pass.name, pass.metalRayKernel.filename().string()), pass.metalRayKernel);
            return false;
        }
        return true;
    }

    for (auto& shaderPath : pass.shaders)
    {
        if (!metal_validate_standard_shader(scene, pass, shaderPath))
        {
            valid = false;
        }
    }

    for (auto& spShaderGroup : pass.shaderGroups)
    {
        if (!spShaderGroup)
        {
            continue;
        }

        switch (spShaderGroup->groupType)
        {
        case ShaderType::Geometry:
            report_metal_scene_error(scene, fmt::format("Metal pass '{}' does not support geometry shader groups.", pass.name));
            valid = false;
            break;
        case ShaderType::RayGroupGeneral:
        case ShaderType::RayGroupTriangles:
        case ShaderType::RayGroupProcedural:
            report_metal_scene_error(scene, fmt::format("Metal pass '{}' does not support ray shader groups.", pass.name));
            valid = false;
            break;
        case ShaderType::Vertex:
        case ShaderType::Fragment:
            break;
        }

        for (auto& [_, spShader] : spShaderGroup->shaders)
        {
            if (spShader && !metal_validate_standard_shader(scene, pass, spShader->path))
            {
                valid = false;
            }
        }
    }

    return valid;
}

bool metal_validate_scene(Scene& scene)
{
    bool valid = true;

    for (auto& [_, spShader] : scene.shaders)
    {
        if (spShader && !metal_shader_extension_supported(spShader->path) && !metal_geometry_shader_extension(spShader->path) && !metal_ray_shader_extension(spShader->path))
        {
            report_metal_scene_error(scene, fmt::format("Metal does not support shader stage '{}'. Only .vert and .frag shaders are accepted.", spShader->path.filename().string()), spShader->path);
            valid = false;
        }
    }

    for (auto& spPass : scene.passes)
    {
        if (spPass && !metal_validate_pass(scene, *spPass))
        {
            valid = false;
        }
    }

    return valid;
}

void metal_scene_destroy_resources(metal::MetalContext& ctx, metal::MetalScene& metalScene)
{
    for (auto& spPass : metalScene.passes)
    {
        if (spPass)
        {
            metal::metal_pass_destroy(ctx, *spPass);
        }
    }
    metalScene.passes.clear();

    for (auto& [_, spSurface] : metalScene.surfaces)
    {
        if (spSurface)
        {
            metal::metal_surface_destroy(ctx, *spSurface);
        }
    }
    metalScene.surfaces.clear();

    for (auto& [_, spShader] : metalScene.shaderStages)
    {
        if (spShader)
        {
            metal::metal_shader_destroy(ctx, *spShader);
        }
    }
    metalScene.shaderStages.clear();

    for (auto& [_, spModel] : metalScene.models)
    {
        if (spModel)
        {
            metal::metal_model_destroy(ctx, *spModel);
        }
    }
    metalScene.models.clear();
}

} // namespace

namespace metal
{

std::shared_ptr<MetalScene> metal_scene_get(MetalContext& ctx, Scene& scene)
{
    std::lock_guard<std::mutex> lock(ctx.metalSceneMutex);
    auto itr = ctx.mapMetalScene.find(&scene);
    if (itr == ctx.mapMetalScene.end())
    {
        return nullptr;
    }
    return itr->second;
}

std::shared_ptr<MetalScene> metal_scene_create(MetalContext& ctx, Scene& scene)
{
    metal_scene_destroy(ctx, scene);

    if (!scene.errors.empty() || !fs::exists(scene.root) || !scene.valid)
    {
        return nullptr;
    }

    if (!metal_validate_scene(scene))
    {
        return nullptr;
    }

    auto spMetalScene = std::make_shared<MetalScene>(&scene);

    for (auto& [_, pShader] : scene.shaders)
    {
        if (pShader && metal_ray_shader_extension(pShader->path))
        {
            continue;
        }

        if (pShader && !metal_shader_create(ctx, *spMetalScene, *pShader))
        {
            metal_scene_destroy_resources(ctx, *spMetalScene);
            return nullptr;
        }
    }

    for (auto& spPass : scene.passes)
    {
        if (!spPass || spPass->metalRayKernel.empty())
        {
            continue;
        }

        if (spMetalScene->shaderStages.find(spPass->metalRayKernel) != spMetalScene->shaderStages.end())
        {
            continue;
        }

        if (!metal_shader_create_native_ray(ctx, *spMetalScene, spPass->metalRayKernel))
        {
            metal_scene_destroy_resources(ctx, *spMetalScene);
            return nullptr;
        }
    }

    for (auto& [path, pGeom] : scene.models)
    {
        if (pGeom)
        {
            auto spModel = metal_model_create(ctx, *spMetalScene, *pGeom);
            if (!spModel)
            {
                metal_scene_destroy_resources(ctx, *spMetalScene);
                return nullptr;
            }
            spMetalScene->models[path] = spModel;
        }
    }

    for (auto& spPass : scene.passes)
    {
        if (spPass)
        {
            spMetalScene->passes.push_back(metal_pass_create(*spMetalScene, *spPass));
        }
    }

    {
        std::lock_guard<std::mutex> lock(ctx.metalSceneMutex);
        ctx.mapMetalScene[&scene] = spMetalScene;
    }

    return spMetalScene;
}

void metal_scene_destroy(MetalContext& ctx, Scene& scene)
{
    std::shared_ptr<MetalScene> spMetalScene;
    {
        std::lock_guard<std::mutex> lock(ctx.metalSceneMutex);
        auto itr = ctx.mapMetalScene.find(&scene);
        if (itr == ctx.mapMetalScene.end())
        {
            return;
        }

        spMetalScene = itr->second;
        ctx.mapMetalScene.erase(itr);
    }

    metal_scene_destroy_resources(ctx, *spMetalScene);
}

MetalSurface* metal_scene_get_or_create_surface(MetalContext& ctx, MetalScene& metalScene, const std::string& surfaceName, uint64_t frameCount, bool sampling)
{
    (void)ctx;
    if (!metalScene.pScene)
    {
        return nullptr;
    }

    auto pSurface = scene_get_surface(*metalScene.pScene, surfaceName.c_str());
    if (!pSurface)
    {
        scene_report_error(*metalScene.pScene, MessageSeverity::Error, fmt::format("Could not find surface: {}", surfaceName));
        return nullptr;
    }

    MetalSurfaceKey key(surfaceName, frameCount, sampling);
    auto itr = metalScene.surfaces.find(key);
    if (itr != metalScene.surfaces.end())
    {
        return itr->second.get();
    }

    auto spSurface = metal_surface_create(ctx, metalScene, *pSurface);
    if (!spSurface)
    {
        return nullptr;
    }

    spSurface->key = key;
    spSurface->debugName = key.DebugName();
    metalScene.surfaces[key] = spSurface;
    return spSurface.get();
}

void metal_scene_prepare_output_targets(MetalContext& ctx, MetalScene& metalScene)
{
    metalScene.viewableTargets.clear();

    for (auto& [_, spMetalSurface] : metalScene.surfaces)
    {
        if (!spMetalSurface || !spMetalSurface->pSurface || !spMetalSurface->pSurface->rendered)
        {
            continue;
        }

        spMetalSurface->pSurface->rendered = false;
        if (format_is_depth(spMetalSurface->pSurface->format) || spMetalSurface->format == MetalSurfaceFormat::Depth32Float || !spMetalSurface->texture)
        {
            continue;
        }

        if (!spMetalSurface->sampler)
        {
            metal_surface_create_sampler(ctx, *spMetalSurface);
        }

        if (spMetalSurface->pSurface->name == "default_color")
        {
            metalScene.defaultTarget = spMetalSurface->key;
        }
        else
        {
            metalScene.viewableTargets.insert(spMetalSurface->key);
        }
    }
}

RenderOutput metal_scene_render(MetalContext& ctx, MetalScene& metalScene, const glm::vec2& size)
{
    auto renderSize = glm::uvec2(static_cast<uint32_t>(std::max(size.x, 0.0f)), static_cast<uint32_t>(std::max(size.y, 0.0f)));
    metalScene.defaultTarget = MetalSurfaceKey();

    if (!metalScene.pScene || !metalScene.pScene->valid)
    {
        metalScene.viewableTargets.clear();
        return {};
    }

    if (!metalScene.pScene->pause)
    {
        Scene::GlobalFrameCount++;
        Scene::GlobalElapsedSeconds = metalScene.pScene->recording ? (Scene::GlobalFrameCount * (1.0 / 60.0)) : Zest::timer_get_elapsed_seconds(Zest::globalTimer);
    }
    else
    {
        Scene::GlobalElapsedSeconds = Scene::GlobalFrameCount * (1.0 / 60.0);
    }

    if (renderSize.x == 0 || renderSize.y == 0)
    {
        metalScene.viewableTargets.clear();
        return {};
    }

    try
    {
        for (auto& spPass : metalScene.passes)
        {
            if (!spPass)
            {
                continue;
            }

            if (!metal_pass_draw(ctx, *spPass, renderSize))
            {
                validation_clear_error_state();
                return {};
            }
        }

        metal_scene_prepare_output_targets(ctx, metalScene);
        return metal_scene_get_output(ctx, metalScene);
    }
    catch (std::exception& ex)
    {
        validation_error(ex.what());
        metal_scene_destroy(ctx, *metalScene.pScene);
        ctx.deviceState = DeviceState::Lost;
    }

    return {};
}

RenderOutput metal_scene_get_output(MetalContext& ctx, MetalScene& metalScene)
{
    RenderOutput out;
    if (ctx.deviceState != DeviceState::Normal || !metalScene.defaultTarget)
    {
        return out;
    }

    auto itrTarget = metalScene.surfaces.find(metalScene.defaultTarget);
    if (itrTarget == metalScene.surfaces.end() || !itrTarget->second)
    {
        return out;
    }

    auto& surface = *itrTarget->second;
    if (!surface.texture)
    {
        return out;
    }

    out.pSurface = surface.pSurface;
    out.textureId = metal_imgui_texture_id(surface.texture);
    return out;
}

void metal_scene_write_to_file(MetalContext& ctx, MetalScene& metalScene, const fs::path& path)
{
    if (!metalScene.pScene)
    {
        return;
    }

    if (!metalScene.defaultTarget)
    {
        metalScene.pScene->recording = false;
        return;
    }

    auto itrTarget = metalScene.surfaces.find(metalScene.defaultTarget);
    if (itrTarget == metalScene.surfaces.end() || !itrTarget->second)
    {
        metalScene.pScene->recording = false;
        return;
    }

    std::vector<uint8_t> pixels;
    glm::uvec2 size(0);
    if (!metal_surface_read_rgba8(ctx, *itrTarget->second, pixels, size))
    {
        report_metal_scene_error(*metalScene.pScene, "Metal render capture failed to read the default target.");
        metalScene.pScene->recording = false;
        return;
    }

    std::error_code createError;
    fs::create_directories(path, createError);
    if (createError)
    {
        report_metal_scene_error(*metalScene.pScene, fmt::format("Metal render capture failed to create '{}': {}", path.string(), createError.message()));
        metalScene.pScene->recording = false;
        return;
    }

    const auto fileName = path / fmt::format("Frame_{:05}.png", metalScene.pScene->GlobalFrameCount);
    const auto encodeError = lodepng::encode(fileName.string(), pixels.data(), size.x, size.y, LCT_RGBA);
    if (encodeError != 0)
    {
        report_metal_scene_error(*metalScene.pScene, fmt::format("Metal render capture failed to write '{}': {}", fileName.string(), lodepng_error_text(encodeError)));
        metalScene.pScene->recording = false;
    }
}

std::vector<RenderTargetView> metal_scene_target_views(MetalContext& ctx, MetalScene& scene)
{
    (void)ctx;
    std::vector<RenderTargetView> views;
    for (const auto& target : scene.viewableTargets)
    {
        auto itrTarget = scene.surfaces.find(target);
        if (itrTarget == scene.surfaces.end() || !itrTarget->second)
        {
            continue;
        }

        const auto& surface = *itrTarget->second;
        if (!surface.texture)
        {
            continue;
        }

        views.push_back(RenderTargetView{
            surface.debugName,
            metal_imgui_texture_id(surface.texture),
            surface.size });
    }
    return views;
}

} // namespace metal
