#pragma once
#include <functional>
#include <nodegraph/widgets/widget.h>

namespace NodeGraph {

class Canvas;
class Socket;

enum class SocketOp
{
    Get,
    Set
};

enum class SocketType
{
    Left,
    Right
};

namespace SocketFlags {
enum
{
    None = 0,
    ReadOnly = (1)
};
}
struct SocketValue : WidgetValue
{
    uint32_t flags = SocketFlags::None;
};

struct ISocketCB
{
    virtual void UpdateSocket(Socket* pSocket, SocketOp op, SocketValue& val) = 0;
};

class Socket : public Widget, public ISocketCB
{
public:
    Socket(const std::string& label, SocketType type, ISocketCB* pCB = nullptr);
    virtual void Draw(Canvas& canvas) override;
    virtual Widget* MouseDown(CanvasInputState& input) override;
    virtual void MouseUp(CanvasInputState& input) override;
    virtual bool MouseMove(CanvasInputState& input) override;

    // ISocketCB
    virtual void UpdateSocket(Socket* pSlider, SocketOp op, SocketValue& val) override;

    // Internal
    virtual void ClampNormalized(SocketValue& value);
    virtual void Update(CanvasInputState& input);

private:
    ISocketCB* m_pCB = nullptr;
    SocketValue m_value;
    SocketType m_type = SocketType::Left;
};

}
