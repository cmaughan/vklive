#include <algorithm>
#include <cstdint>
#include <mutex>

#include <fmt/format.h>

#include <vklive/metal/metal_context.h>
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

void report_metal_scene_warning(Scene& scene, const std::string& text, const fs::path& path = fs::path())
{
    Message msg;
    msg.severity = MessageSeverity::Warning;
    msg.text = text;
    msg.path = path.empty() ? scene.sceneGraphPath : path;
    scene.warnings.push_back(msg);
}

bool metal_shader_extension_supported(const fs::path& path)
{
    return path.extension() == ".vert" || path.extension() == ".frag";
}

bool metal_validate_pass(Scene& scene, Pass& pass)
{
    bool valid = true;

    if (pass.passType == PassType::RayTracing)
    {
        report_metal_scene_error(scene, fmt::format("Metal does not support ray tracing pass '{}' yet.", pass.name));
        valid = false;
    }
    else if (pass.passType == PassType::Scripted)
    {
        report_metal_scene_error(scene, fmt::format("Metal does not support scripted pass '{}' yet.", pass.name));
        valid = false;
    }

    for (auto& shaderPath : pass.shaders)
    {
        if (!metal_shader_extension_supported(shaderPath))
        {
            report_metal_scene_error(scene, fmt::format("Metal pass '{}' does not support shader stage '{}'. Only .vert and .frag shaders are accepted.", pass.name, shaderPath.filename().string()), shaderPath);
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
            if (spShader && !metal_shader_extension_supported(spShader->path))
            {
                report_metal_scene_error(scene, fmt::format("Metal pass '{}' does not support shader stage '{}'. Only .vert and .frag shaders are accepted.", pass.name, spShader->path.filename().string()), spShader->path);
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
        if (spShader && !metal_shader_extension_supported(spShader->path))
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
        if (pShader && !metal_shader_create(ctx, *spMetalScene, *pShader))
        {
            for (auto& [_, spShader] : spMetalScene->shaderStages)
            {
                if (spShader)
                {
                    metal_shader_destroy(ctx, *spShader);
                }
            }
            spMetalScene->shaderStages.clear();
            return nullptr;
        }
    }

    for (auto& [path, pGeom] : scene.models)
    {
        if (pGeom)
        {
            spMetalScene->models[path] = metal_model_create(ctx, *spMetalScene, *pGeom);
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

    for (auto& spPass : spMetalScene->passes)
    {
        if (spPass)
        {
            metal_pass_destroy(ctx, *spPass);
        }
    }
    spMetalScene->passes.clear();

    for (auto& [_, spSurface] : spMetalScene->surfaces)
    {
        if (spSurface)
        {
            metal_surface_destroy(ctx, *spSurface);
        }
    }
    spMetalScene->surfaces.clear();

    for (auto& [_, spShader] : spMetalScene->shaderStages)
    {
        if (spShader)
        {
            metal_shader_destroy(ctx, *spShader);
        }
    }
    spMetalScene->shaderStages.clear();

    for (auto& [_, spModel] : spMetalScene->models)
    {
        if (spModel)
        {
            metal_model_destroy(ctx, *spModel);
        }
    }
    spMetalScene->models.clear();

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

    if (renderSize.x == 0 || renderSize.y == 0)
    {
        metalScene.viewableTargets.clear();
        return {};
    }

    if (metalScene.pScene && !metalScene.reportedRasterRenderUnsupported)
    {
        metalScene.reportedRasterRenderUnsupported = true;
        report_metal_scene_warning(*metalScene.pScene, "Metal raster pass rendering is not implemented yet.");
    }

    if (metalScene.pScene)
    {
        for (auto& [surfaceName, spSurface] : metalScene.pScene->surfaces)
        {
            if (!spSurface || !spSurface->isTarget)
            {
                continue;
            }

            auto pMetalSurface = metal_scene_get_or_create_surface(ctx, metalScene, surfaceName);
            if (pMetalSurface)
            {
                metal_surface_ensure_target(ctx, metalScene, *pMetalSurface, renderSize);
            }
        }
    }

    metal_scene_prepare_output_targets(ctx, metalScene);
    return metal_scene_get_output(ctx, metalScene);
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
    (void)ctx;
    (void)path;

    if (metalScene.pScene && !metalScene.reportedCaptureUnsupported)
    {
        metalScene.reportedCaptureUnsupported = true;
        report_metal_scene_error(*metalScene.pScene, "Metal render capture is not implemented yet.");
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
