#pragma once
#include <nodegraph/widgets/widget_slider.h>

namespace NodeGraph {

class WaveSlider : public Slider
{
public:
    enum class WaveType
    {
        Triangle,
        Square,
        PWM,
        Saw
    };

    WaveSlider(const std::string& label, std::shared_ptr<ISliderCB> pCB = nullptr);
    WaveSlider(const std::string& label, const SliderValue& value);
    virtual void PostDraw(Canvas& canvas, const NRectf& rc);
    virtual void DrawGeneratedWave(Canvas& canvas, const NRectf& rc);

    void SetWave(const std::vector<float>& vals);

private:
    std::vector<float> m_wave;
};

}
