#include <algorithm>

#include <zest/logger/logger.h>

#include <nodegraph/canvas.h>
#include <nodegraph/theme.h>
#include <nodegraph/widgets/layout.h>
#include <nodegraph/widgets/widget_label.h>

namespace NodeGraph {

TextLabel::TextLabel(const std::string& label, const std::string& font)
    : Widget(label)
    , m_font(font)
{
}

void TextLabel::Draw(Canvas& canvas)
{
    auto& settings = Zest::GlobalSettingsManager::Instance();
    auto theme = settings.GetCurrentTheme();

    Widget::Draw(canvas);

    auto rc = GetWorldRect();

    // Draw the background area
    rc = DrawSlab(canvas,
        rc,
        settings.GetFloat(theme, s_sliderBorderRadius),
        settings.GetFloat(theme, s_sliderShadowSize),
        settings.GetVec4f(theme, c_sliderShadowColor),
        settings.GetFloat(theme, s_sliderBorderSize),
        settings.GetVec4f(theme, c_sliderBorderColor),
        settings.GetVec4f(theme, c_sliderCenterColor),
        m_label.c_str(),
        2.0f,
        glm::vec4(1.0f),
        0.0f,
        m_font.empty() ? nullptr : m_font.c_str());

    for (auto& child : GetLayout()->GetBackToFront())
    {
        child->Draw(canvas);
    }
}

}
