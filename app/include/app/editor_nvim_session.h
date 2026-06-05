#pragma once

#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

#include <vklive_nvim/nvim_host.h>
#include <vklive_nvim/render_model.h>

class INvimHostAdapter
{
public:
    virtual ~INvimHostAdapter() = default;

    virtual bool start(const vklive_nvim::NvimHostOptions& options) = 0;
    virtual void stop() = 0;
    virtual bool running() const = 0;
    virtual void resize(int columns, int rows) = 0;
    virtual void open_project_files(const vklive_nvim::NvimProjectFiles& project) = 0;
    virtual void pump() = 0;
    virtual void send_input(std::string_view input) = 0;
    virtual const vklive_nvim::RenderModel& render_model() const = 0;
    virtual const vklive_nvim::HighlightTable& highlights() const = 0;
};

class NvimEditorSession
{
public:
    NvimEditorSession();
    explicit NvimEditorSession(std::unique_ptr<INvimHostAdapter> host);
    ~NvimEditorSession();

    NvimEditorSession(const NvimEditorSession&) = delete;
    NvimEditorSession& operator=(const NvimEditorSession&) = delete;

    void set_project_root(const std::filesystem::path& root);
    void set_files(std::vector<std::filesystem::path> files, bool activate_first);

    bool ensure_started(int columns, int rows);
    bool running() const;
    void pump();
    void stop();
    void send_input(std::string_view input);

    const vklive_nvim::RenderModel& render_model() const;
    const vklive_nvim::HighlightTable& highlights() const;

private:
    void sync_project_files();

    std::unique_ptr<INvimHostAdapter> m_host;
    std::filesystem::path m_projectRoot;
    std::vector<std::filesystem::path> m_files;
    bool m_activateFirst = false;
    bool m_pendingProjectSync = true;
    int m_columns = 0;
    int m_rows = 0;
};
