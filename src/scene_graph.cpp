#include <vklive/scene.h>

void scenegraph_build(SceneGraph& scene)
{
    scene.sortedPasses.clear();

    // Naive for now
    for (auto& spPass : scene.passOrder)
    {
        scene.sortedPasses.push_back(spPass);
    }
}

void scenegraph_render(SceneGraph& scene)
{

}
