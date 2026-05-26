#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace vklive_nvim
{

struct ProcessResult
{
    bool ok = false;
    std::string message;

    explicit operator bool() const
    {
        return ok;
    }

    static ProcessResult success()
    {
        return { true, {} };
    }

    static ProcessResult failure(std::string error_message)
    {
        return { false, std::move(error_message) };
    }
};

class NvimProcess
{
public:
    NvimProcess();
    ~NvimProcess();

    NvimProcess(const NvimProcess&) = delete;
    NvimProcess& operator=(const NvimProcess&) = delete;

    ProcessResult spawn(const std::string& nvim_path = "nvim",
        const std::vector<std::string>& extra_args = {},
        const std::string& working_dir = {});
    void shutdown();

    bool write(const std::uint8_t* data, std::size_t len) const;
    int read(std::uint8_t* buffer, std::size_t max_len) const;
    bool is_running() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace vklive_nvim
