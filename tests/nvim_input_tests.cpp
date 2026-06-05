#include <vklive_nvim/input.h>

#include <SDL.h>
#include <iostream>
#include <string>

namespace
{

bool require_input(SDL_Keycode key, SDL_Keymod mods, const std::string& expected)
{
    const std::string actual = vklive_nvim::sdl_key_to_nvim(key, mods);
    if (actual == expected)
    {
        return true;
    }

    std::cerr << "Expected " << expected << ", got " << actual << "\n";
    return false;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= require_input(SDLK_RETURN, KMOD_NONE, "<CR>");
    ok &= require_input(SDLK_ESCAPE, KMOD_NONE, "<Esc>");
    ok &= require_input(SDLK_BACKSPACE, KMOD_NONE, "<BS>");
    ok &= require_input(SDLK_TAB, KMOD_SHIFT, "<S-Tab>");
    ok &= require_input(SDLK_s, KMOD_CTRL, "<C-s>");
    ok &= require_input(SDLK_h, static_cast<SDL_Keymod>(KMOD_CTRL | KMOD_SHIFT), "<C-S-H>");
    ok &= require_input(SDLK_l, static_cast<SDL_Keymod>(KMOD_CTRL | KMOD_SHIFT), "<C-S-L>");
    ok &= require_input(SDLK_LEFT, KMOD_NONE, "<Left>");
    ok &= require_input(SDLK_RIGHT, KMOD_ALT, "<A-Right>");

    return ok ? 0 : 1;
}
