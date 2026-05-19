#pragma once

#include <memory>

struct Geometry;

namespace metal
{

struct MetalContext;
struct MetalScene;

struct MetalModel
{
    explicit MetalModel(Geometry* pG)
        : pGeometry(pG)
    {
    }

    Geometry* pGeometry = nullptr;
};

std::shared_ptr<MetalModel> metal_model_create(MetalContext& ctx, MetalScene& scene, Geometry& geometry);
void metal_model_destroy(MetalContext& ctx, MetalModel& model);

} // namespace metal
