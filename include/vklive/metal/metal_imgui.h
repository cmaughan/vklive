#pragma once

#include <string>

struct ImDrawData;

namespace metal
{

struct MetalContext;

void imgui_init(MetalContext& ctx, const std::string& iniPath, bool viewports);
void imgui_shutdown(MetalContext& ctx);
void imgui_render(MetalContext& ctx, ImDrawData* drawData);

} // namespace metal
