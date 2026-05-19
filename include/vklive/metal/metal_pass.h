#pragma once

#include <memory>

struct Pass;

namespace metal
{

struct MetalContext;
struct MetalScene;

struct MetalPass
{
    MetalPass(MetalScene& s, Pass& p)
        : metalScene(s)
        , pass(p)
    {
    }

    MetalScene& metalScene;
    Pass& pass;
};

std::shared_ptr<MetalPass> metal_pass_create(MetalScene& scene, Pass& pass);
void metal_pass_destroy(MetalContext& ctx, MetalPass& pass);

} // namespace metal
