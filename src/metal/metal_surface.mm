#include <vklive/metal/metal_surface.h>
#include <vklive/scene.h>

namespace metal
{

std::shared_ptr<MetalSurface> metal_surface_create(MetalContext& ctx, MetalScene& scene, Surface& surface)
{
    (void)ctx;
    (void)scene;
    auto spSurface = std::make_shared<MetalSurface>(&surface);
    spSurface->key.targetName = surface.name;
    spSurface->debugName = surface.name;
    return spSurface;
}

void metal_surface_destroy(MetalContext& ctx, MetalSurface& surface)
{
    (void)ctx;
    (void)surface;
}

} // namespace metal
