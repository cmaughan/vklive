#pragma once
#include <functional>
#include <nodegraph/widgets/widget.h>

namespace NodeGraph {

class Canvas;
class TextLabel;

class TextLabel : public Widget
{
public:
    TextLabel(const std::string& label, const std::string& font = "");
    virtual void Draw(Canvas& canvas) override;

private:
    NRectf m_sliderRangeArea;
    std::string m_font;
};

}
