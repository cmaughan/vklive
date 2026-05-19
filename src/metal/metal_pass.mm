#include <vklive/metal/metal_pass.h>

namespace metal
{

std::shared_ptr<MetalPass> metal_pass_create(MetalScene& scene, Pass& pass)
{
    return std::make_shared<MetalPass>(scene, pass);
}

void metal_pass_destroy(MetalContext& ctx, MetalPass& pass)
{
    (void)ctx;
    (void)pass;
}

} // namespace metal
