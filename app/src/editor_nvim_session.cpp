#include <app/editor_nvim_session.h>

#include <algorithm>
#include <utility>

namespace
{

class RealNvimHostAdapter final : public INvimHostAdapter
{
public:
    bool start(const vklive_nvim::NvimHostOptions& options) override
    {
        return m_host.start(options);
    }

    void stop() override
    {
        m_host.stop();
    }

    bool running() const override
    {
        return m_host.running();
    }

    void resize(int columns, int rows) override
    {
        m_host.resize(columns, rows);
    }

    void open_project_files(const vklive_nvim::NvimProjectFiles& project) override
    {
        m_host.open_project_files(project);
    }

    void pump() override
    {
        m_host.pump();
    }

    void send_input(std::string_view input) override
    {
        m_host.send_input(input);
    }

    const vklive_nvim::RenderModel& render_model() const override
    {
        return m_host.render_model();
    }

    const vklive_nvim::HighlightTable& highlights() const override
    {
        return m_host.highlights();
    }

private:
    vklive_nvim::NvimHost m_host;
};

int positive_or_one(int value)
{
    return std::max(1, value);
}

} // namespace

NvimEditorSession::NvimEditorSession()
    : NvimEditorSession(std::make_unique<RealNvimHostAdapter>())
{
}

NvimEditorSession::NvimEditorSession(std::unique_ptr<INvimHostAdapter> host)
    : m_host(std::move(host))
{
}

NvimEditorSession::~NvimEditorSession()
{
    stop();
}

void NvimEditorSession::set_project_root(const std::filesystem::path& root)
{
    if (m_projectRoot == root)
    {
        return;
    }

    const bool wasRunning = running();
    if (wasRunning)
    {
        stop();
    }

    m_projectRoot = root;
    m_pendingProjectSync = true;
}

void NvimEditorSession::set_files(std::vector<std::filesystem::path> files, bool activate_first)
{
    const bool changed = m_files != files || m_activateFirst != activate_first;
    m_files = std::move(files);
    m_activateFirst = activate_first;

    if (!changed)
    {
        return;
    }

    m_pendingProjectSync = true;
    sync_project_files();
}

bool NvimEditorSession::ensure_started(int columns, int rows)
{
    columns = positive_or_one(columns);
    rows = positive_or_one(rows);

    if (!running())
    {
        vklive_nvim::NvimHostOptions options;
        options.project_root = m_projectRoot;
        options.columns = columns;
        options.rows = rows;

        if (!m_host || !m_host->start(options))
        {
            return false;
        }

        m_columns = columns;
        m_rows = rows;
        m_pendingProjectSync = true;
    }
    else if (m_columns != columns || m_rows != rows)
    {
        m_host->resize(columns, rows);
        m_columns = columns;
        m_rows = rows;
    }

    sync_project_files();
    return true;
}

bool NvimEditorSession::running() const
{
    return m_host && m_host->running();
}

void NvimEditorSession::pump()
{
    if (running())
    {
        m_host->pump();
    }
}

void NvimEditorSession::stop()
{
    if (!m_host || !m_host->running())
    {
        return;
    }

    m_host->stop();
    m_columns = 0;
    m_rows = 0;
    m_pendingProjectSync = true;
}

void NvimEditorSession::send_input(std::string_view input)
{
    if (input.empty() || !running())
    {
        return;
    }

    m_host->send_input(input);
}

const vklive_nvim::RenderModel& NvimEditorSession::render_model() const
{
    return m_host->render_model();
}

const vklive_nvim::HighlightTable& NvimEditorSession::highlights() const
{
    return m_host->highlights();
}

void NvimEditorSession::sync_project_files()
{
    if (!m_pendingProjectSync || !running())
    {
        return;
    }

    vklive_nvim::NvimProjectFiles project;
    project.project_root = m_projectRoot;
    project.files = m_files;
    m_host->open_project_files(project);
    m_pendingProjectSync = false;
}
