#include <reproc++/drain.hpp>
#include <reproc++/reproc.hpp>
#include <reproc++/run.hpp>

#include <vector>
#include <cassert>
#include <iostream>
#include <sstream>

#include <vklive/process/process.h>
#include <vklive/logger/logger.h>

std::error_code run_process(const std::vector<std::string>& args, std::string* pOutput)
{
    assert(!args.empty());

    const uint32_t ProcessWaitMilliseconds = 10000;
    const uint32_t KillProcessMilliseconds = 2000;
    const uint32_t TerminateProcessMilliseconds = 2000;

    // Wait for process, then terminate it
    reproc::stop_actions stop = {
        { reproc::stop::wait, reproc::milliseconds(ProcessWaitMilliseconds) },
        { reproc::stop::terminate, reproc::milliseconds(TerminateProcessMilliseconds) },
        { reproc::stop::kill, reproc::milliseconds(KillProcessMilliseconds) }
    };

    reproc::options options;
    options.stop = stop;

    reproc::process proc;
    std::error_code ec = proc.start(args, options);
    if (ec == std::errc::no_such_file_or_directory)
    {
        std::ostringstream str;
        str << "RunProcess - Program Not Found";
        if (!args.empty())
            str << " : " << args[0];
        LOG(ERROR, str.str());
        return ec;
    }
    else if (ec)
    {
        LOG(ERROR, "RunProcess - " << ec.message());
        return ec;
    }

    if (pOutput)
    {
        reproc::sink::string sink(*pOutput);
        ec = reproc::drain(proc, sink, sink);
        if (ec)
        {
            LOG(ERROR, "RunProcess Draining - " << ec.message());
            return ec;
        }
    }

    // Call `process::stop` manually so we can access the exit status.
    int status = 0;
    std::tie(status, ec) = proc.stop(options.stop);
    if (ec)
    {
        LOG(ERROR, "RunProcess - " << ec.message());
        return ec;
    }
    return ec;
}
