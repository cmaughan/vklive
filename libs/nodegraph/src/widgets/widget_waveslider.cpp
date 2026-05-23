#include <algorithm>

#include <zest/logger/logger.h>

#include <nodegraph/canvas.h>
#include <nodegraph/theme.h>
#include <nodegraph/widgets/layout.h>
#include <nodegraph/widgets/widget_waveslider.h>

namespace NodeGraph {

WaveSlider::WaveSlider(const std::string& label, const SliderValue& value)
    : Slider(label, value)
{
}

WaveSlider::WaveSlider(const std::string& label, std::shared_ptr<ISliderCB> pCB)
    : Slider(label, pCB)
{
}

void WaveSlider::PostDraw(Canvas& canvas, const NRectf& rc)
{
    auto sliderArea = ToWorldRect(rc);
    const float NumWaves = 4.0f;
    const float waveWidth = sliderArea.Width() / float(NumWaves);
    const float instep = 10.0f;
    auto types = std::vector<WaveType>{ WaveType::Triangle, WaveType::Square, WaveType::PWM, WaveType::Saw };

    SliderValue val;
    m_pCB->UpdateSlider(this, SliderOp::Get, val);

    canvas.SetLineCap(LineCap::ROUND);
    for (uint32_t index = 0; index < types.size(); index++)
    {
        auto waveType = types[index];

        for (int y = 0; y < 2; y++)
        {
            NRectf waveRect = sliderArea;
            waveRect.SetSize(glm::vec2(waveWidth, waveRect.Height()));
            waveRect.Adjust(waveWidth * index, 0, waveWidth * index, 0);
            waveRect.Adjust(instep, instep, -instep, -instep);

            float valIndex = val.value * (types.size() - 1);
            float diff = std::fabs(valIndex - float(index)); // *types.size();
            // diff /= types.size();
            diff = std::clamp(diff, 0.0f, 1.0f);
            diff = 1.0f - diff;

            float colorScale = std::clamp(diff, 0.0f, 1.0f);

            float width;
            glm::vec4 color;

            auto& settings = Zest::GlobalSettingsManager::Instance();
            auto theme = settings.GetCurrentTheme();
            auto thumbColor = settings.GetVec4f(theme, c_waveSliderCenterColor);
            auto waveColor = glm::vec4(thumbColor.x * colorScale, thumbColor.y * colorScale, thumbColor.z * colorScale, 1.0f);
            // Shadow
            if (y == 0)
            {
                width = 7.0f;
                color = settings.GetVec4f(theme, c_waveSliderBorderColor);
                // color = Zest::ColorForBackground(waveColor);
            }
            else
            {
                width = 6.0f;
                color = waveColor;
            }

            if (waveType == WaveType::Triangle)
            {
                waveRect.Adjust(0.0f, 0.0f, 0.0f, -4.0f);
                auto start = glm::vec2(waveRect.Left(), waveRect.Center().y);
                auto end = glm::vec2(waveRect.Right(), waveRect.Center().y);
                canvas.FilledCircle(start, width * .5f, color);
                canvas.BeginStroke(start, width, color);
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .25f, waveRect.Top()));
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .5f, waveRect.Bottom()));
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .75f, waveRect.Top()));
                canvas.LineTo(end);
                canvas.EndStroke();
                canvas.FilledCircle(end, width * .5f, color);
            }
            else if (waveType == WaveType::Square)
            {
                auto start = glm::vec2(waveRect.Left(), waveRect.Center().y);
                auto end = glm::vec2(waveRect.Left() + waveRect.Width(), waveRect.Center().y);
                canvas.FilledCircle(start, width * .5f, color);
                canvas.BeginStroke(start, width, color);
                canvas.LineTo(glm::vec2(waveRect.Left(), waveRect.Top()));
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .33f, waveRect.Top()));
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .33f, waveRect.Bottom()));
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .66f, waveRect.Bottom()));
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .66f, waveRect.Top()));
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width(), waveRect.Top()));
                canvas.LineTo(end);
                canvas.EndStroke();
                canvas.FilledCircle(end, width * .5f, color);
            }
            else if (waveType == WaveType::PWM)
            {
                auto start = glm::vec2(waveRect.Left() + waveRect.Width() * .1f, waveRect.Center().y);
                auto end = glm::vec2(waveRect.Left() + waveRect.Width() * .8f, waveRect.Center().y);
                canvas.FilledCircle(start, width * .5f, color);
                canvas.BeginStroke(start, width, color);
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .1f, waveRect.Top()));
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .3f, waveRect.Top()));
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .3f, waveRect.Bottom()));
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .6f, waveRect.Bottom()));
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .6f, waveRect.Top()));
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .8f, waveRect.Top()));
                canvas.LineTo(end);
                canvas.EndStroke();
                canvas.FilledCircle(end, width * .5f, color);
            }
            else if (waveType == WaveType::Saw)
            {
                waveRect.Adjust(0.0f, 0.0f, 0.0f, -4.0f);

                auto start = glm::vec2(waveRect.Left(), waveRect.Center().y);
                auto end = glm::vec2(waveRect.Left() + waveRect.Width(), waveRect.Center().y);
                canvas.FilledCircle(start, width * .5f, color);
                canvas.BeginStroke(start, width, color);
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .25f, waveRect.Top()));
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .25f, waveRect.Bottom()));
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .75f, waveRect.Top()));
                canvas.LineTo(glm::vec2(waveRect.Left() + waveRect.Width() * .75f, waveRect.Bottom()));
                canvas.LineTo(end);
                canvas.EndStroke();
                canvas.FilledCircle(end, width * .5f, color);
            }
        }
    }
    canvas.SetLineCap(LineCap::BUTT);
}

void WaveSlider::SetWave(const std::vector<float>& vals)
{
    m_wave = vals;
}

void WaveSlider::DrawGeneratedWave(Canvas& canvas, const NRectf& rc)
{
    /*
    std::vector<float> wave;
    wave.resize(1000);
    for (int i = 0; i < wave.size(); ++i)
    {
        wave[i] = std::sin(float((i * 8.0 * glm::pi<float>()) / wave.size()));
    }
    */

    auto rcWorld = ToWorldRect(rc);

    auto& settings = Zest::GlobalSettingsManager::Instance();
    auto theme = settings.GetCurrentTheme();

    // Draw the background area
    rcWorld = DrawSlab(canvas,
        rcWorld,
        settings.GetFloat(theme, s_sliderBorderRadius),
        settings.GetFloat(theme, s_sliderShadowSize),
        settings.GetVec4f(theme, c_sliderShadowColor),
        settings.GetFloat(theme, s_sliderBorderSize),
        settings.GetVec4f(theme, c_sliderBorderColor),
        settings.GetVec4f(theme, c_sliderCenterColor));

    auto waveColor = settings.GetVec4f(theme, c_sliderThumbColor);

    if (m_wave.empty())
    {
        return;
    }

    rcWorld.Adjust(8, 8, -8, -8);
    for (uint32_t x = 0; x < rcWorld.Width(); x++)
    {
        auto index = size_t(x * m_wave.size() / rc.Width());
        if (index >= m_wave.size())
        {
            continue;
        }
        auto y = m_wave[index];
        if (x == 0)
        {
            canvas.BeginStroke(glm::vec2(rcWorld.Left(), rcWorld.Center().y + y * rcWorld.Height() * 0.5f), 4.0f, waveColor);
        }
        else
        {
            canvas.LineTo(glm::vec2(rcWorld.Left() + x, rcWorld.Center().y + y * rcWorld.Height() * 0.5f));
        }
    }
    canvas.EndStroke();
}

} // Nodegraph
