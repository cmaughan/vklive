#pragma once

#include <app/editor_backend.h>

#include <filesystem>
#include <map>
#include <memory>
#include <vector>

struct EditorShowContext
{
    bool focus = false;
};

class IEditorBackend
{
public:
    virtual ~IEditorBackend() = default;

    virtual EditorBackendKind kind() const = 0;
    virtual void set_project_root(const std::filesystem::path& root) = 0;
    virtual void set_files(const std::vector<std::filesystem::path>& files, bool activate_first) = 0;
    virtual void show(EditorShowContext& context) = 0;
    virtual void save_dirty_edit_files() = 0;
    virtual bool has_dirty_edit_files() const = 0;
};

class EditorHost
{
public:
    void register_backend(std::unique_ptr<IEditorBackend> backend);
    bool set_active_backend(EditorBackendKind kind);
    EditorBackendKind active_backend() const;

    void set_project_root(const std::filesystem::path& root);
    void set_files(std::vector<std::filesystem::path> files, bool activate_first);
    void show(EditorShowContext& context);
    void save_dirty_edit_files();
    bool has_dirty_edit_files() const;

private:
    IEditorBackend* active();
    const IEditorBackend* active() const;
    void sync_backend(IEditorBackend& backend);

    EditorBackendKind m_activeBackend = EditorBackendKind::Zep;
    std::filesystem::path m_projectRoot;
    std::vector<std::filesystem::path> m_files;
    bool m_activateFirst = false;
    std::map<EditorBackendKind, std::unique_ptr<IEditorBackend>> m_backends;
};
