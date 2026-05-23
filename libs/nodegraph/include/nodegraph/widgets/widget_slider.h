#pragma once
#include <functional>
#include <nodegraph/widgets/widget.h>

namespace NodeGraph {

class Canvas;
class Slider;

enum class SliderOp
{
    Get,
    Set
};

enum class SliderType
{
    Mark,
    Magnitude
};

struct SliderValue : WidgetValue
{
    SliderType type = SliderType::Magnitude;
};

struct ISliderCB
{
    virtual void UpdateSlider(Slider* pSlider, SliderOp op, SliderValue& val) = 0;
};

struct DefaultSliderCB : public ISliderCB
{
    SliderValue myVal;
    DefaultSliderCB();
    DefaultSliderCB(const SliderValue& value);
    virtual void UpdateSlider(Slider* pSlider, SliderOp op, SliderValue& val) override;
};

class Slider : public Widget
{
public:
    Slider(const std::string& label, const SliderValue& value);
    Slider(const std::string& label, std::shared_ptr<ISliderCB> pCB = nullptr);
    virtual void Draw(Canvas& canvas) override;
    virtual Widget* MouseDown(CanvasInputState& input) override;
    virtual void MouseUp(CanvasInputState& input) override;
    virtual bool MouseMove(CanvasInputState& input) override;

    // Internal
    virtual void ClampNormalized(SliderValue& value);
    virtual void Update(CanvasInputState& input);
    virtual float ThumbWorldSize(Canvas& canvas, float width) const;

    virtual const NRectf& GetSliderRangeArea() const;

    ISliderCB* GetCB() const { return m_pCB.get(); }
protected:
    std::shared_ptr<ISliderCB> m_pCB;
    NRectf m_sliderRangeArea;
};

}
