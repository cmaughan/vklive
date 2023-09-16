#include <zest/imgui/imgui.h>

#include <vklive/scene.h>

void window_render(Scene& scene, bool background, const std::function<ImTextureID(const glm::vec2& size, Scene& scene)>& fnRender);
