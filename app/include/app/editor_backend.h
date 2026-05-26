#pragma once

#include <string_view>

enum class EditorBackendKind
{
    Zep,
    Neovim,
};

const char* editor_backend_to_string(EditorBackendKind backend);
bool editor_backend_from_string(std::string_view value, EditorBackendKind& backend);
