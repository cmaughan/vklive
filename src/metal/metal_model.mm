#include <vklive/metal/metal_model.h>

namespace metal
{

std::shared_ptr<MetalModel> metal_model_create(MetalContext& ctx, MetalScene& scene, Geometry& geometry)
{
    (void)ctx;
    (void)scene;
    return std::make_shared<MetalModel>(&geometry);
}

void metal_model_destroy(MetalContext& ctx, MetalModel& model)
{
    (void)ctx;
    (void)model;
}

} // namespace metal
