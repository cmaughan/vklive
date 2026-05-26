#include <app/editor_host.h>

#include <utility>

void EditorHost::register_backend(std::unique_ptr<IEditorBackend> backend)
{
    if (!backend)
    {
        return;
    }

    const auto kind = backend->kind();
    sync_backend(*backend);
    m_backends[kind] = std::move(backend);
}

bool EditorHost::set_active_backend(EditorBackendKind kind)
{
    auto itr = m_backends.find(kind);
    if (itr == m_backends.end())
    {
        return false;
    }

    m_activeBackend = kind;
    sync_backend(*itr->second);
    return true;
}

EditorBackendKind EditorHost::active_backend() const
{
    return m_activeBackend;
}

void EditorHost::set_project_root(const std::filesystem::path& root)
{
    m_projectRoot = root;
    for (auto& [_, backend] : m_backends)
    {
        backend->set_project_root(m_projectRoot);
    }
}

void EditorHost::set_files(std::vector<std::filesystem::path> files, bool activate_first)
{
    m_files = std::move(files);
    m_activateFirst = activate_first;
    for (auto& [_, backend] : m_backends)
    {
        backend->set_files(m_files, m_activateFirst);
    }
}

void EditorHost::show(EditorShowContext& context)
{
    if (auto* backend = active())
    {
        backend->show(context);
    }
}

void EditorHost::save_dirty_edit_files()
{
    if (auto* backend = active())
    {
        backend->save_dirty_edit_files();
    }
}

bool EditorHost::has_dirty_edit_files() const
{
    if (const auto* backend = active())
    {
        return backend->has_dirty_edit_files();
    }

    return false;
}

IEditorBackend* EditorHost::active()
{
    auto itr = m_backends.find(m_activeBackend);
    return itr == m_backends.end() ? nullptr : itr->second.get();
}

const IEditorBackend* EditorHost::active() const
{
    auto itr = m_backends.find(m_activeBackend);
    return itr == m_backends.end() ? nullptr : itr->second.get();
}

void EditorHost::sync_backend(IEditorBackend& backend)
{
    backend.set_project_root(m_projectRoot);
    backend.set_files(m_files, m_activateFirst);
}
