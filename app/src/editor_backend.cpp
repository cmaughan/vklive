#include <app/editor_backend.h>

#include <algorithm>
#include <cctype>
#include <string>

const char* editor_backend_to_string(EditorBackendKind backend)
{
    switch (backend)
    {
    case EditorBackendKind::Zep:
        return "zep";
    case EditorBackendKind::Neovim:
        return "neovim";
    }

    return "zep";
}

bool editor_backend_from_string(std::string_view value, EditorBackendKind& backend)
{
    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (normalized == "zep")
    {
        backend = EditorBackendKind::Zep;
        return true;
    }

    if (normalized == "nvim" || normalized == "neovim")
    {
        backend = EditorBackendKind::Neovim;
        return true;
    }

    return false;
}
