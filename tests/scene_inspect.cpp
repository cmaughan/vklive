#include <cstdlib>
#include <iostream>
#include <string>

#include <zest/logger/logger.h>
#include <zest/file/runtree.h>

#include <vklive/model.h>
#include <vklive/scene.h>

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
}

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "usage: vklive_scene_tests <project-root> <scenegraph> <valid|invalid|ray_metal>\n";
        return EXIT_FAILURE;
    }

    const fs::path root = argv[1];
    const fs::path scenegraph = argv[2];
    const std::string mode = argv[3];

    fs::path sourceRoot = root;
    while (!sourceRoot.empty() && !fs::exists(sourceRoot / "run_tree"))
    {
        sourceRoot = sourceRoot.parent_path();
    }
    if (!sourceRoot.empty())
    {
        Zest::runtree_init(sourceRoot.string().c_str(), sourceRoot.string().c_str());
    }

    auto scene = scene_build(root, scenegraph);

    bool ok = true;
    if (mode == "valid")
    {
        ok &= require(scene->valid, "override scene should be valid");
        ok &= require(scene->sceneGraphPath.filename() == scenegraph.filename(),
            "override scenegraph not selected: " + scene->sceneGraphPath.string());
        auto robot = scene->modelAssets.find("robot");
        ok &= require(robot != scene->modelAssets.end(), "robot model asset missing");
        if (robot != scene->modelAssets.end())
        {
            ok &= require(robot->second->uvOrigin == ModelUvOrigin::LowerLeft,
                "robot scene model should request lower-left UV texture origin");
        }
    }
    else if (mode == "invalid")
    {
        ok &= require(!scene->valid, "missing override scene should invalidate scene");
        ok &= require(!scene->errors.empty(), "missing override scene should report an error");
    }
    else if (mode == "ray_metal")
    {
        ok &= require(scene->valid, "ray tracer scene should be valid");
        ok &= require(scene->passes.size() >= 1, "ray tracer scene should have at least one pass");
        if (!scene->passes.empty())
        {
            auto& tracePass = *scene->passes.front();
            ok &= require(tracePass.passType == PassType::RayTracing, "trace pass should be ray tracing");
            ok &= require(tracePass.metalRayKernel.filename() == "rt_trace.metal", "trace pass should retain native Metal ray kernel path");
            ok &= require(tracePass.metalRayKernel.parent_path() == root, "trace pass Metal ray kernel should resolve relative to project root");

            bool hasRayGen = false;
            bool hasMiss = false;
            bool hasClosestHit = false;
            bool hasMetalInPassShaders = false;
            for (const auto& shaderPath : tracePass.shaders)
            {
                hasRayGen = hasRayGen || shaderPath.filename() == "rt_gen.rgen";
                hasMiss = hasMiss || shaderPath.filename() == "rt_miss.rmiss";
                hasClosestHit = hasClosestHit || shaderPath.filename() == "rt_closest.rchit";
                hasMetalInPassShaders = hasMetalInPassShaders || shaderPath.filename() == "rt_trace.metal";
            }

            ok &= require(hasRayGen, "Vulkan ray generation shader should remain in pass shaders");
            ok &= require(hasMiss, "Vulkan ray miss shader should remain in pass shaders");
            ok &= require(hasClosestHit, "Vulkan closest-hit shader should remain in pass shaders");
            ok &= require(!hasMetalInPassShaders, "native Metal ray kernel must not be inserted into pass shaders");
            ok &= require(scene->shaders.find(tracePass.metalRayKernel) == scene->shaders.end(), "native Metal ray kernel must not be inserted into scene shaders");
            ok &= require(scene_is_shader(tracePass.metalRayKernel), ".metal files should be recognized as editable shader sources");
        }
    }
    else
    {
        std::cerr << "unknown mode: " << mode << "\n";
        return EXIT_FAILURE;
    }

    Zest::runtree_destroy();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
