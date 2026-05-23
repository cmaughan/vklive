#include <algorithm>
#include <fmt/format.h>

#include <zest/logger/logger.h>

#include <nodegraph/canvas.h>
#include <nodegraph/theme.h>
#include <nodegraph/widgets/layout.h>
#include <nodegraph/widgets/widget_knob.h>

namespace NodeGraph {

Knob::Knob(const std::string& label, IKnobCB* pCB)
    : Widget(label)
    , m_pCB(pCB)
{
    m_value.name = label;
    m_value.step = 0.2f;
}

void Knob::Draw(Canvas& canvas)
{
    auto& settings = Zest::GlobalSettingsManager::Instance();
    auto theme = settings.GetCurrentTheme();
    Widget::Draw(canvas);

    auto rc = GetWorldRect();

    if (settings.GetBool(theme, b_debugShowLayout))
    {
        DrawSlab(canvas,
            rc,
            0.0f,
            0.0f,
            glm::vec4(0.0f),
            0.0f,
            glm::vec4(0.0f),
            glm::vec4(0.2f, 0.4f, 0.6f, 1.0f));
    }

    KnobValue val;
    if (m_pCB)
    {
        m_pCB->UpdateKnob(this, KnobOp::Get, val);
    }
    else
    {
        UpdateKnob(this, KnobOp::Get, val);
    }

    auto textSize = settings.GetFloat(theme, s_knobTextSize);
    auto pack = settings.GetFloat(theme, s_knobTextInset);

    NRectf remain = rc.Adjusted(glm::vec4(0.0f, 0.0f, 0.0f, -(textSize - pack)));
    NRectf textRect = rc.Adjusted(glm::vec4(0.0f, rc.Height() - textSize, 0.0f, -pack));

    auto knobSize = remain.ShortSide();

    auto knobRegion = NRectf(remain.Center() - glm::vec2(knobSize * .5f), remain.Center() + glm::vec2(knobSize * .5f));
    knobSize *= 0.5f;
    auto markerInset = knobSize * .25f;
    auto arcOffset = -270.0f;

    // Degree ranges for the ImGui origin, which is 90 degrees from top
    auto startArc = 55.0f;
    auto endArc = 360.0f - 55.0f;

    // The full range
    float arcRange = (endArc - startArc);

    // Figure out where the position is on the arc
    float ratioPos = val.value;
    auto posArc = startArc + arcRange * ratioPos;

    // Figure out where the origin is on the arc
    float ratioOrigin = 0.0f;
    auto posArcBegin = startArc + arcRange * ratioOrigin;

    auto channelWidth = settings.GetFloat(theme, s_knobChannelWidth);
    auto channelGap = settings.GetFloat(theme, s_knobChannelGap);

    auto innerSize = knobSize - channelWidth - channelGap;

    auto shadowColor = settings.GetVec4f(theme, c_knobShadowColor);

    // Knob surrounding shadow; a filled circle behind it
    canvas.FilledCircle(knobRegion.Center(), innerSize, shadowColor);

    innerSize -= settings.GetFloat(theme, s_knobShadowSize);

    auto color = settings.GetVec4f(theme, c_knobFillColor);
    auto colorHL = settings.GetVec4f(theme, c_knobFillHLColor);
    auto markColor = settings.GetVec4f(theme, c_knobMarkColor);
    auto markHLColor = settings.GetVec4f(theme, c_knobMarkHLColor);
    auto channelHLColor = settings.GetVec4f(theme, c_knobChannelHLColor);
    auto channelColor = settings.GetVec4f(theme, c_knobChannelColor);

    auto shadowSize = settings.GetFloat(theme, s_knobShadowSize);

    if (val.flags & KnobFlags::ReadOnly)
    {
        color.w = .6f;
        colorHL.w = .6f;
        markColor.w = .6f;
        channelHLColor.w = .6f;
    }
    /*
    else if (hover || captured)
    {
        markColor = markHLColor;
        color = settings.Get(color_controlFillColorHL);
    }
    */

    // Only draw the actual knob if big enough
    if (!m_mini)
    {
        canvas.FilledGradientCircle(knobRegion.Center(), innerSize, NRectf(knobRegion.Center().x, knobRegion.Center().y - innerSize, 0, innerSize * 1.5f), colorHL, color);

        // the notch on the button/indicator
        auto markerAngle = Zest::degToRad(posArc + arcOffset);
        auto markVector = glm::vec2(std::cos(markerAngle), std::sin(markerAngle));
        canvas.Stroke(knobRegion.Center() + markVector * (markerInset - shadowSize), knobRegion.Center() + markVector * (innerSize - shadowSize), channelWidth, shadowColor);
        canvas.Stroke(knobRegion.Center() + markVector * markerInset, knobRegion.Center() + markVector * (innerSize - shadowSize * 2), channelWidth - shadowSize, markColor);
    }
    else
    {
        float size = knobSize - channelWidth;
        canvas.FilledGradientCircle(knobRegion.Center(), size, NRectf(knobRegion.Center().x, knobRegion.Center().y - size, 0, size * 1.5f), colorHL, color);
    }

    canvas.Arc(knobRegion.Center(), knobSize - channelWidth * .5f, channelWidth, channelColor, startArc + arcOffset, endArc + arcOffset);

    // Cover the shortest arc between the 2 points
    if (posArcBegin > posArc)
    {
        std::swap(posArcBegin, posArc);
    }

    canvas.Arc(knobRegion.Center(), knobSize - channelWidth * .5f, channelWidth, channelHLColor, posArcBegin + arcOffset, posArc + arcOffset);

    canvas.Text(textRect.Center(), textSize, TextColorForBackground(settings.GetVec4f(theme, c_knobFillColor)), val.name.c_str(), nullptr, TEXT_ALIGN_MIDDLE | TEXT_ALIGN_CENTER);
    //canvas.Text()
    /*
    if (fCurrentVal > (fMax + std::numeric_limits<float>::epsilon()))
    {
        m_spCanvas->Arc(knobRegion.Center(), knobSize - channelWidth * .5f, channelWidth, channelHighColor, endArc - 10 + arcOffset, endArc + arcOffset);
    }
    else if (fCurrentVal < (fMin - std::numeric_limits<float>::epsilon()))
    {
        m_spCanvas->Arc(knobRegion.Center(), knobSize - channelWidth * .5f, channelWidth, channelHighColor, startArc + arcOffset, startArc + 10 + arcOffset);
    }
    */

    DrawTip(canvas, glm::vec2(knobRegion.Center().x, knobRegion.Top()), val);

    for (auto& child : GetLayout()->GetBackToFront())
    {
        child->Draw(canvas);
    }
}

void Knob::ClampNormalized(KnobValue& value)
{
    value.value = std::max(0.0f, value.value);
    value.value = std::min(1.0f, value.value);

    value.step = std::max(0.001f, value.step);
    value.step = std::min(1.0f, value.step);
}

Widget* Knob::MouseDown(CanvasInputState& input)
{
    if (input.buttonClicked[0])
    {
        m_startValue = m_value.value;
        Update(input);
        return this;
    }
    return nullptr;
}

void Knob::MouseUp(CanvasInputState& input)
{
}

void Knob::Update(CanvasInputState& input)
{
    KnobValue val;

    if (m_pCB)
    {
        m_pCB->UpdateKnob(this, KnobOp::Get, val);
    }
    else
    {
        UpdateKnob(this, KnobOp::Get, val);
    }

    ClampNormalized(val);

    auto dragDistanceWorld = 200.0f;

    val.value = m_startValue + (-input.worldDragDelta.y / dragDistanceWorld);

    // Quantize
    val.value = int(val.value / val.step) * val.step;

    ClampNormalized(val);

    if (m_pCB)
    {
        m_pCB->UpdateKnob(this, KnobOp::Set, val);
    }
    else
    {
        UpdateKnob(this, KnobOp::Set, val);
    }
}

bool Knob::MouseMove(CanvasInputState& input)
{
    // Only move top level
    if (input.m_pMouseCapture == this)
    {
        Update(input);
        return true;
    }
    return false;
}

void Knob::UpdateKnob(Knob* pKnob, KnobOp op, KnobValue& val)
{
    if (op == KnobOp::Set)
    {
        m_value = val;
    }
    else
    {
        m_value.valueText = fmt::format("{:1.2f}", m_value.value);
        val = m_value;
    }
}

}
