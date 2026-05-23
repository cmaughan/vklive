#include <functional>

#include <fmt/format.h>

#include <zest/logger/logger.h>

#include <nodegraph/canvas.h>
#include <nodegraph/theme.h>
#include <nodegraph/widgets/layout.h>
#include <nodegraph/widgets/widget.h>

namespace NodeGraph {

std::unordered_map<Widget*, TipTimer*> TipTimer::ActiveTips;

Widget::Widget(const std::string& label)
    : m_label(label),
    m_tipTimer(this, 0.5f, 0.25f, 0.25f)
{
}

Widget* Widget::GetParent() const
{
    return m_pParent;
}

void Widget::SetParent(Widget* pParent)
{
    assert(!m_pParent);
    assert(pParent);
    m_pParent = pParent;
}

const NRectf& Widget::GetRect() const
{
    return m_rect;
}

void Widget::SetRect(const NRectf& sz)
{
    if (m_sizeHint.x == 0.0f && m_sizeHint.y == 0.0f)
    {
        m_sizeHint = sz.Size();
    }
    m_rect = sz;
    // LOG(DBG, "Widget: " << GetLabel() << ": " << m_rect);
}

NRectf Widget::ToLocalRect(const NRectf& rc) const
{
    if (!m_pParent)
    {
        return rc;
    }

    auto rcNew = rc.Adjusted(-m_pParent->GetRect().TopLeft());
    // return rcNew;
    return m_pParent->ToLocalRect(rcNew);
}

NRectf Widget::ToWorldRect(const NRectf& rc) const
{
    if (!m_pParent)
    {
        return rc;
    }

    auto rcNew = rc.Adjusted(m_pParent->GetRect().TopLeft());
    return m_pParent->ToWorldRect(rcNew);
}

NRectf Widget::GetWorldRect() const
{
    return ToWorldRect(m_rect);
}

void Widget::Draw(Canvas& canvas)
{
    auto& settings = Zest::GlobalSettingsManager::Instance();
    auto theme = settings.GetCurrentTheme();

    if (settings.GetBool(theme, b_debugShowLayout))
    {
        canvas.FillRect(ToWorldRect(m_rect), glm::vec4(0.1f, 0.5f, 0.1f, 1.0f));
    }

    PostDraw(canvas, m_rect);
}

void Widget::PostDraw(Canvas& canvas, const NRectf& hintRect)
{
    PostDrawSignal(canvas, hintRect);
}

Widget* Widget::MouseDown(CanvasInputState& input)
{
    for (auto& child : GetLayout()->GetFrontToBack())
    {
        if (child->GetWorldRect().Contains(input.worldMousePos))
        {
            if (auto pCapture = child->MouseDown(input))
            {
                return pCapture;
            }
        }
    }
    return this;
}

void Widget::MouseUp(CanvasInputState& input)
{
    for (auto& child : GetLayout()->GetFrontToBack())
    {
        if (child->GetWorldRect().Contains(input.worldMousePos))
        {
            child->MouseUp(input);
            return;
        }
    }
}

void Widget::Visit(const std::function<void(Widget*)>& fnVisit)
{
    fnVisit(this);
    for (auto& child : GetLayout()->GetFrontToBack())
    {
        child->Visit(fnVisit);
    }
}

Widget* Widget::MouseHover(CanvasInputState& input)
{
    for (auto& child : GetLayout()->GetFrontToBack())
    {
        if (child->GetWorldRect().Contains(input.worldMousePos))
        {
            if (auto pCapture = child->MouseHover(input))
            {
                return pCapture;
            }
        }
    }
    return this;
}

bool Widget::MouseMove(CanvasInputState& input)
{
    for (auto& child : GetLayout()->GetFrontToBack())
    {
        if (child->GetWorldRect().Contains(input.worldMousePos))
        {
            return child->MouseMove(input);
        }
    }
    return false;
}

const std::string& Widget::GetLabel() const
{
    return m_label;
}

void Widget::SetLabel(const char* pszLabel)
{
    m_label = pszLabel;
}

NRectf Widget::DrawSlab(Canvas& canvas, const NRectf& rect, float borderRadius, float shadowSize, const glm::vec4& shadowColor, float borderSize, const glm::vec4& borderColor, const glm::vec4& centerColor, const char* pszText, float fontPad, const glm::vec4& textColor, float fontSize, const char* pszFont)
{
    NRectf rc = rect;

    rc.Adjust(shadowSize, shadowSize, 0.0f, 0.0f);
    canvas.FillRoundedRect(rc, borderRadius, shadowColor);

    rc.Adjust(-shadowSize, -shadowSize);

    if (borderSize != 0.0f)
    {
        canvas.FillRoundedRect(rc, borderRadius, borderColor);
        rc.Adjust(borderSize, borderSize, -borderSize, -borderSize);
    }

    canvas.FillRoundedRect(rc, borderRadius, centerColor);

    if (pszText)
    {
        if (fontSize == 0.0f)
        {
            fontSize = rc.Height() - fontPad * 2.0f;
        }
        canvas.Text(glm::vec2(rc.Center().x, rc.Center().y + 0.5f), fontSize, textColor, pszText, pszFont, TEXT_ALIGN_MIDDLE | TEXT_ALIGN_CENTER);
    }
    return rc;
}

const glm::uvec2& Widget::GetConstraints() const
{
    return m_constraints;
}

void Widget::SetConstraints(const glm::uvec2& constraints)
{
    m_constraints = constraints;
}

const glm::vec4& Widget::GetPadding() const
{
    return m_padding;
}

void Widget::SetPadding(const glm::vec4& padding)
{
    m_padding = padding;
}

void Widget::SetLayout(std::shared_ptr<Layout> spLayout)
{
    m_spLayout = spLayout;
    m_spLayout->SetParent(this);
}

Layout* Widget::GetLayout()
{
    if (!m_spLayout)
    {
        SetLayout(std::make_shared<Layout>(LayoutType::Vertical));
    }
    return m_spLayout.get();
}

uint64_t Widget::GetFlags() const
{
    return m_flags;
}

void Widget::SetFlags(uint64_t flags)
{
    m_flags = flags;
}

glm::vec4 Widget::GetMinMaxSize() const
{
    const float minSize = 10.0f; // TBD
    const float maxSize = 100000.0f; // Arbitrary large (not max_float!)
    auto ret = glm::vec4(minSize, minSize, maxSize, maxSize);

    if (m_constraints.x & LayoutConstraint::Preferred)
    {
        ret.x = ret.z = m_rect.Width();
    }
    if (m_constraints.y & LayoutConstraint::Preferred)
    {
        ret.y = ret.w = m_rect.Height();
    }
    return ret;
}

glm::vec2 Widget::GetSizeHint() const
{
    return m_sizeHint;
}

NRectf Widget::GetRectWithPad() const
{
    return m_rect.Adjusted(glm::vec4(-m_padding.x, -m_padding.y, m_padding.z, m_padding.w));
}

void Widget::SetRectWithPad(const NRectf& rc)
{
    SetRect(rc.Adjusted(glm::vec4(m_padding.x, m_padding.y, -m_padding.z, -m_padding.w)));
}

glm::vec4 Widget::TextColorForBackground(const glm::vec4& color)
{
    return Zest::ColorForBackground(color);
}

TipTimer& Widget::GetTipTimer()
{
    return m_tipTimer;
}

bool Widget::IsMouseHover(Canvas& canvas)
{
    auto state = m_tipTimer.GetState();

    switch (state)
    {
    case TipState::On:
    case TipState::Wait:
    case TipState::Decay:
        return true;
    default:
        return false;
        break;
    }
}

bool Widget::IsMouseCapture(Canvas& canvas)
{
    if (canvas.GetInputState().m_pMouseCapture == this)
    {
        return true;
    }
    return false;
}

bool Widget::IsMouseOver(Canvas& canvas)
{
    auto& state = canvas.GetInputState();
    if (state.m_pMouseCapture == this)
    {
        return true;
    }

    for (auto& child : GetLayout()->GetFrontToBack())
    {
        if (child->GetWorldRect().Contains(state.worldMousePos))
        {
            return false;
        }
    }

    if (GetWorldRect().Contains(state.worldMousePos))
    {
        return true;
    }

    return false;
}

void Widget::DrawTip(Canvas& canvas, const glm::vec2& widgetTopCenter, const WidgetValue& val)
{
    if (IsMouseHover(canvas) || IsMouseCapture(canvas))
    {
        auto& tipTimer = GetTipTimer();
        float alpha = 1.0f;
        alpha = tipTimer.Alpha();
        if (alpha == 0.0f)
        {
            return;
        }
        //std::string tip = fmt::format("{}: {} {}", val.name, val.value, val.units);
        std::string tip = fmt::format("{} {}", val.valueText, val.units);

        auto& settings = Zest::GlobalSettingsManager::Instance();
        auto theme = settings.GetCurrentTheme();

        auto tipPad = settings.GetFloat(theme, s_sliderTipFontPad);
        auto fontSize = settings.GetFloat(theme, s_sliderTipFontSize);

        auto rcBounds = canvas.TextBounds(widgetTopCenter, fontSize, tip.c_str(), nullptr, TEXT_ALIGN_TOP | TEXT_ALIGN_LEFT);
        rcBounds.SetHeight(fontSize);

        auto step = fontSize * 1.5f; //(rcBounds.Height()) * 3.0f);

        NRectf panelRect = NRectf(widgetTopCenter.x - rcBounds.Width() / 2.0f - tipPad, widgetTopCenter.y - step, rcBounds.Width() + tipPad * 2.0f, rcBounds.Height() + tipPad * 2.0f);

        auto rc = DrawSlab(canvas,
            panelRect,
            settings.GetFloat(theme, s_sliderTipBorderRadius),
            settings.GetFloat(theme, s_sliderTipShadowSize),
            Zest::ModifyAlpha(settings.GetVec4f(theme, c_sliderTipShadowColor), alpha),
            settings.GetFloat(theme, s_sliderTipBorderSize),
            Zest::ModifyAlpha(settings.GetVec4f(theme, c_sliderTipBorderColor), alpha),
            Zest::ModifyAlpha(settings.GetVec4f(theme, c_sliderTipCenterColor), alpha),
            tip.c_str(),
            4.0f,
            Zest::ModifyAlpha(TextColorForBackground(settings.GetVec4f(theme, c_sliderTipCenterColor)), alpha),
            fontSize);
    }
}

} // Nodegraph
