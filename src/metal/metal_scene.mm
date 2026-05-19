#include <fmt/format.h>

#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_model.h>
#include <vklive/metal/metal_pass.h>
#include <vklive/metal/metal_scene.h>
#include <vklive/scene.h>
#include <vklive/validation.h>

namespace
{

void report_metal_scene_error(Scene& scene, const std::string& text, const fs::path& path = fs::path())
{
    scene_report_error(scene, MessageSeverity::Error, text, path.empty() ? scene.sceneGraphPath : path);
    validation_error(text);
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
    ctx.mapMetalScene[&scene] = spMetalScene;

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

    return spMetalScene;
}

void metal_scene_destroy(MetalContext& ctx, Scene& scene)
{
    auto itr = ctx.mapMetalScene.find(&scene);
    if (itr == ctx.mapMetalScene.end())
    {
        return;
    }

    auto spMetalScene = itr->second;
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

    for (auto& [_, spModel] : spMetalScene->models)
    {
        if (spModel)
        {
            metal_model_destroy(ctx, *spModel);
        }
    }
    spMetalScene->models.clear();

    ctx.mapMetalScene.erase(itr);
}

RenderOutput metal_scene_render(MetalContext& ctx, MetalScene& metalScene, const glm::vec2& size)
{
    (void)ctx;
    (void)size;

    if (metalScene.pScene && !metalScene.reportedRasterRenderUnsupported)
    {
        metalScene.reportedRasterRenderUnsupported = true;
        report_metal_scene_error(*metalScene.pScene, "Metal raster pass rendering is not implemented yet.");
    }

    return {};
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
    (void)scene;
    return {};
}

} // namespace metal
