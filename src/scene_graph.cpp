#include <vklive/scene.h>
#include <vklive/IDevice.h>

extern IDevice* GetDevice();

void scenegraph_build(SceneGraph& scene)
{
    scene.sortedPasses.clear();

    // Naive for now
    for (auto& spPass : scene.passOrder)
    {
        scene.sortedPasses.push_back(spPass);
    }
}

void scenegraph_render(SceneGraph& scene, const glm::vec2& size)
{
    auto device = GetDevice();
}
