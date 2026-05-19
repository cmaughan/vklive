#include <cstdlib>
#include <iostream>
#include <string>

#include <zest/logger/logger.h>

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
        std::cerr << "usage: vklive_scene_tests <project-root> <scenegraph> <valid|invalid>\n";
        return EXIT_FAILURE;
    }

    const fs::path root = argv[1];
    const fs::path scenegraph = argv[2];
    const std::string mode = argv[3];

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
    else
    {
        std::cerr << "unknown mode: " << mode << "\n";
        return EXIT_FAILURE;
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
