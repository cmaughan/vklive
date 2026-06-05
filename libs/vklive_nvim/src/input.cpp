#include <vklive_nvim/input.h>

#include <string_view>

namespace vklive_nvim
{
namespace
{

bool has_mod(SDL_Keymod mods, SDL_Keymod expected)
{
    return (static_cast<int>(mods) & static_cast<int>(expected)) != 0;
}

std::string angle_key(std::string_view name, SDL_Keymod mods)
{
    const bool ctrl = has_mod(mods, KMOD_CTRL);
    const bool alt = has_mod(mods, KMOD_ALT);
    const bool shift = has_mod(mods, KMOD_SHIFT);

    if (!ctrl && !alt && !shift)
    {
        std::string value = "<";
        value.append(name);
        value.push_back('>');
        return value;
    }

    std::string value = "<";
    if (ctrl)
    {
        value.append("C-");
    }
    if (alt)
    {
        value.append("A-");
    }
    if (shift)
    {
        value.append("S-");
    }
    value.append(name);
    value.push_back('>');
    return value;
}

std::string special_key(SDL_Keycode key, SDL_Keymod mods)
{
    switch (key)
    {
    case SDLK_RETURN:
        return angle_key("CR", static_cast<SDL_Keymod>(static_cast<int>(mods) & ~static_cast<int>(KMOD_SHIFT)));
    case SDLK_ESCAPE:
        return angle_key("Esc", static_cast<SDL_Keymod>(static_cast<int>(mods) & ~static_cast<int>(KMOD_SHIFT)));
    case SDLK_BACKSPACE:
        return angle_key("BS", static_cast<SDL_Keymod>(static_cast<int>(mods) & ~static_cast<int>(KMOD_SHIFT)));
    case SDLK_TAB:
        return angle_key("Tab", mods);
    case SDLK_DELETE:
        return angle_key("Del", mods);
    case SDLK_INSERT:
        return angle_key("Insert", mods);
    case SDLK_HOME:
        return angle_key("Home", mods);
    case SDLK_END:
        return angle_key("End", mods);
    case SDLK_PAGEUP:
        return angle_key("PageUp", mods);
    case SDLK_PAGEDOWN:
        return angle_key("PageDown", mods);
    case SDLK_UP:
        return angle_key("Up", mods);
    case SDLK_DOWN:
        return angle_key("Down", mods);
    case SDLK_LEFT:
        return angle_key("Left", mods);
    case SDLK_RIGHT:
        return angle_key("Right", mods);
    default:
        return {};
    }
}

} // namespace

std::string sdl_key_to_nvim(SDL_Keycode key, SDL_Keymod mods)
{
    if (auto special = special_key(key, mods); !special.empty())
    {
        return special;
    }

    const bool ctrl = has_mod(mods, KMOD_CTRL);
    const bool alt = has_mod(mods, KMOD_ALT);
    const bool shift = has_mod(mods, KMOD_SHIFT);

    if (key >= SDLK_a && key <= SDLK_z && (ctrl || alt))
    {
        char keyName = static_cast<char>(key);
        if (shift)
        {
            keyName = static_cast<char>('A' + (key - SDLK_a));
        }
        return angle_key(std::string_view(&keyName, 1), mods);
    }

    if (!ctrl && !alt && !shift && key >= 32 && key <= 126)
    {
        return std::string(1, static_cast<char>(key));
    }

    return {};
}

} // namespace vklive_nvim
