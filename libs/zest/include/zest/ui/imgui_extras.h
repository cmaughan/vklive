#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <zest/math/math_utils.h>

namespace ImGui
{
bool Combo(const char* label, int* currIndex, std::vector<std::string>& values);
bool ListBox(const char* label, int* currIndex, std::vector<std::string>& values);
bool DragIntRange4(const char* label, glm::i32vec4& v, float v_speed, int v_min, int v_max, const char* format = "%d", const char* format_max = nullptr);
} // namespace ImGui
