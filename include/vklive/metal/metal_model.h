#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <vklive/model.h>

struct Geometry;

namespace metal
{

struct MetalContext;
struct MetalScene;

struct MetalModel : Model
{
    Geometry* pGeometry = nullptr;
    void* vertexBuffer = nullptr;
    void* indexBuffer = nullptr;
    std::string debugName;
    uint32_t vertexStride = 0;
    bool staged = false;
};

std::shared_ptr<MetalModel> metal_model_create(MetalContext& ctx, MetalScene& scene, Geometry& geometry);
void metal_model_destroy(MetalContext& ctx, MetalModel& model);

} // namespace metal
