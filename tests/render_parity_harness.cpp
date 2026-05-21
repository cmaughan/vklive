#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include <reproc++/drain.hpp>
#include <reproc++/reproc.hpp>

namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: render_parity_harness <Rezonality> <project> [renderer] [expected-exit-code] [expected-output-substring]\n";
        return EXIT_FAILURE;
    }

    const fs::path app = argv[1];
    const fs::path project = argv[2];
    const std::string renderer = argc >= 4 ? argv[3] : "metal";
    const int expectedStatus = argc >= 5 ? std::atoi(argv[4]) : 0;
    const std::string expectedOutput = argc >= 6 ? argv[5] : std::string();

    if (!fs::exists(app))
    {
        std::cerr << "missing app executable: " << app << "\n";
        return EXIT_FAILURE;
    }
    if (!fs::exists(project))
    {
        std::cerr << "missing project: " << project << "\n";
        return EXIT_FAILURE;
    }

    const std::vector<std::string> command = {
        app.string(),
        "-ApplePersistenceIgnoreState",
        "YES",
        "--renderer",
        renderer,
        "--project",
        project.string(),
        "--startup-frame-test"
    };

    constexpr uint32_t ProcessWaitMilliseconds = 20000;
    constexpr uint32_t KillProcessMilliseconds = 2000;
    constexpr uint32_t TerminateProcessMilliseconds = 2000;
    const reproc::stop_actions stop = {
        { reproc::stop::wait, reproc::milliseconds(ProcessWaitMilliseconds) },
        { reproc::stop::terminate, reproc::milliseconds(TerminateProcessMilliseconds) },
        { reproc::stop::kill, reproc::milliseconds(KillProcessMilliseconds) }
    };

    reproc::options options;
    options.stop = stop;
    options.redirect.out.type = reproc::redirect::pipe;
    options.redirect.err.type = reproc::redirect::pipe;
    options.deadline = reproc::milliseconds(ProcessWaitMilliseconds);

    reproc::process process;
    std::error_code ec = process.start(command, options);
    if (ec)
    {
        std::cerr << "render parity launch failed: " << ec.message() << "\n";
        return EXIT_FAILURE;
    }

    std::string output;
    reproc::sink::string outputSink(output);
    ec = reproc::drain(process, outputSink, outputSink);
    if (ec)
    {
        std::cerr << output;
        std::cerr << "render parity process output capture failed: " << ec.message() << "\n";
        process.stop(stop);
        return EXIT_FAILURE;
    }

    int status = 0;
    std::tie(status, ec) = process.stop(stop);
    if (ec)
    {
        std::cerr << "render parity process stop failed: " << ec.message() << "\n";
        return EXIT_FAILURE;
    }
    if (status != expectedStatus)
    {
        std::cerr << output;
        std::cerr << "render parity app exited with status: " << status << " (expected " << expectedStatus << ")\n";
        return EXIT_FAILURE;
    }
    if (!expectedOutput.empty() && output.find(expectedOutput) == std::string::npos)
    {
        std::cerr << output;
        std::cerr << "render parity output did not contain expected text: " << expectedOutput << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
