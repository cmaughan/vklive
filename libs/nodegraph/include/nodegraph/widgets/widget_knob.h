#pragma once
#include <functional>
#include <nodegraph/widgets/widget.h>

namespace NodeGraph {

class Canvas;
class Knob;

enum class KnobOp
{
    Get,
    Set
};

enum class KnobType
{
    Standard,
    Mini
};

namespace KnobFlags {
enum
{
    None = 0,
    ReadOnly = (1)
};
}
struct KnobValue : WidgetValue
{
    uint32_t flags = KnobFlags::None;
    KnobType type = KnobType::Standard;
};

struct IKnobCB
{
    virtual void UpdateKnob(Knob* pKnob, KnobOp op, KnobValue& val) = 0;
};

class Knob : public Widget, public IKnobCB
{
public:
    Knob(const std::string& label, IKnobCB* pCB = nullptr);
    virtual void Draw(Canvas& canvas) override;
    virtual Widget* MouseDown(CanvasInputState& input) override;
    virtual void MouseUp(CanvasInputState& input) override;
    virtual bool MouseMove(CanvasInputState& input) override;

    // IKnobCB
    virtual void UpdateKnob(Knob* pSlider, KnobOp op, KnobValue& val) override;

    // Internal
    virtual void ClampNormalized(KnobValue& value);
    virtual void Update(CanvasInputState& input);
    // virtual float ThumbWorldSize(Canvas& canvas, float width) const;

private:
    IKnobCB* m_pCB = nullptr;
    bool m_mini = false;
    KnobValue m_value;
    float m_startValue;
};

}
