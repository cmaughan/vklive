#include <app/editor_backend.h>
#include <app/editor_host.h>

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

class CountingEditorBackend : public IEditorBackend
{
public:
    explicit CountingEditorBackend(EditorBackendKind kind)
        : m_kind(kind)
    {
    }

    EditorBackendKind kind() const override
    {
        return m_kind;
    }

    void set_project_root(const std::filesystem::path& root) override
    {
        projectRoot = root;
        ++setProjectCount;
    }

    void set_files(const std::vector<std::filesystem::path>& files, bool activate_first) override
    {
        fileCount = files.size();
        activated = activate_first;
    }

    void show(EditorShowContext&) override
    {
        ++showCount;
    }

    void save_dirty_edit_files() override
    {
        ++saveCount;
    }

    bool has_dirty_edit_files() const override
    {
        return dirty;
    }

    std::filesystem::path projectRoot;
    int setProjectCount = 0;
    int showCount = 0;
    int saveCount = 0;
    std::size_t fileCount = 0;
    bool activated = false;
    bool dirty = false;

private:
    EditorBackendKind m_kind;
};

int main()
{
    EditorHost host;
    auto zep = std::make_unique<CountingEditorBackend>(EditorBackendKind::Zep);
    auto nvim = std::make_unique<CountingEditorBackend>(EditorBackendKind::Neovim);

    auto* zepPtr = zep.get();
    auto* nvimPtr = nvim.get();

    host.register_backend(std::move(zep));
    host.register_backend(std::move(nvim));

    assert(host.active_backend() == EditorBackendKind::Zep);
    host.set_project_root("D:/project");
    host.set_files({ "a.frag", "b.vert" }, true);

    assert(host.set_active_backend(EditorBackendKind::Neovim));
    assert(host.active_backend() == EditorBackendKind::Neovim);
    assert(nvimPtr->projectRoot == std::filesystem::path("D:/project"));
    assert(nvimPtr->setProjectCount >= 1);
    assert(nvimPtr->fileCount == 2);
    assert(nvimPtr->activated);

    EditorShowContext context;
    host.show(context);
    assert(nvimPtr->showCount == 1);

    nvimPtr->dirty = true;
    assert(host.has_dirty_edit_files());
    host.save_dirty_edit_files();
    assert(nvimPtr->saveCount == 1);

    assert(host.set_active_backend(EditorBackendKind::Zep));
    assert(host.active_backend() == EditorBackendKind::Zep);
    assert(zepPtr->projectRoot == std::filesystem::path("D:/project"));
    assert(zepPtr->fileCount == 2);

    assert(!host.set_active_backend(static_cast<EditorBackendKind>(999)));
    assert(host.active_backend() == EditorBackendKind::Zep);
}
