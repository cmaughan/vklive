#pragma once

#include <cassert>
#include <chrono>
#include <memory>
#include <vector>

#include <zest/math/math_utils.h>
#include <zest/time/timer.h>

#include <nodegraph/fonts.h>
#include <nodegraph/widgets/widget.h>

namespace NodeGraph {

enum class LineCap
{
    ROUND,
    BUTT
};

// Note: Matches NVG_ALIGN for convenience
enum TextAlign
{
    // Horizontal align
    TEXT_ALIGN_LEFT = 1 << 0, // Default, align text horizontally to left.
    TEXT_ALIGN_CENTER = 1 << 1, // Align text horizontally to center.
    TEXT_ALIGN_RIGHT = 1 << 2, // Align text horizontally to right.
                               // Vertical align
    TEXT_ALIGN_TOP = 1 << 3, // Align text vertically to top.
    TEXT_ALIGN_MIDDLE = 1 << 4, // Align text vertically to middle.
    TEXT_ALIGN_BOTTOM = 1 << 5, // Align text vertically to bottom.
    TEXT_ALIGN_BASELINE = 1 << 6, // Default, align text vertically to baseline.
};

enum class CaptureState
{
    None, // Mouse not captured
    MoveCanvas, // Moving Canvas
    MoveNode, // Moving a Node
    Parameter // Tweaking a parameter
};

// Represents the current interaction state with the canvas; used for
// tracking mouse manipulation
struct CanvasInputState
{
    glm::vec2 mousePos; // Mouse location, pixel coordinates
    glm::vec2 dragDelta; // Drag delta while mouse button down
    glm::vec2 mouseDelta; // Mouse move delta (without drag?)

    glm::vec2 worldMousePos; // Mouse location, pixel coordinates
    glm::vec2 worldDragDelta; // Drag delta while mouse button down
    glm::vec2 worldMoveDelta; // Mouse move delta (without drag?)

    glm::vec2 lastWorldMouseClick[MouseButtons::MOUSE_MAX]; // Mouse down in world coordinates

    // Button states
    bool buttonDown[MouseButtons::MOUSE_MAX];
    bool buttonClicked[MouseButtons::MOUSE_MAX];
    bool buttonReleased[MouseButtons::MOUSE_MAX];
    bool slowDrag = false; // Dragging slowly
    float wheelDelta;
    bool canCapture = false;
    CaptureState captureState = CaptureState::None;
    Widget* m_pMouseCapture = nullptr;
};

using Zest::NRectf;

class Canvas
{
public:
    Canvas(IFontTexture* pFontTexture, float worldScale = 1.0f, const glm::vec2& scaleLimits = glm::vec2(0.1f, 10.0f));
    virtual ~Canvas();

    // Conversions between pixel space and world space
    const glm::vec2 PixelToWorld(const glm::vec2& pixel) const;
    const glm::vec2 PixelSizeToWorldSize(const glm::vec2& pixel) const;
    const float PixelSizeToWorldSize(const float size) const;

    virtual glm::vec2 WorldToPixels(const glm::vec2& pos) const;
    virtual Zest::NRectf WorldToPixels(const Zest::NRectf& rc) const;
    virtual float WorldSizeToPixelSize(float size) const;
    virtual glm::vec2 WorldSizeToPixelSize(const glm::vec2& size) const;

    virtual float GetWorldScale() const;
    virtual void SetDevicePixelRatio(float ratio);
    virtual float GetDevicePixelRatio() const;
    virtual void SetWorldAtCenter(const glm::vec2& world);

    // Mouse state
    virtual glm::vec2 GetWorldMousePos() const;
    virtual void HandleMouse();
    CanvasInputState& GetInputState();

    // Pixel region
    void SetPixelRegionSize(const glm::vec2& sz);
    glm::vec2 GetPixelRegionSize() const;

    // Base class rendering
    virtual void DrawGrid(float worldStep);
    virtual void DrawCubicBezier(const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3, const glm::vec2& p4, const glm::vec4& color, float width = 1.0f);
    virtual void DrawLine(const glm::vec2& from, const glm::vec2& to, const glm::vec4& color, float width);

    // Does this implementation support varying gradients (imgui currently does not)
    virtual bool HasGradientVarying() const;

    // Drawing functions; These are all in world space, not pixel space
    void CubicBezier(std::vector<glm::vec2>& path, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, float tess_tol, int level);
    virtual void Begin(const glm::vec4& clearColor) = 0;
    virtual void End() = 0;
    virtual void FilledCircle(const glm::vec2& center, float radius, const glm::vec4& color) = 0;
    virtual void FilledGradientCircle(const glm::vec2& center, float radius, const NRectf& gradientRange, const glm::vec4& startColor, const glm::vec4& endColor) = 0;
    virtual void FillRoundedRect(const NRectf& rc, float radius, const glm::vec4& color) = 0;
    virtual void FillRect(const NRectf& rc, const glm::vec4& color) = 0;
    virtual void FillGradientRoundedRect(const NRectf& rc, float radius, const NRectf& gradientRange, const glm::vec4& startColor, const glm::vec4& endColor) = 0;
    virtual void FillGradientRoundedRectVarying(const NRectf& rc, const glm::vec4& radius, const NRectf& gradientRange, const glm::vec4& startColor, const glm::vec4& endColor) = 0;
    virtual void Stroke(const glm::vec2& from, const glm::vec2& to, float width, const glm::vec4& color) = 0;
    virtual void Arc(const glm::vec2& pos, float radius, float width, const glm::vec4& color, float startAngle, float endAngle) = 0;
    virtual void SetAA(bool set) = 0;
    virtual void BeginStroke(const glm::vec2& from, float width, const glm::vec4& color) = 0;
    virtual void BeginPath(const glm::vec2& from, const glm::vec4& color) = 0;
    virtual void MoveTo(const glm::vec2& to) = 0;
    virtual void LineTo(const glm::vec2& to) = 0;
    virtual void SetLineCap(LineCap cap) = 0;
    virtual void ClosePath() = 0;
    virtual void EndPath() = 0;
    virtual void EndStroke() = 0;
    virtual void Text(const glm::vec2& pos, float size, const glm::vec4& color, const char* pszText, const char* pszFace = nullptr, uint32_t align = TEXT_ALIGN_MIDDLE | TEXT_ALIGN_CENTER) = 0;
    virtual NRectf TextBounds(const glm::vec2& pos, float size, const char* pszText, const char* pszFace, uint32_t align = TEXT_ALIGN_MIDDLE | TEXT_ALIGN_CENTER) const = 0;
    virtual void TextBox(const glm::vec2& pos, float size, float breakWidth, const glm::vec4& color, const char* pszText, const char* pszFace = nullptr, uint32_t align = TEXT_ALIGN_MIDDLE | TEXT_ALIGN_CENTER) = 0;

    void HandleMouseDown(CanvasInputState& input);
    void HandleMouseUp(CanvasInputState& input);
    void HandleMouseMove(CanvasInputState& input);

    Layout* GetRootLayout() const;

    void Draw();

protected:
    glm::vec2 m_pixelSize; // Pixel size on screen of canvas
    glm::vec2 m_worldOrigin = glm::vec2(0.0f); // Origin of the world at the top left pixel
    float m_worldScale = 1.0f;
    glm::vec2 m_worldScaleLimits = glm::vec2(0.1f, 10.0f);
    CanvasInputState m_inputState;
    std::vector<glm::vec2> pointStorage;
    std::shared_ptr<FontContext> spFontContext;
    std::shared_ptr<Layout> m_spRootLayout;
};

} // namespace NodeGraph
