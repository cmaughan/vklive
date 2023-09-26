#include <zest/imgui/imgui.h>

#include <vklive/scene.h>

struct RenderOutput;
struct IDevice;

void window_render(IDevice* pDevice, Scene& scene, bool background, const std::function<RenderOutput(const glm::vec2& size, Scene& scene)>& fnRender);
