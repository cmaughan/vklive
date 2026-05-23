#include <cstdlib>
#include <iostream>
#include <string>

#include <app/window_nodegraph.h>
#include <app/menu.h>

#include <zest/logger/logger.h>

namespace Zest
{
Logger logger{ false, LT::DBG };
bool Log::disabled = false;
} // namespace Zest

WindowEnables g_WindowEnables;

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

} // namespace

int main()
{
    NodeGraphWindow window;

    bool ok = true;
    ok &= require(!window.HasCanvas(), "nodegraph window should start without a canvas");

    window.BuildDemoGraphForTests();

    ok &= require(window.HasCanvas(), "nodegraph test graph should create a canvas");
    ok &= require(window.NodeCountForTests() >= 2, "nodegraph test graph should contain multiple nodes");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
