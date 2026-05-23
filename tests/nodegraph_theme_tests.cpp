#include <cstdlib>
#include <iostream>
#include <string>

#include <app/nodegraph_theme.h>

#include <nodegraph/theme.h>

#include <zest/logger/logger.h>
#include <zest/settings/settings.h>

namespace Zest
{
Logger logger{ false, LT::DBG };
bool Log::disabled = false;
} // namespace Zest

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
    nodegraph_seed_default_theme();

    const auto& settings = Zest::GlobalSettingsManager::Instance();
    const auto& theme = settings.GetCurrentTheme();

    bool ok = true;
    ok &= require(settings.GetFloat(theme, NodeGraph::s_gridLineSize) > 0.0f, "nodegraph grid size should be seeded");
    ok &= require(settings.GetVec4f(theme, NodeGraph::c_nodeCenterColor).w > 0.0f, "nodegraph node center color should be seeded");
    ok &= require(!settings.GetBool(theme, NodeGraph::b_debugShowLayout), "nodegraph debug layout should default off");

    auto& mutableSettings = Zest::GlobalSettingsManager::Instance();
    const glm::vec4 customNodeColor(0.1f, 0.2f, 0.3f, 0.4f);
    mutableSettings.Set(theme, NodeGraph::c_nodeCenterColor, customNodeColor);
    nodegraph_seed_default_theme();
    ok &= require(settings.GetVec4f(theme, NodeGraph::c_nodeCenterColor) == customNodeColor, "nodegraph theme seeding should not overwrite user values");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
