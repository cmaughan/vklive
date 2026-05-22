#include <cstdlib>
#include <iostream>
#include <string>

#include <app/editor_font.h>

namespace
{

bool require_equal(int actual, int expected, const std::string& message)
{
    if (actual != expected)
    {
        std::cerr << message << ": expected=" << expected << " actual=" << actual << "\n";
        return false;
    }
    return true;
}

} // namespace

int main()
{
    bool ok = true;

    ok &= require_equal(zep_effective_font_pixel_height(16.0f, 1.0f), 16, "unscaled font height should stay logical");
    ok &= require_equal(zep_effective_font_pixel_height(32.0f, 0.5f), 16, "retina font atlas scale should not double zep text");
    ok &= require_equal(zep_effective_font_pixel_height(16.5f, 1.0f), 17, "fractional font height should round to nearest pixel");
    ok &= require_equal(zep_effective_font_pixel_height(0.0f, 1.0f), 1, "font height should not collapse to zero");
    ok &= require_equal(zep_effective_font_pixel_height(16.0f, 0.0f), 16, "invalid global scale should fall back to one");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
