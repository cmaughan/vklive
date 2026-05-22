#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>

#include <zest/file/file.h>
#include <zest/logger/logger.h>

namespace fs = std::filesystem;

namespace Zest
{
Logger logger{ false, LT::DBG };
bool Log::disabled = false;
}

namespace
{

bool require(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << message << "\n";
        return false;
    }
    return true;
}

std::optional<size_t> open_descriptor_count()
{
#if defined(__APPLE__) || defined(__linux__)
    const fs::path fdRoot = fs::exists("/proc/self/fd") ? fs::path("/proc/self/fd") : fs::path("/dev/fd");
    size_t count = 0;
    for (const auto& _ : fs::directory_iterator(fdRoot))
    {
        (void)_;
        ++count;
    }
    return count;
#else
    return std::nullopt;
#endif
}

bool write_text(const fs::path& path, const std::string& text)
{
    std::ofstream file(path);
    if (!file)
    {
        return false;
    }
    file << text;
    return true;
}

bool test_file_gather_files_closes_directory_handles()
{
    auto before = open_descriptor_count();
    if (!before)
    {
        return true;
    }

    const fs::path root = fs::temp_directory_path() / "vklive_zest_file_tests";
    fs::remove_all(root);
    fs::create_directories(root / "nested");
    if (!write_text(root / "default.scenegraph", "pass: Test {}\n")
        || !write_text(root / "nested" / "screen.frag", "#version 450\n"))
    {
        std::cerr << "could not write test files under " << root << "\n";
        return false;
    }

    for (int i = 0; i < 64; ++i)
    {
        const auto files = Zest::file_gather_files(root, false);
        if (!require(!files.empty(), "file_gather_files should return the root file"))
        {
            return false;
        }
    }

    auto after = open_descriptor_count();
    fs::remove_all(root);
    if (!after)
    {
        return true;
    }

    return require(*after <= *before + 4,
        "file_gather_files leaked directory handles: before=" + std::to_string(*before) + " after=" + std::to_string(*after));
}

} // namespace

int main()
{
    bool ok = true;
    ok &= test_file_gather_files_closes_directory_handles();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
