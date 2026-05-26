#pragma once

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include <string>

namespace vklive_nvim
{

std::string sdl_key_to_nvim(SDL_Keycode key, SDL_Keymod mods);

} // namespace vklive_nvim
