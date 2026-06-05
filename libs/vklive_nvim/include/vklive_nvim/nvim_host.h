#pragma once

#include <filesystem>
#include <memory>
#include <string_view>
#include <string>
#include <vector>

#include <vklive_nvim/highlight.h>
#include <vklive_nvim/nvim_rpc.h>

namespace vklive_nvim
{

class RenderModel;

struct NvimProjectFiles
{
    std::filesystem::path project_root;
    std::vector<std::filesystem::path> files;
};

struct NvimHostOptions
{
    std::filesystem::path project_root;
    std::string executable = "nvim";
    int columns = 80;
    int rows = 24;
    IRpcTransport* transport = nullptr;
};

std::vector<std::string> build_open_project_tab_commands(const NvimProjectFiles& project);

class NvimHost
{
public:
    NvimHost();
    ~NvimHost();

    NvimHost(const NvimHost&) = delete;
    NvimHost& operator=(const NvimHost&) = delete;

    bool start(const NvimHostOptions& options);
    void stop();
    bool running() const;

    void resize(int columns, int rows);
    void open_project_files(const NvimProjectFiles& project);
    void pump();
    void send_input(std::string_view input);

    const RenderModel& render_model() const;
    const HighlightTable& highlights() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace vklive_nvim
