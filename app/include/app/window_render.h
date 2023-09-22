#include <zest/imgui/imgui.h>

#include <vklive/scene.h>

struct RenderOutput;

void window_render(Scene& scene, bool background, const std::function<RenderOutput(const glm::vec2& size, Scene& scene)>& fnRender);
