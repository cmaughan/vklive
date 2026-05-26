#include <app/editor_backend.h>

#include <cassert>
#include <string>

int main()
{
    assert(editor_backend_to_string(EditorBackendKind::Zep) == std::string("zep"));
    assert(editor_backend_to_string(EditorBackendKind::Neovim) == std::string("neovim"));

    EditorBackendKind parsed = EditorBackendKind::Zep;
    assert(editor_backend_from_string("zep", parsed));
    assert(parsed == EditorBackendKind::Zep);
    assert(editor_backend_from_string("Zep", parsed));
    assert(parsed == EditorBackendKind::Zep);
    assert(editor_backend_from_string("nvim", parsed));
    assert(parsed == EditorBackendKind::Neovim);
    assert(editor_backend_from_string("neovim", parsed));
    assert(parsed == EditorBackendKind::Neovim);

    parsed = EditorBackendKind::Neovim;
    assert(!editor_backend_from_string("vim", parsed));
    assert(parsed == EditorBackendKind::Neovim);
}
