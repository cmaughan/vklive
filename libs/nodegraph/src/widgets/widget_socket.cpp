#include <algorithm>
#include <fmt/format.h>
#include <zest/logger/logger.h>
#include <nodegraph/canvas.h>
#include <nodegraph/theme.h>
#include <nodegraph/widgets/layout.h>
#include <nodegraph/widgets/widget_socket.h>

namespace NodeGraph {

Socket::Socket(const std::string& label, SocketType type, ISocketCB* pCB)
    : Widget(label)
    , m_pCB(pCB)
    , m_type(type)
{
    m_value.name = label;
    if (!m_pCB)
    {
        m_pCB = this;
    }
}

void Socket::Draw(Canvas& canvas)
{
    auto& settings = Zest::GlobalSettingsManager::Instance();
    auto theme = settings.GetCurrentTheme();

    Widget::Draw(canvas);

    auto rc = GetWorldRect();

    SocketValue val;
    m_pCB->UpdateSocket(this, SocketOp::Get, val);

    auto socketSize = rc.ShortSide() * 0.33f;
    auto socketRadius = 4.0f;
    auto innerSize = socketSize - socketRadius;

    // Socket surrounding shadow; a filled circle behind it
    canvas.FilledCircle(rc.Center(), socketSize, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    canvas.FilledCircle(rc.Center(), innerSize, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

    if (m_type == SocketType::Left)
    {
        auto start = rc.Center() - glm::vec2(socketSize, 0.0f);
        auto end = start - glm::vec2(socketRadius * 1.5f, 0.0f);

        canvas.DrawLine(start, end, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), socketRadius);
    }
    else
    {
        auto start = rc.Center() + glm::vec2(socketSize, 0.0f);
        auto end = start + glm::vec2(socketRadius * 1.5f, 0.0f);

        canvas.DrawLine(start, end, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), socketRadius);
    }

    auto color = settings.GetVec4f(theme, c_socketColor);

    if (val.flags & SocketFlags::ReadOnly)
    {
        color.w = .6f;
    }
    /*
    else if (hover || captured)
    {
        markColor = markHLColor;
        color = settings.Get(color_controlFillColorHL);
    }
    */

    for (auto& child : GetLayout()->GetBackToFront())
    {
        child->Draw(canvas);
    }
}

void Socket::ClampNormalized(SocketValue& value)
{
    value.value = std::max(0.0f, value.value);
    value.value = std::min(1.0f, value.value);

    value.step = std::max(0.001f, value.step);
    value.step = std::min(1.0f, value.step);
}

Widget* Socket::MouseDown(CanvasInputState& input)
{
    if (input.buttonClicked[0])
    {
        Update(input);
        return this;
    }
    return nullptr;
}

void Socket::MouseUp(CanvasInputState& input)
{
}

void Socket::Update(CanvasInputState& input)
{
    SocketValue val;
    m_pCB->UpdateSocket(this, SocketOp::Get, val);

    ClampNormalized(val);

    m_pCB->UpdateSocket(this, SocketOp::Set, val);
}

bool Socket::MouseMove(CanvasInputState& input)
{
    // Only move top level
    if (input.m_pMouseCapture == this)
    {
        Update(input);
        return true;
    }
    return false;
}

void Socket::UpdateSocket(Socket* pSocket, SocketOp op, SocketValue& val)
{
    if (op == SocketOp::Set)
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
