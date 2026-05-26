#include <vklive_nvim/input.h>

#include <SDL.h>
#include <cassert>
#include <string>

int main()
{
    assert(vklive_nvim::sdl_key_to_nvim(SDLK_RETURN, KMOD_NONE) == std::string("<CR>"));
    assert(vklive_nvim::sdl_key_to_nvim(SDLK_ESCAPE, KMOD_NONE) == std::string("<Esc>"));
    assert(vklive_nvim::sdl_key_to_nvim(SDLK_BACKSPACE, KMOD_NONE) == std::string("<BS>"));
    assert(vklive_nvim::sdl_key_to_nvim(SDLK_TAB, KMOD_SHIFT) == std::string("<S-Tab>"));
    assert(vklive_nvim::sdl_key_to_nvim(SDLK_s, KMOD_CTRL) == std::string("<C-s>"));
    assert(vklive_nvim::sdl_key_to_nvim(SDLK_LEFT, KMOD_NONE) == std::string("<Left>"));
    assert(vklive_nvim::sdl_key_to_nvim(SDLK_RIGHT, KMOD_ALT) == std::string("<A-Right>"));
}
