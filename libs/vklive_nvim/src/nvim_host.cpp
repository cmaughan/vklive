#include <vklive_nvim/nvim_host.h>

#include <vklive_nvim/nvim_process.h>
#include <vklive_nvim/nvim_ui.h>
#include <vklive_nvim/render_model.h>

#include <utility>

namespace vklive_nvim
{
namespace
{

std::string escape_vim_path(const std::filesystem::path& path)
{
    const std::string value = path.generic_string();
    std::string escaped;
    escaped.reserve(value.size());

    for (char c : value)
    {
        switch (c)
        {
        case ' ':
        case '\\':
        case '|':
        case '"':
        case '%':
        case '#':
            escaped.push_back('\\');
            break;
        default:
            break;
        }
        escaped.push_back(c);
    }

    return escaped;
}

} // namespace

struct NvimHost::Impl
{
    NvimProcess process;
    NvimRpc rpc;
    UiEventHandler ui;
    RenderModel render_model;
    bool running = false;
    bool owns_process = false;
};

std::vector<std::string> build_open_project_tab_commands(const NvimProjectFiles& project)
{
    std::vector<std::string> commands;
    commands.reserve(project.files.size() + 2);
    commands.emplace_back("silent! tabonly");

    for (const auto& file : project.files)
    {
        commands.push_back("tabedit " + escape_vim_path(file));
    }

    if (!project.files.empty())
    {
        commands.emplace_back("tabfirst");
    }

    return commands;
}

NvimHost::NvimHost()
    : m_impl(std::make_unique<Impl>())
{
    m_impl->ui.set_render_model(&m_impl->render_model);
}

NvimHost::~NvimHost()
{
    stop();
}

bool NvimHost::start(const NvimHostOptions& options)
{
    if (m_impl->running)
    {
        return true;
    }

    IRpcTransport* transport = options.transport;
    m_impl->owns_process = transport == nullptr;
    if (!transport)
    {
        auto result = m_impl->process.spawn(options.executable, {}, options.project_root.string());
        if (!result)
        {
            return false;
        }
        transport = &m_impl->process;
    }

    m_impl->render_model.resize(options.columns, options.rows);

    if (!m_impl->rpc.initialize(*transport))
    {
        if (m_impl->owns_process)
        {
            m_impl->process.shutdown();
        }
        return false;
    }

    m_impl->running = true;

    const MpackValue uiOptions = NvimRpc::make_map({
        { NvimRpc::make_str("rgb"), NvimRpc::make_bool(true) },
        { NvimRpc::make_str("ext_linegrid"), NvimRpc::make_bool(true) },
        { NvimRpc::make_str("ext_multigrid"), NvimRpc::make_bool(false) },
    });

    auto attach = m_impl->rpc.request("nvim_ui_attach", {
        NvimRpc::make_int(options.columns),
        NvimRpc::make_int(options.rows),
        uiOptions,
    });
    if (!attach)
    {
        stop();
        return false;
    }

    for (const char* command : { "set termguicolors", "set noshowmode", "set mouse=a" })
    {
        auto result = m_impl->rpc.request("nvim_command", { NvimRpc::make_str(command) });
        if (!result)
        {
            stop();
            return false;
        }
    }

    return true;
}

void NvimHost::stop()
{
    if (!m_impl->running)
    {
        return;
    }

    m_impl->rpc.notify("nvim_input", { NvimRpc::make_str("<C-\\><C-n>:qa!<CR>") });
    m_impl->rpc.shutdown();
    if (m_impl->owns_process)
    {
        m_impl->process.shutdown();
    }

    m_impl->running = false;
}

bool NvimHost::running() const
{
    return m_impl->running && !m_impl->rpc.connection_failed();
}

void NvimHost::resize(int columns, int rows)
{
    m_impl->render_model.resize(columns, rows);
    if (running())
    {
        m_impl->rpc.notify("nvim_ui_try_resize", {
            NvimRpc::make_int(columns),
            NvimRpc::make_int(rows),
        });
    }
}

void NvimHost::open_project_files(const NvimProjectFiles& project)
{
    if (!running())
    {
        return;
    }

    for (const auto& command : build_open_project_tab_commands(project))
    {
        auto result = m_impl->rpc.request("nvim_command", { NvimRpc::make_str(command) });
        if (!result)
        {
            return;
        }
    }
}

void NvimHost::pump()
{
    for (const auto& notification : m_impl->rpc.drain_notifications())
    {
        if (notification.method == "redraw")
        {
            m_impl->ui.process_redraw(notification.params);
        }
    }

    if (m_impl->rpc.connection_failed())
    {
        m_impl->running = false;
    }
}

void NvimHost::send_input(std::string_view input)
{
    if (!running())
    {
        return;
    }

    m_impl->rpc.notify("nvim_input", { NvimRpc::make_str(std::string(input)) });
}

const RenderModel& NvimHost::render_model() const
{
    return m_impl->render_model;
}

} // namespace vklive_nvim
