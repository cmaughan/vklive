#pragma once

#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_pass.h>

struct NVGcontext;
struct Pass;

namespace metal
{

bool metal_nanovg_init(MetalContext& ctx);
void metal_nanovg_destroy(MetalContext& ctx);
bool metal_nanovg_begin(MetalContext& ctx, MetalPass& pass, const MetalPassTargets& targets);
bool metal_nanovg_end(MetalContext& ctx, MetalPass& pass);

} // namespace metal
