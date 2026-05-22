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
        std::cerr << "usage: render_parity_harness <Rezonality> <project> [renderer] [expected-exit-code] [expected-output-substring] [--record-one-frame] [--viewports] [--allow-metal-raytracing-unsupported]\n";
        return EXIT_FAILURE;
    }

    const fs::path app = argv[1];
    const fs::path project = argv[2];
    auto isFlag = [](const char* arg) {
        return arg && std::string(arg).rfind("--", 0) == 0;
    };

    int argIndex = 3;
    std::string renderer = "metal";
    if (argIndex < argc && !isFlag(argv[argIndex]))
    {
        renderer = argv[argIndex++];
    }

    int expectedStatus = 0;
    if (argIndex < argc && !isFlag(argv[argIndex]))
    {
        expectedStatus = std::atoi(argv[argIndex++]);
    }

    std::string expectedOutput;
    if (argIndex < argc && !isFlag(argv[argIndex]))
    {
        expectedOutput = argv[argIndex++];
    }

    bool recordOneFrame = false;
    bool viewports = false;
    bool allowMetalRaytracingUnsupported = false;
    for (int i = argIndex; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--record-one-frame")
        {
            recordOneFrame = true;
            continue;
        }
        if (std::string(argv[i]) == "--viewports")
        {
            viewports = true;
            continue;
        }
        if (std::string(argv[i]) == "--allow-metal-raytracing-unsupported")
        {
            allowMetalRaytracingUnsupported = true;
            continue;
        }

        std::cerr << "unknown render parity harness option: " << argv[i] << "\n";
        return EXIT_FAILURE;
    }

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

    auto runTree = project.parent_path().parent_path();
    const auto bundleRunTree = app.parent_path().parent_path() / "Resources" / "run_tree";
    if (fs::exists(bundleRunTree))
    {
        runTree = bundleRunTree;
    }
    else if (fs::exists(app.parent_path() / "run_tree"))
    {
        runTree = app.parent_path() / "run_tree";
    }

    std::vector<std::string> command = {
        app.string(),
        "-ApplePersistenceIgnoreState",
        "YES",
        "--renderer",
        renderer,
        "--project",
        project.string(),
        "--startup-frame-test"
    };
    if (recordOneFrame)
    {
        command.push_back("--record-one-frame");
        std::error_code removeError;
        fs::remove(runTree / "renders" / "Frame_00001.png", removeError);
    }
    if (viewports)
    {
        command.push_back("--viewports");
    }

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
        if (allowMetalRaytracingUnsupported && output.find("Metal ray tracing is unsupported") != std::string::npos)
        {
            std::cout << "render parity skipped native Metal ray tracing on unsupported device\n";
            return EXIT_SUCCESS;
        }
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
    if (recordOneFrame)
    {
        const auto recordingPath = runTree / "renders" / "Frame_00001.png";
        if (!fs::exists(recordingPath))
        {
            std::cerr << output;
            std::cerr << "render parity recording did not create: " << recordingPath << "\n";
            return EXIT_FAILURE;
        }
        if (fs::file_size(recordingPath) == 0)
        {
            std::cerr << output;
            std::cerr << "render parity recording created an empty file: " << recordingPath << "\n";
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
