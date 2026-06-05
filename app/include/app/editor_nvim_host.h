#pragma once

#include <filesystem>
#include <vector>

union SDL_Event;

std::vector<std::filesystem::path> nvim_editor_collect_edit_files(const std::filesystem::path& root);
void nvim_editor_update_files(const std::filesystem::path& root, bool reset);
void nvim_editor_show(bool focus);
void nvim_editor_handle_event(const SDL_Event& event);
void nvim_editor_destroy();
bool nvim_editor_focused();
