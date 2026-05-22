#pragma once

namespace metal
{

struct MetalContext;
struct MetalModel;
struct MetalScene;

bool metal_model_build_acceleration_structures(MetalContext& ctx, MetalScene& scene, MetalModel& model);

} // namespace metal
