#include <cstdlib>
#include <iostream>
#include <string>

#include <vklive/render_backend.h>

namespace
{
bool require(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << message << "\n";
        return false;
    }
    return true;
}
}

int main()
{
    bool ok = true;
    RenderBackend backend = RenderBackend::Auto;
    ok &= require(render_backend_from_string("auto", backend) && backend == RenderBackend::Auto, "auto parse failed");
    ok &= require(render_backend_from_string("vulkan", backend) && backend == RenderBackend::Vulkan, "vulkan parse failed");
    ok &= require(render_backend_from_string("metal", backend) && backend == RenderBackend::Metal, "metal parse failed");
    ok &= require(!render_backend_from_string("Metal", backend), "renderer parsing should be lowercase and explicit");
    ok &= require(render_backend_to_string(RenderBackend::Metal) == "metal", "metal string conversion failed");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
