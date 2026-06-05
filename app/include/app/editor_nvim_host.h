#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include <vklive_nvim/input.h>

struct IDevice;

std::vector<std::filesystem::path> nvim_editor_collect_edit_files(const std::filesystem::path& root);
bool nvim_editor_should_forward_keydown(SDL_Keycode key, SDL_Keymod mods, std::string_view input, bool text_input_active);
void nvim_editor_update_files(const std::filesystem::path& root, bool reset);
void nvim_editor_show(bool focus, IDevice* device);
void nvim_editor_handle_event(const SDL_Event& event);
void nvim_editor_destroy();
bool nvim_editor_focused();
