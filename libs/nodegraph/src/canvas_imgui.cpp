#include <filesystem>

#include <zest/file/runtree.h>
#include <zest/math/math_utils.h>

#include <nodegraph/canvas_imgui.h>
#include <nodegraph/fonts.h>
#include <nodegraph/nodegraph.h>

namespace fs = std::filesystem;

namespace {
const int CircleSegments = 40;
const int ArcSegments = 40;
}

namespace NodeGraph {

CanvasImGui::CanvasImGui(IFontTexture* pFontTexture, float worldScale, const glm::vec2& scaleLimits, ImFont* pFont)
    : Canvas(pFontTexture, worldScale, scaleLimits)
    , m_pFont(pFont)
{
    if (m_pFont == nullptr)
    {
        m_pFont = ImGui::GetFont();
    }

    auto fapath2 = Zest::runtree_find_path("fonts/fa-solid-900.ttf");
    auto fontPath = Zest::runtree_find_path("fonts/Roboto-Regular.ttf");
    m_defaultFont = fonts_create(*spFontContext, "sans", fontPath.string().c_str());
    m_fontIcon = fonts_create(*spFontContext, "ficon", fapath2.string().c_str());
}

void CanvasImGui::Begin(const glm::vec4& clearColor)
{
    fonts_begin_frame(*spFontContext);

    origin = ImGui::GetCursorScreenPos();
    auto size = ImGui::GetContentRegionAvail();

    auto bottomRight = ImVec2(origin.x + size.x, origin.y + size.y);
    ImGui::GetWindowDrawList()->PushClipRect(origin, bottomRight);
    ImGui::GetWindowDrawList()->AddRectFilled(origin, bottomRight, ToImColor(clearColor));

}

void CanvasImGui::End()
{
    ImGui::GetWindowDrawList()->PopClipRect();

    fonts_end_frame(*spFontContext);
}

void CanvasImGui::FilledCircle(const glm::vec2& center, float radius, const glm::vec4& color)
{
    auto worldCenter = WorldToPixels(center);
    auto worldRadius = WorldSizeToPixelSize(radius);
    worldCenter += glm::vec2(origin);

    auto pDraw = ImGui::GetWindowDrawList();
    pDraw->AddCircleFilled(worldCenter, worldRadius, ToImColor(color), CircleSegments);
}

void CanvasImGui::FilledGradientCircle(const glm::vec2& center, float radius, const NRectf& gradientRange, const glm::vec4& startColor, const glm::vec4& endColor)
{
    auto worldCenter = WorldToPixels(center);
    auto worldRadius = WorldSizeToPixelSize(radius);
    // auto worldGradientBegin = WorldToPixels(gradientRange.topLeftPx);
    // auto worldGradientEnd = WorldToPixels(gradientRange.bottomRightPx);
    worldCenter += glm::vec2(origin);

    // TODO: Should be gradient but can't do it on ImGui yet
    auto pDraw = ImGui::GetWindowDrawList();
    pDraw->AddCircleFilled(worldCenter, worldRadius, ToImColor(startColor), CircleSegments);
}

void CanvasImGui::Stroke(const glm::vec2& from, const glm::vec2& to, float width, const glm::vec4& color)
{
    auto worldFrom = WorldToPixels(from);
    auto worldTo = WorldToPixels(to);
    auto worldWidth = WorldSizeToPixelSize(width);

    worldFrom += glm::vec2(origin);
    worldTo += glm::vec2(origin);

    auto pDraw = ImGui::GetWindowDrawList();
    pDraw->AddLine(worldFrom, worldTo, ToImColor(color), worldWidth);
}

void CanvasImGui::FillRoundedRect(const NRectf& rc, float radius, const glm::vec4& color)
{
    auto worldRect = WorldToPixels(rc);
    auto worldSize = WorldSizeToPixelSize(radius);
    worldRect.Adjust(origin.x, origin.y);

    auto pDraw = ImGui::GetWindowDrawList();
    pDraw->AddRectFilled(worldRect.topLeftPx, worldRect.bottomRightPx, ToImColor(color), worldSize);
}

void CanvasImGui::FillGradientRoundedRect(const NRectf& rc, float radius, const NRectf& gradientRange, const glm::vec4& startColor, const glm::vec4& endColor)
{
    auto worldRect = WorldToPixels(rc);
    auto worldSize = WorldSizeToPixelSize(radius);
    // auto worldGradientBegin = WorldToPixels(gradientRange.topLeftPx);
    // auto worldGradientEnd = WorldToPixels(gradientRange.bottomRightPx);
    worldRect.Adjust(origin.x, origin.y);

    auto pDraw = ImGui::GetWindowDrawList();
    // pDraw->AddRectFilledMultiColor(worldRect.topLeftPx, worldRect.bottomRightPx, ToImColor(startColor), ToImColor(startColor), ToImColor(startColor), ToImColor(endColor));
    pDraw->AddRectFilled(worldRect.topLeftPx, worldRect.bottomRightPx, ToImColor(startColor), worldSize);
}

void CanvasImGui::FillGradientRoundedRectVarying(const NRectf& rc, const glm::vec4& radius, const NRectf& gradientRange, const glm::vec4& startColor, const glm::vec4& endColor)
{
    auto worldRect = WorldToPixels(rc);
    float worldSize0 = 0.0f;
    // auto worldGradientBegin = WorldToPixels(gradientRange.topLeftPx);
    // auto worldGradientEnd = WorldToPixels(gradientRange.bottomRightPx);

    auto flags = 0;
    if (radius.x > 0.0f)
    {
        flags |= ImDrawFlags_RoundCornersTopLeft;
        worldSize0 = WorldSizeToPixelSize(radius.x);
    }
    if (radius.y > 0.0f)
    {
        flags |= ImDrawFlags_RoundCornersTopRight;
        worldSize0 = WorldSizeToPixelSize(radius.y);
    }
    if (radius.z > 0.0f)
    {
        flags |= ImDrawFlags_RoundCornersBottomRight;
        worldSize0 = WorldSizeToPixelSize(radius.z);
    }
    if (radius.w > 0.0f)
    {
        flags |= ImDrawFlags_RoundCornersBottomLeft;
        worldSize0 = WorldSizeToPixelSize(radius.w);
    }

    worldRect.Adjust(origin.x, origin.y);

    auto pDraw = ImGui::GetWindowDrawList();
    pDraw->AddRectFilled(worldRect.topLeftPx, worldRect.bottomRightPx, ToImColor(startColor), worldSize0, flags);
}

void CanvasImGui::FillRect(const NRectf& rc, const glm::vec4& color)
{
    auto worldRect = WorldToPixels(rc);
    auto pDraw = ImGui::GetWindowDrawList();

    worldRect.Adjust(origin.x, origin.y);
    pDraw->AddRectFilled(worldRect.topLeftPx, worldRect.bottomRightPx, ToImColor(color));
}

NRectf CanvasImGui::TextBounds(const glm::vec2& pos, float size, const char* pszText, const char* pszFace, uint32_t align) const
{
    auto worldPos = WorldToPixels(pos);
    worldPos += glm::vec2(origin);

    if (pszFace != nullptr)
    {
        fonts_set_face(*spFontContext, pszFace);
    }
    else
    {
        fonts_set_face(*spFontContext, m_defaultFont);
    }

    fonts_set_size(*spFontContext, size);
    fonts_set_align(*spFontContext, align);
    fonts_set_scale(*spFontContext, m_worldScale);
    auto width = fonts_text_bounds(*spFontContext, worldPos.x / m_worldScale, worldPos.y / m_worldScale, pszText, nullptr, nullptr);

    // Return everything in World space, since we scale every draw call
    return NRectf(pos.x, pos.y, PixelSizeToWorldSize(width) * m_worldScale, size);
}

void CanvasImGui::Text(const glm::vec2& pos, float size, const glm::vec4& color, const char* pszText, const char* pszFace, uint32_t align)
{
    auto worldPos = WorldToPixels(pos);
    worldPos += glm::vec2(origin);

    if (pszFace != nullptr)
    {
        fonts_set_face(*spFontContext, pszFace);
    }
    else
    {
        fonts_set_face(*spFontContext, m_defaultFont);
    }

    auto packedColor = glm::packUnorm4x8(color);

    fonts_set_size(*spFontContext, size);
    fonts_set_align(*spFontContext, align);
    fonts_set_scale(*spFontContext, m_worldScale);
    fonts_draw_text(*spFontContext, worldPos.x / m_worldScale, worldPos.y / m_worldScale, packedColor, pszText, nullptr);
}

void CanvasImGui::TextBox(const glm::vec2& pos, float size, float breakWidth, const glm::vec4& color, const char* pszText, const char* pszFace, uint32_t align)
{
    auto worldPos = WorldToPixels(pos);
    worldPos += glm::vec2(origin);

    if (pszFace != nullptr)
    {
        fonts_set_face(*spFontContext, pszFace);
    }
    else
    {
        fonts_set_face(*spFontContext, m_defaultFont);
    }

    fonts_set_size(*spFontContext, size);
    fonts_set_align(*spFontContext, align);
    fonts_set_scale(*spFontContext, m_worldScale);
    fonts_text_box(*spFontContext, worldPos.x / m_worldScale, worldPos.y / m_worldScale, breakWidth, glm::packUnorm4x8(color), pszText, nullptr);
}

void CanvasImGui::Arc(const glm::vec2& pos, float radius, float width, const glm::vec4& color, float startAngle, float endAngle)
{
    auto worldRadius = WorldSizeToPixelSize(radius);
    auto worldPos = WorldToPixels(pos);
    auto worldWidth = WorldSizeToPixelSize(width);

    worldPos += glm::vec2(origin);

    auto pDraw = ImGui::GetWindowDrawList();
    pDraw->PathClear();
    pDraw->PathArcTo(worldPos, worldRadius, Zest::degToRad(startAngle), Zest::degToRad(endAngle), ArcSegments);
    pDraw->PathStroke(ToImColor(color), false, worldWidth);
}

void CanvasImGui::SetAA(bool set)
{
    /* Nothing currently */
    M_UNUSED(set);
}

void CanvasImGui::BeginStroke(const glm::vec2& from, float width, const glm::vec4& color)
{
    auto worldPos = WorldToPixels(from);
    auto size = WorldSizeToPixelSize(width);

    worldPos += glm::vec2(origin);

    auto pDraw = ImGui::GetWindowDrawList();
    pDraw->PathClear();
    pDraw->PathLineTo(worldPos);

    m_pathWidth = size;
    m_pathColor = ToImColor(color);
    m_closePath = false;
}

void CanvasImGui::BeginPath(const glm::vec2& from, const glm::vec4& color)
{
    auto worldPos = WorldToPixels(from);
    worldPos += glm::vec2(origin);

    auto pDraw = ImGui::GetWindowDrawList();
    pDraw->PathClear();
    pDraw->PathLineTo(worldPos);
    m_pathColor = ToImColor(color);
    m_closePath = false;
}

void CanvasImGui::LineTo(const glm::vec2& to)
{
    auto worldPos = WorldToPixels(to);
    worldPos += glm::vec2(origin);

    auto pDraw = ImGui::GetWindowDrawList();
    pDraw->PathLineTo(worldPos);
}

void CanvasImGui::MoveTo(const glm::vec2& to)
{
    auto worldPos = WorldToPixels(to);
    worldPos += glm::vec2(origin);

    auto pDraw = ImGui::GetWindowDrawList();
    pDraw->PathClear();
    pDraw->PathLineTo(worldPos);
}

void CanvasImGui::EndStroke()
{
    auto pDraw = ImGui::GetWindowDrawList();
    pDraw->PathStroke(m_pathColor, m_closePath, m_pathWidth);
}

void CanvasImGui::EndPath()
{
    auto pDraw = ImGui::GetWindowDrawList();
    pDraw->PathFillConvex(m_pathColor);
}

void CanvasImGui::SetLineCap(LineCap cap)
{
    /*
    if (cap == LineCap::BUTT)
    {
        nvgLineCap(vg, NImGui_BUTT);
        nvgLineJoin(vg, NImGui_BUTT);
    }
    else
    {
        nvgLineCap(vg, NImGui_ROUND);
        nvgLineJoin(vg, NImGui_ROUND);
    }
    */
}

void CanvasImGui::ClosePath()
{
    m_closePath = true;
}

} // namespace NodeGraph
