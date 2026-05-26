#include <vklive_nvim/nvim_process.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <climits>
#include <cstring>
#include <sstream>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <array>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#endif

namespace vklive_nvim
{

struct NvimProcess::Impl
{
#ifdef _WIN32
    std::atomic<HANDLE> child_stdin_write{ INVALID_HANDLE_VALUE };
    std::atomic<HANDLE> child_stdout_read{ INVALID_HANDLE_VALUE };
    PROCESS_INFORMATION proc_info = {};
#else
    std::atomic<int> child_stdin_write{ -1 };
    std::atomic<int> child_stdout_read{ -1 };
    std::atomic<pid_t> child_pid{ -1 };
#endif
    std::atomic<bool> started{ false };
};

NvimProcess::NvimProcess()
    : m_impl(std::make_unique<Impl>())
{
}

NvimProcess::~NvimProcess()
{
    shutdown();
}

#ifdef _WIN32
namespace
{

std::string quote_windows_arg(const std::string& value)
{
    if (value.find_first_of(" \t\"") == std::string::npos)
    {
        return value;
    }

    std::string quoted = "\"";
    size_t backslashes = 0;
    for (char ch : value)
    {
        if (ch == '\\')
        {
            ++backslashes;
            quoted.push_back(ch);
            continue;
        }
        if (ch == '"')
        {
            quoted.append(backslashes, '\\');
            quoted.push_back('\\');
        }
        backslashes = 0;
        quoted.push_back(ch);
    }
    quoted.append(backslashes, '\\');
    quoted.push_back('"');
    return quoted;
}

bool ascii_iequals(std::string_view lhs, std::string_view rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (size_t i = 0; i < lhs.size(); ++i)
    {
        const unsigned char a = static_cast<unsigned char>(lhs[i]);
        const unsigned char b = static_cast<unsigned char>(rhs[i]);
        if (std::tolower(a) != std::tolower(b))
        {
            return false;
        }
    }
    return true;
}

std::vector<char> build_windows_environment_block_with_term_dumb()
{
    LPCH inherited_block = GetEnvironmentStringsA();
    if (!inherited_block)
    {
        return { 'T', 'E', 'R', 'M', '=', 'd', 'u', 'm', 'b', '\0', '\0' };
    }

    std::vector<std::string> entries;
    bool term_overridden = false;
    for (const char* cursor = inherited_block; *cursor != '\0'; cursor += std::strlen(cursor) + 1)
    {
        std::string entry(cursor);
        if (!entry.empty() && entry[0] != '=')
        {
            const size_t equals = entry.find('=');
            if (equals != std::string::npos && ascii_iequals(std::string_view(entry.data(), equals), "TERM"))
            {
                entry = "TERM=dumb";
                term_overridden = true;
            }
        }
        entries.push_back(std::move(entry));
    }
    FreeEnvironmentStringsA(inherited_block);

    if (!term_overridden)
    {
        entries.emplace_back("TERM=dumb");
    }

    size_t total_bytes = 1;
    for (const auto& entry : entries)
    {
        total_bytes += entry.size() + 1;
    }

    std::vector<char> environment_block(total_bytes, '\0');
    char* out = environment_block.data();
    for (const auto& entry : entries)
    {
        std::memcpy(out, entry.data(), entry.size());
        out += entry.size();
        *out++ = '\0';
    }
    *out = '\0';
    return environment_block;
}

} // namespace

ProcessResult NvimProcess::spawn(const std::string& nvim_path, const std::vector<std::string>& extra_args, const std::string& working_dir)
{
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE stdin_read = INVALID_HANDLE_VALUE;
    HANDLE stdin_write = INVALID_HANDLE_VALUE;
    HANDLE stdout_read = INVALID_HANDLE_VALUE;
    HANDLE stdout_write = INVALID_HANDLE_VALUE;

    if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0))
    {
        return ProcessResult::failure("Failed to create stdin pipe");
    }
    SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0))
    {
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        return ProcessResult::failure("Failed to create stdout pipe");
    }
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    HANDLE nul_handle = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    si.hStdError = nul_handle;
    si.dwFlags |= STARTF_USESTDHANDLES;

    std::ostringstream command;
    command << quote_windows_arg(nvim_path) << " --embed";
    for (const auto& arg : extra_args)
    {
        command << ' ' << quote_windows_arg(arg);
    }

    std::string cmd = command.str();
    std::vector<char> env_block = build_windows_environment_block_with_term_dumb();
    const char* cwd = working_dir.empty() ? nullptr : working_dir.c_str();

    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, env_block.empty() ? nullptr : env_block.data(), cwd, &si, &m_impl->proc_info))
    {
        const DWORD err = GetLastError();
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        if (nul_handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(nul_handle);
        }
        return ProcessResult::failure("CreateProcess failed (GetLastError=" + std::to_string(err) + ")");
    }

    CloseHandle(stdin_read);
    CloseHandle(stdout_write);
    if (nul_handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(nul_handle);
    }

    m_impl->child_stdin_write.store(stdin_write, std::memory_order_relaxed);
    m_impl->child_stdout_read.store(stdout_read, std::memory_order_relaxed);
    m_impl->started.store(true, std::memory_order_release);
    return ProcessResult::success();
}

void NvimProcess::shutdown()
{
    if (!m_impl->started.load(std::memory_order_acquire))
    {
        return;
    }

    HANDLE stdin_h = m_impl->child_stdin_write.exchange(INVALID_HANDLE_VALUE, std::memory_order_acq_rel);
    if (stdin_h != INVALID_HANDLE_VALUE)
    {
        CloseHandle(stdin_h);
    }

    HANDLE stdout_h = m_impl->child_stdout_read.exchange(INVALID_HANDLE_VALUE, std::memory_order_acq_rel);
    if (stdout_h != INVALID_HANDLE_VALUE)
    {
        CloseHandle(stdout_h);
    }

    if (m_impl->proc_info.hProcess)
    {
        WaitForSingleObject(m_impl->proc_info.hProcess, 2000);
        TerminateProcess(m_impl->proc_info.hProcess, 0);
        CloseHandle(m_impl->proc_info.hProcess);
        CloseHandle(m_impl->proc_info.hThread);
        m_impl->proc_info = {};
    }

    m_impl->started.store(false, std::memory_order_release);
}

bool NvimProcess::write(const std::uint8_t* data, std::size_t len)
{
    HANDLE h = m_impl->child_stdin_write.load(std::memory_order_acquire);
    if (h == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    size_t total_written = 0;
    while (total_written < len)
    {
        DWORD written = 0;
        DWORD to_write = static_cast<DWORD>(std::min<std::size_t>(len - total_written, MAXDWORD));
        if (!WriteFile(h, data + total_written, to_write, &written, nullptr))
        {
            return false;
        }
        if (written == 0)
        {
            return false;
        }
        total_written += written;
    }
    return true;
}

int NvimProcess::read(std::uint8_t* buffer, std::size_t max_len)
{
    HANDLE h = m_impl->child_stdout_read.load(std::memory_order_acquire);
    if (h == INVALID_HANDLE_VALUE)
    {
        return -1;
    }

    DWORD bytes_read = 0;
    if (!ReadFile(h, buffer, static_cast<DWORD>(max_len), &bytes_read, nullptr))
    {
        return -1;
    }
    return bytes_read > static_cast<DWORD>(INT_MAX) ? -1 : static_cast<int>(bytes_read);
}

bool NvimProcess::is_running() const
{
    if (!m_impl->started.load(std::memory_order_acquire))
    {
        return false;
    }

    DWORD exit_code = 0;
    GetExitCodeProcess(m_impl->proc_info.hProcess, &exit_code);
    return exit_code == STILL_ACTIVE;
}

#else

ProcessResult NvimProcess::spawn(const std::string& nvim_path, const std::vector<std::string>& extra_args, const std::string& working_dir)
{
    std::array<int, 2> stdin_pipe;
    std::array<int, 2> stdout_pipe;
    std::array<int, 2> exec_status_pipe;

    if (pipe(stdin_pipe.data()) != 0)
    {
        return ProcessResult::failure(std::string("Failed to create stdin pipe: ") + strerror(errno));
    }
    if (pipe(stdout_pipe.data()) != 0)
    {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        return ProcessResult::failure(std::string("Failed to create stdout pipe: ") + strerror(errno));
    }
    if (pipe(exec_status_pipe.data()) != 0)
    {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return ProcessResult::failure(std::string("Failed to create exec-status pipe: ") + strerror(errno));
    }

    if (fcntl(exec_status_pipe[1], F_SETFD, FD_CLOEXEC) != 0)
    {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(exec_status_pipe[0]);
        close(exec_status_pipe[1]);
        return ProcessResult::failure(std::string("Failed to configure exec-status pipe: ") + strerror(errno));
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(exec_status_pipe[0]);
        close(exec_status_pipe[1]);
        return ProcessResult::failure(std::string("fork() failed: ") + strerror(errno));
    }

    if (pid == 0)
    {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(exec_status_pipe[0]);

        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);

        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0)
        {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        const int max_fd = static_cast<int>(sysconf(_SC_OPEN_MAX));
        const int limit = max_fd > 0 ? max_fd : 1024;
        for (int fd = STDERR_FILENO + 1; fd < limit; ++fd)
        {
            if (fd == exec_status_pipe[1])
            {
                continue;
            }
            close(fd);
        }

        signal(SIGPIPE, SIG_DFL);
        setenv("TERM", "dumb", 1);

        if (!working_dir.empty() && chdir(working_dir.c_str()) != 0)
        {
            int chdir_errno = errno;
            (void)!::write(exec_status_pipe[1], &chdir_errno, sizeof(chdir_errno));
            close(exec_status_pipe[1]);
            _exit(127);
        }

        std::vector<std::string> argv_storage;
        argv_storage.reserve(extra_args.size() + 2);
        argv_storage.push_back(nvim_path);
        argv_storage.emplace_back("--embed");
        for (const auto& arg : extra_args)
        {
            argv_storage.push_back(arg);
        }

        std::vector<char*> argv;
        argv.reserve(argv_storage.size() + 1);
        for (auto& arg : argv_storage)
        {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);

        execvp(nvim_path.c_str(), argv.data());
        int exec_errno = errno;
        (void)!::write(exec_status_pipe[1], &exec_errno, sizeof(exec_errno));
        close(exec_status_pipe[1]);
        _exit(127);
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    close(exec_status_pipe[1]);

    int exec_errno = 0;
    const ssize_t status_bytes = ::read(exec_status_pipe[0], &exec_errno, sizeof(exec_errno));
    close(exec_status_pipe[0]);
    if (status_bytes > 0)
    {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        int status = 0;
        waitpid(pid, &status, 0);
        return ProcessResult::failure(std::string("execvp(") + nvim_path + ") failed: " + strerror(exec_errno));
    }

    m_impl->child_stdin_write.store(stdin_pipe[1], std::memory_order_relaxed);
    m_impl->child_stdout_read.store(stdout_pipe[0], std::memory_order_relaxed);
    m_impl->child_pid.store(pid, std::memory_order_relaxed);
    m_impl->started.store(true, std::memory_order_release);
    return ProcessResult::success();
}

void NvimProcess::shutdown()
{
    if (!m_impl->started.load(std::memory_order_acquire))
    {
        return;
    }

    int stdin_fd = m_impl->child_stdin_write.exchange(-1, std::memory_order_acq_rel);
    if (stdin_fd >= 0)
    {
        close(stdin_fd);
    }

    int stdout_fd = m_impl->child_stdout_read.exchange(-1, std::memory_order_acq_rel);
    if (stdout_fd >= 0)
    {
        close(stdout_fd);
    }

    pid_t pid = m_impl->child_pid.exchange(-1, std::memory_order_acq_rel);
    if (pid > 0)
    {
        int status = 0;
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == 0)
        {
            kill(pid, SIGTERM);
            std::thread([pid]() {
                using namespace std::chrono;
                const auto deadline = steady_clock::now() + milliseconds(500);
                int s = 0;
                while (steady_clock::now() < deadline)
                {
                    if (waitpid(pid, &s, WNOHANG) != 0)
                    {
                        return;
                    }
                    std::this_thread::sleep_for(milliseconds(20));
                }
                kill(pid, SIGKILL);
                waitpid(pid, &s, 0);
            }).detach();
        }
    }

    m_impl->started.store(false, std::memory_order_release);
}

bool NvimProcess::write(const std::uint8_t* data, std::size_t len)
{
    int fd = m_impl->child_stdin_write.load(std::memory_order_acquire);
    if (fd < 0)
    {
        return false;
    }

    size_t total_written = 0;
    while (total_written < len)
    {
        ssize_t n = ::write(fd, data + total_written, len - total_written);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return false;
        }
        if (n == 0)
        {
            return false;
        }
        total_written += static_cast<size_t>(n);
    }
    return true;
}

int NvimProcess::read(std::uint8_t* buffer, std::size_t max_len)
{
    int fd = m_impl->child_stdout_read.load(std::memory_order_acquire);
    if (fd < 0)
    {
        return -1;
    }

    ssize_t n = 0;
    do
    {
        n = ::read(fd, buffer, max_len);
    } while (n < 0 && errno == EINTR);
    if (n < 0)
    {
        return -1;
    }
    return static_cast<int>(n);
}

bool NvimProcess::is_running() const
{
    if (!m_impl->started.load(std::memory_order_acquire))
    {
        return false;
    }

    pid_t pid = m_impl->child_pid.load(std::memory_order_acquire);
    if (pid <= 0)
    {
        return false;
    }

    int status = 0;
    const pid_t result = waitpid(pid, &status, WNOHANG);
    return result == 0;
}

#endif

} // namespace vklive_nvim
