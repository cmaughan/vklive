#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <functional>

#include <zest/math/math_utils.h>
#include <zest/time/timer.h>

#include <signals/signals.hpp>

namespace NodeGraph {

class Canvas;
struct CanvasInputState;
class TipTimer;

enum MouseButtons
{
    MOUSE_LEFT,
    MOUSE_RIGHT,
    MOUSE_MIDDLE,
    MOUSE_MAX
};

namespace LayoutConstraint {
enum
{
    Preferred = (1 << 0),
    Expanding = (1 << 1),
};
}

namespace WidgetFlags {
enum
{
    None = 0,
    DoNotLayout = (1 << 0)
};
};

namespace WidgetValueFlags {
enum
{
    None,
    ShowText,
    NoQuantization,
    Default = ShowText
};
};

struct WidgetValue
{
    std::string name;
    std::string valueText;
    std::string units = "Hz";
    float value = 0.0f;
    float step = 0.01f;
    uint32_t valueFlags = WidgetValueFlags::ShowText;
};

enum class TipState
{
    Wait,
    Ramp,
    On,
    Decay,
    Off
};

class Widget;
class TipTimer
{
public:
    static std::unordered_map<Widget*, TipTimer*> ActiveTips;

    TipTimer(Widget* pOwner, float wait, float secondsIn, float secondsOut)
        : m_in(secondsIn)
        , m_out(secondsOut)
        , m_wait(wait)
        , m_state(TipState::Off)
        , m_pOwner(pOwner)
    {
        m_decayTime = m_out;
    }

    ~TipTimer()
    {
        if (ActiveTips.find(m_pOwner) != ActiveTips.end())
        {
            ActiveTips.erase(m_pOwner);
        }
    }
    TipState Update()
    {
        // Wait or decay over time
        switch (m_state)
        {
        case TipState::Wait:
        {
            if (timer_get_elapsed_seconds(m_time) > m_wait)
            {
                SetState(TipState::Ramp);
            }
        }
        break;
        case TipState::Ramp:
        {
            if (timer_get_elapsed_seconds(m_time) > m_in)
            {
                SetState(TipState::On);
            }
        }
        break;
        case TipState::Decay:
        {
            if (timer_get_elapsed_seconds(m_time) > m_in)
            {
                SetState(TipState::Off);
            }
        }
        break;
        }
        return m_state;
    }

    float Alpha() const
    {
        auto elapsed = float(timer_get_elapsed_seconds(m_time));
        if (m_state == TipState::Decay)
        {
            if (elapsed > m_decayTime)
            {
                return 0.0f;
            }
            return ((m_decayTime - elapsed) / m_out);
        }
        else if (m_state == TipState::Ramp || m_state == TipState::On)
        {
            if (elapsed > m_in)
            {
                return 1.0f;
            }
            return 1.0f - ((m_in - elapsed) / m_in);
        }
        return 0.0f;
    }

    void Stop()
    {
        // If waiting, reverse to a decay for the remaining alpha
        if (m_state == TipState::Ramp)
        {
            auto a = Alpha();
            m_state = TipState::Decay;
            timer_restart(m_time);
            m_decayTime = m_out - (m_out * a);
        }
        else
        {
            // If on, switch to decay
            if (m_state == TipState::On || m_state == TipState::Decay)
            {
                SetState(TipState::Decay);
            }
            else
            {
                // Othwerise directly off
                SetState(TipState::Off);
            }
        }
    }

    void Start()
    {
        if (m_state != TipState::On && m_state != TipState::Ramp)
        {
            SetState(TipState::Wait);
        }
    }

    void SetState(TipState s)
    {
        if (s == m_state)
        {
            return;
        }
        m_state = s;
        m_decayTime = m_out;
        timer_restart(m_time);
        Update();

        if (s != TipState::Off)
        {
            ActiveTips[m_pOwner] = this;
        }
        else
        {
            ActiveTips.erase(m_pOwner);
        }
    }

    TipState GetState()
    {
        Update();
        return m_state;
    }

private:
    float m_wait = 0.5f;
    float m_in = 0.25f;
    float m_out = 0.25f;
    float m_decayTime = 0.0f;
    bool on = false;
    Zest::timer m_time;
    TipState m_state = TipState::Off;
    Widget* m_pOwner;
};

class Layout;
class Widget;
using WidgetList = std::vector<std::shared_ptr<Widget>>;

class Widget
{
public:
    Widget(const std::string& label);

    virtual void Draw(Canvas& canvas);
    virtual void PostDraw(Canvas& canvas, const Zest::NRectf& hintRect);

    virtual Widget* GetParent() const;
    virtual void SetParent(Widget* pParent);

    virtual const Zest::NRectf& GetRect() const;
    virtual void SetRect(const Zest::NRectf& sz);
    virtual const glm::uvec2& GetConstraints() const;
    virtual void SetConstraints(const glm::uvec2& constraints);
    virtual const glm::vec4& GetPadding() const;
    virtual void SetPadding(const glm::vec4& padding);

    virtual Widget* MouseDown(CanvasInputState& input);
    virtual void MouseUp(CanvasInputState& input);
    virtual bool MouseMove(CanvasInputState& input);
    virtual Widget* MouseHover(CanvasInputState& input);

    virtual Zest::NRectf ToWorldRect(const Zest::NRectf& rc) const;
    virtual Zest::NRectf ToLocalRect(const Zest::NRectf& rc) const;
    virtual Zest::NRectf GetWorldRect() const;

    virtual const std::string& GetLabel() const;
    virtual void SetLabel(const char* pszLabel);

    virtual void SetLayout(std::shared_ptr<Layout> spLayout);
    virtual Layout* GetLayout();

    virtual uint64_t GetFlags() const;
    virtual void SetFlags(uint64_t flags);

    // Min and max size for this widget
    virtual glm::vec4 GetMinMaxSize() const;

    // The size this widget would like to be.
    virtual glm::vec2 GetSizeHint() const;

    virtual Zest::NRectf GetRectWithPad() const;
    virtual void SetRectWithPad(const Zest::NRectf& rc);

    // Draw helpers
    Zest::NRectf DrawSlab(Canvas& canvas, const Zest::NRectf& rect, float borderRadius, float shadowSize, const glm::vec4& shadowColor, float borderSize, const glm::vec4& borderColor, const glm::vec4& centerColor, const char* pszText = nullptr, float fontPad = 2.0f, const glm::vec4& textColor = glm::vec4(1.0f), float fontSize = 0.0f, const char* pszFont = nullptr);

    virtual glm::vec4 TextColorForBackground(const glm::vec4& color);

    virtual void DrawTip(Canvas& canvas, const glm::vec2& widgetTopCenter, const WidgetValue& value);

    virtual bool IsMouseOver(Canvas& canvas);
    virtual bool IsMouseHover(Canvas& canvas);
    virtual bool IsMouseCapture(Canvas& canvas);

    TipTimer& GetTipTimer();

    void Visit(const std::function<void(Widget*)>& fnVisit);

    fteng::signal<void()> ValueUpdatedSignal;
    fteng::signal<void(Canvas& canvas, const Zest::NRectf& hintRect)> PostDrawSignal;

protected:
    Zest::NRectf m_rect;
    Widget* m_pParent = nullptr;
    std::string m_label;
    glm::uvec2 m_constraints = glm::uvec2(LayoutConstraint::Expanding, LayoutConstraint::Expanding);
    glm::vec4 m_padding = glm::vec4(0.0f);
    std::shared_ptr<Layout> m_spLayout;
    uint64_t m_flags = 0;
    glm::vec2 m_sizeHint = glm::vec2(0.0f);
    TipTimer m_tipTimer;
};

}
