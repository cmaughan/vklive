#include <vector>
#include <zest/ui/colors.h>
#include <zest/math/math_utils.h>

namespace Zest
{

namespace
{
std::vector<glm::vec4> DefaultColors;
}

void colors_calculate_defaults()
{
    double golden_ratio_conjugate = 0.618033988749895;
    double h = .85f;
    for (int i = 0; i < (int)NUM_DEFAULT_COLORS; i++)
    {
        h += golden_ratio_conjugate;
        h = std::fmod(h, 1.0);
        DefaultColors.emplace_back(HSVToRGB(float(h) * 360.0f, 0.6f, 200.0f));
    }
}

glm::vec4 colors_get_default(uint64_t id)
{
    if (DefaultColors.empty())
    {
        colors_calculate_defaults();
    }
    return DefaultColors[id % NUM_DEFAULT_COLORS];
}

} // namespace Zest
