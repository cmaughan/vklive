#include <app/nodegraph_theme.h>

#include <exception>

#include <glm/glm.hpp>

#include <nodegraph/theme.h>

#include <zest/settings/settings.h>

namespace
{

void set_default(Zest::SettingsManager& settings, const Zest::StringId& theme, const Zest::StringId& id, const Zest::SettingValue& value)
{
    auto& section = settings.GetSection(theme);
    if (section.find(id) == section.end())
    {
        settings.Set(theme, id, value);
    }
}

void set_color(Zest::SettingsManager& settings, const Zest::StringId& theme, const Zest::StringId& id, const glm::vec4& value)
{
    set_default(settings, theme, id, value);
}

void set_float(Zest::SettingsManager& settings, const Zest::StringId& theme, const Zest::StringId& id, float value)
{
    set_default(settings, theme, id, value);
}

void set_bool(Zest::SettingsManager& settings, const Zest::StringId& theme, const Zest::StringId& id, bool value)
{
    set_default(settings, theme, id, value);
}

} // namespace

void nodegraph_seed_default_theme()
{
    auto& settings = Zest::GlobalSettingsManager::Instance();
    const auto& theme = settings.GetCurrentTheme();

    set_bool(settings, theme, NodeGraph::b_debugShowLayout, false);

    set_color(settings, theme, NodeGraph::c_gridLines, glm::vec4(0.25f, 0.25f, 0.25f, 1.0f));
    set_float(settings, theme, NodeGraph::s_gridLineSize, 2.0f);

    set_float(settings, theme, NodeGraph::s_nodeBorderRadius, 4.0f);
    set_float(settings, theme, NodeGraph::s_nodeShadowSize, 2.0f);
    set_float(settings, theme, NodeGraph::s_nodeBorderSize, 2.0f);
    set_color(settings, theme, NodeGraph::c_nodeShadowColor, glm::vec4(0.10f, 0.10f, 0.10f, 0.5f));
    set_color(settings, theme, NodeGraph::c_nodeCenterColor, glm::vec4(0.262f, 0.262f, 0.262f, 1.0f));
    set_color(settings, theme, NodeGraph::c_nodeBorderColor, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

    set_float(settings, theme, NodeGraph::s_nodeTitleSize, 26.0f);
    set_float(settings, theme, NodeGraph::s_nodeTitleFontPad, 2.0f);
    set_float(settings, theme, NodeGraph::s_nodeTitleBorder, 0.0f);
    set_float(settings, theme, NodeGraph::s_nodeTitleBorderRadius, 8.0f);
    set_float(settings, theme, NodeGraph::s_nodeTitlePad, 4.0f);
    set_float(settings, theme, NodeGraph::s_nodeTitleShadowSize, 0.0f);
    set_float(settings, theme, NodeGraph::s_nodeTitleBorderSize, 0.0f);
    set_color(settings, theme, NodeGraph::c_nodeTitleShadowColor, glm::vec4(0.196f, 0.196f, 0.196f, 1.0f));
    set_color(settings, theme, NodeGraph::c_nodeTitleCenterColor, glm::vec4(0.820f, 0.137f, 0.137f, 1.0f));
    set_color(settings, theme, NodeGraph::c_nodeTitleBorderColor, glm::vec4(0.320f, 0.320f, 0.320f, 1.0f));

    set_float(settings, theme, NodeGraph::s_sliderBorderSize, 0.0f);
    set_float(settings, theme, NodeGraph::s_sliderThumbPad, 2.0f);
    set_float(settings, theme, NodeGraph::s_sliderThumbShadowSize, 0.0f);
    set_float(settings, theme, NodeGraph::s_sliderThumbRadius, 4.0f);
    set_color(settings, theme, NodeGraph::c_sliderThumbShadowColor, glm::vec4(0.545f, 0.410f, 0.410f, 1.0f));
    set_color(settings, theme, NodeGraph::c_sliderThumbColor, glm::vec4(0.761f, 0.134f, 0.134f, 1.0f));
    set_float(settings, theme, NodeGraph::s_sliderBorderRadius, 4.0f);
    set_float(settings, theme, NodeGraph::s_sliderShadowSize, 0.0f);
    set_float(settings, theme, NodeGraph::s_sliderFontPad, 4.0f);
    set_color(settings, theme, NodeGraph::c_sliderBorderColor, glm::vec4(0.300f, 0.300f, 0.300f, 1.0f));
    set_color(settings, theme, NodeGraph::c_sliderCenterColor, glm::vec4(0.107f, 0.085f, 0.085f, 1.0f));
    set_color(settings, theme, NodeGraph::c_sliderShadowColor, glm::vec4(0.337f, 0.326f, 0.326f, 1.0f));

    set_float(settings, theme, NodeGraph::s_sliderTipBorderRadius, 5.0f);
    set_float(settings, theme, NodeGraph::s_sliderTipShadowSize, 4.0f);
    set_float(settings, theme, NodeGraph::s_sliderTipFontPad, 7.0f);
    set_float(settings, theme, NodeGraph::s_sliderTipFontSize, 36.0f);
    set_float(settings, theme, NodeGraph::s_sliderTipBorderSize, 0.0f);
    set_color(settings, theme, NodeGraph::c_sliderTipBorderColor, glm::vec4(0.767f, 0.767f, 0.767f, 1.0f));
    set_color(settings, theme, NodeGraph::c_sliderTipCenterColor, glm::vec4(0.231f, 0.231f, 0.231f, 1.0f));
    set_color(settings, theme, NodeGraph::c_sliderTipShadowColor, glm::vec4(0.110f, 0.110f, 0.110f, 1.0f));
    set_color(settings, theme, NodeGraph::c_sliderTipFontColor, glm::vec4(0.679f, 0.766f, 0.839f, 1.0f));

    set_color(settings, theme, NodeGraph::c_labelColor, glm::vec4(0.820f, 0.820f, 0.820f, 1.0f));
    set_color(settings, theme, NodeGraph::c_knobChannelColor, glm::vec4(0.380f, 0.380f, 0.380f, 1.0f));
    set_color(settings, theme, NodeGraph::c_knobShadowColor, glm::vec4(0.222f, 0.220f, 0.220f, 1.0f));
    set_color(settings, theme, NodeGraph::c_knobChannelHLColor, glm::vec4(0.821f, 0.137f, 0.137f, 1.0f));
    set_color(settings, theme, NodeGraph::c_knobMarkColor, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    set_color(settings, theme, NodeGraph::c_knobMarkHLColor, glm::vec4(0.694f, 0.682f, 0.682f, 1.0f));
    set_color(settings, theme, NodeGraph::c_knobFillColor, glm::vec4(0.451f, 0.451f, 0.451f, 1.0f));
    set_color(settings, theme, NodeGraph::c_knobFillHLColor, glm::vec4(0.493f, 0.493f, 0.493f, 1.0f));
    set_color(settings, theme, NodeGraph::c_knobTextColor, glm::vec4(0.820f, 0.820f, 0.820f, 1.0f));
    set_float(settings, theme, NodeGraph::s_knobChannelWidth, 8.0f);
    set_float(settings, theme, NodeGraph::s_knobChannelGap, 3.0f);
    set_float(settings, theme, NodeGraph::s_knobShadowSize, 3.0f);
    set_float(settings, theme, NodeGraph::s_knobTextSize, 24.0f);
    set_float(settings, theme, NodeGraph::s_knobTextInset, 3.0f);

    set_color(settings, theme, NodeGraph::c_socketColor, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    set_color(settings, theme, NodeGraph::c_socketShadowColor, glm::vec4(0.100f, 0.100f, 0.100f, 0.4f));
    set_color(settings, theme, NodeGraph::c_waveSliderCenterColor, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f));
    set_color(settings, theme, NodeGraph::c_waveSliderBorderColor, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
}

bool nodegraph_load_theme_file(const fs::path& path)
{
    nodegraph_seed_default_theme();
    if (path.empty() || !fs::exists(path))
    {
        return false;
    }

    try
    {
        return Zest::GlobalSettingsManager::Instance().Load(path);
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool nodegraph_save_theme_file(const fs::path& path)
{
    if (path.empty())
    {
        return false;
    }

    try
    {
        fs::create_directories(path.parent_path());
        return Zest::GlobalSettingsManager::Instance().Save(path);
    }
    catch (const std::exception&)
    {
        return false;
    }
}
