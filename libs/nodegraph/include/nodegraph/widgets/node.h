#pragma once
#include <nodegraph/widgets/widget.h>

namespace NodeGraph {

class Canvas;
class Node : public Widget
{
public:
    Node(const std::string& label);
    virtual void Draw(Canvas& canvas) override;
    virtual Widget* MouseDown(CanvasInputState& input) override;
    virtual void MouseUp(CanvasInputState& input) override;
    virtual bool MouseMove(CanvasInputState& input) override;

protected:
    enum class MoveType
    {
        Move,
        Resize
    };
    MoveType m_moveType = MoveType::Move;
};

}
