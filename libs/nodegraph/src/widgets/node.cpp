#include <zest/settings/settings.h>

#include <nodegraph/canvas.h>
#include <nodegraph/theme.h>
#include <nodegraph/widgets/node.h>
#include <nodegraph/widgets/layout.h>

namespace NodeGraph {

Node::Node(const std::string& label)
    : Widget(label)
{

}

void Node::Draw(Canvas& canvas)
{
    auto& settings = Zest::GlobalSettingsManager::Instance();
    auto theme = settings.GetCurrentTheme();
    Widget::Draw(canvas);

    auto rcWorld = GetWorldRect();

    rcWorld = DrawSlab(canvas,
        rcWorld,
        settings.GetFloat(theme, s_nodeBorderRadius),
        settings.GetFloat(theme, s_nodeShadowSize),
        settings.GetVec4f(theme, c_nodeShadowColor),
        settings.GetFloat(theme, s_nodeBorderSize),
        settings.GetVec4f(theme, c_nodeBorderColor),
        settings.GetVec4f(theme, c_nodeCenterColor));

    auto fontSize = settings.GetFloat(theme, s_nodeTitleSize);
    auto titleHeight = fontSize + settings.GetFloat(theme, s_nodeTitleFontPad) * 2.0f;
    auto titlePad = settings.GetFloat(theme, s_nodeTitlePad);

    auto titlePanelRect = NRectf(rcWorld.Left() + titlePad, rcWorld.Top() + titlePad, rcWorld.Width() - titlePad * 2.0f, titleHeight);

    rcWorld = DrawSlab(canvas,
        titlePanelRect,
        settings.GetFloat(theme, s_nodeTitleBorderRadius),
        settings.GetFloat(theme, s_nodeTitleShadowSize),
        settings.GetVec4f(theme, c_nodeTitleShadowColor),
        settings.GetFloat(theme, s_nodeTitleBorderSize),
        settings.GetVec4f(theme, c_nodeTitleBorderColor),
        settings.GetVec4f(theme, c_nodeTitleCenterColor),
        m_label.c_str(),
        settings.GetFloat(theme, s_nodeTitleFontPad),
        TextColorForBackground(settings.GetVec4f(theme, c_nodeTitleCenterColor)));

    auto bottomGap = settings.GetFloat(theme, s_nodeBorderSize) + settings.GetFloat(theme, s_nodeShadowSize);

    // Layout in child coordinates
    auto layoutRect = NRectf(titlePanelRect.Left(), titlePanelRect.Bottom(), titlePanelRect.Width(), GetWorldRect().Bottom() - bottomGap - titlePanelRect.Bottom());
    layoutRect.Adjust(-GetWorldRect().Left(), -GetWorldRect().Top());
    GetLayout()->SetRectWithPad(layoutRect);
    GetLayout()->SetConstraints(glm::uvec2(LayoutConstraint::Preferred, LayoutConstraint::Expanding));

    GetLayout()->Draw(canvas);
}

Widget* Node::MouseDown(CanvasInputState& input)
{
    if (auto pCapture = Widget::MouseDown(input))
    {
        return pCapture;
    }

    if (input.buttonClicked[0])
    {
        auto rcWorld = GetWorldRect();
        auto moveSize = 50.0f;
        auto sizeRect = NRectf(rcWorld.bottomRightPx.x - moveSize, rcWorld.bottomRightPx.y - moveSize, moveSize, moveSize);
        if (sizeRect.Contains(input.lastWorldMouseClick[0]))
        {
            m_moveType = MoveType::Resize;
        }
        else
        {
            m_moveType = MoveType::Move;
        }

        return this;
    }
    return nullptr;
}

void Node::MouseUp(CanvasInputState& input)
{
}

bool Node::MouseMove(CanvasInputState& input)
{

    // Only move top level
    if (input.m_pMouseCapture == this)
    {
        if (m_moveType == MoveType::Resize)
        {
            m_rect.Adjust(0.0f, 0.0f, input.worldMoveDelta.x, input.worldMoveDelta.y);
        }
        else
        {
            m_rect.Adjust(input.worldMoveDelta);
        }
        return true;
    }
    return false;
}

}
