#include <cstdlib>
#include <cmath>
#include <iostream>
#include <map>
#include <string>

#include <config_app.h>

#include <nodegraph/canvas.h>
#include <nodegraph/canvas_imgui.h>
#include <nodegraph/font_texture_bridge.h>

#include <zest/file/runtree.h>
#include <zest/logger/logger.h>

namespace Zest
{
Logger logger{ false, LT::DBG };
bool Log::disabled = false;
} // namespace Zest

namespace
{

class FakeFontTexture : public Zest::IFontTexture
{
public:
    int UpdateTexture(int, int, int, int, int, const unsigned char*) override
    {
        updateCount++;
        return 1;
    }

    int CreateTexture(int w, int h, const unsigned char*) override
    {
        createCount++;
        const int id = nextId++;
        textures[id] = glm::ivec2(w, h);
        return id;
    }

    void DeleteTexture(int image) override
    {
        deleteCount++;
        textures.erase(image);
    }

    void GetTextureSize(int image, int* w, int* h) override
    {
        const auto itr = textures.find(image);
        const glm::ivec2 size = itr == textures.end() ? glm::ivec2(0) : itr->second;
        *w = size.x;
        *h = size.y;
    }

    void* GetTexture(int image) override
    {
        return image == 0 ? nullptr : reinterpret_cast<void*>(static_cast<intptr_t>(image));
    }

    void BeginFrame() override { beginFrameCount++; }
    void EndFrame() override { endFrameCount++; }

    int updateCount = 0;
    int createCount = 0;
    int deleteCount = 0;
    int beginFrameCount = 0;
    int endFrameCount = 0;

private:
    int nextId = 1;
    std::map<int, glm::ivec2> textures;
};

class TestCanvas : public NodeGraph::Canvas
{
public:
    explicit TestCanvas(Zest::IFontTexture* fontTexture)
        : Canvas(fontTexture, 1.0f, glm::vec2(0.1f, 20.0f))
    {
    }

    void Begin(const glm::vec4&) override {}
    void End() override {}
    void FilledCircle(const glm::vec2&, float, const glm::vec4&) override {}
    void FilledGradientCircle(const glm::vec2&, float, const Zest::NRectf&, const glm::vec4&, const glm::vec4&) override {}
    void FillRoundedRect(const Zest::NRectf&, float, const glm::vec4&) override {}
    void FillRect(const Zest::NRectf&, const glm::vec4&) override {}
    void FillGradientRoundedRect(const Zest::NRectf&, float, const Zest::NRectf&, const glm::vec4&, const glm::vec4&) override {}
    void FillGradientRoundedRectVarying(const Zest::NRectf&, const glm::vec4&, const Zest::NRectf&, const glm::vec4&, const glm::vec4&) override {}
    void Stroke(const glm::vec2&, const glm::vec2&, float, const glm::vec4&) override {}
    void Arc(const glm::vec2&, float, float, const glm::vec4&, float, float) override {}
    void SetAA(bool) override {}
    void BeginStroke(const glm::vec2&, float, const glm::vec4&) override {}
    void BeginPath(const glm::vec2&, const glm::vec4&) override {}
    void MoveTo(const glm::vec2&) override {}
    void LineTo(const glm::vec2&) override {}
    void SetLineCap(NodeGraph::LineCap) override {}
    void ClosePath() override {}
    void EndPath() override {}
    void EndStroke() override {}
    void Text(const glm::vec2&, float, const glm::vec4&, const char*, const char*, uint32_t) override {}
    Zest::NRectf TextBounds(const glm::vec2& pos, float size, const char*, const char*, uint32_t) const override
    {
        return Zest::NRectf(pos.x, pos.y, size, size);
    }
    void TextBox(const glm::vec2&, float, float, const glm::vec4&, const char*, const char*, uint32_t) override {}
};

bool require(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << message << "\n";
        return false;
    }
    return true;
}

} // namespace

int main()
{
    FakeFontTexture fontTexture;
    TestCanvas canvas(&fontTexture);
    canvas.SetPixelRegionSize(glm::vec2(800.0f, 600.0f));
    canvas.SetWorldAtCenter(glm::vec2(0.0f));

    auto& input = canvas.GetInputState();
    input.mousePos = glm::vec2(400.0f, 300.0f);
    input.worldMousePos = canvas.PixelToWorld(input.mousePos);
    input.wheelDelta = 1.0f;

    const float beforeScale = canvas.GetWorldScale();
    const glm::vec2 worldBefore = canvas.PixelToWorld(input.mousePos);

    canvas.HandleMouse();

    const float afterScale = canvas.GetWorldScale();
    const glm::vec2 worldAfter = canvas.PixelToWorld(input.mousePos);

    bool ok = true;
    ok &= require(afterScale > beforeScale, "mouse wheel should zoom the nodegraph canvas in");
    ok &= require(glm::length(worldAfter - worldBefore) < 0.001f, "zoom should keep the world point under the cursor stable");

    ok &= require(std::fabs(canvas.GetDevicePixelRatio() - 1.0f) < 0.001f, "nodegraph canvas should default to 1x device pixel ratio");
    canvas.SetDevicePixelRatio(1.75f);
    ok &= require(std::fabs(canvas.GetDevicePixelRatio() - 1.75f) < 0.001f, "nodegraph canvas should expose the device pixel ratio used by font rasterization");

    Zest::runtree_init(VKLIVE_ROOT, VKLIVE_ROOT);
    {
        FakeFontTexture textTexture;
        NodeGraph::CanvasImGui textCanvas(&textTexture, 2.0f, glm::vec2(0.1f, 20.0f), reinterpret_cast<ImFont*>(1));
        const auto bounds = textCanvas.TextBounds(glm::vec2(5.0f, 6.0f), 20.0f, "Scale", nullptr, NodeGraph::TEXT_ALIGN_LEFT | NodeGraph::TEXT_ALIGN_TOP);
        ok &= require(std::fabs(bounds.Height() - 20.0f) < 0.001f, "text bounds height should stay in world units when the canvas is zoomed");
    }
    Zest::runtree_destroy();

    FakeFontTexture sharedTexture;
    NodeGraph::FrameNeutralFontTexture neutralTexture(sharedTexture);
    const int textureId = neutralTexture.CreateTexture(16, 16, nullptr);
    neutralTexture.UpdateTexture(textureId, 0, 0, 16, 16, nullptr);
    neutralTexture.BeginFrame();
    neutralTexture.EndFrame();
    neutralTexture.DeleteTexture(textureId);

    ok &= require(sharedTexture.createCount == 1, "frame-neutral texture should forward texture creation");
    ok &= require(sharedTexture.updateCount == 1, "frame-neutral texture should forward texture updates");
    ok &= require(sharedTexture.deleteCount == 1, "frame-neutral texture should forward texture deletion");
    ok &= require(sharedTexture.beginFrameCount == 0, "embedded nodegraph should not begin the shared renderer font frame");
    ok &= require(sharedTexture.endFrameCount == 0, "embedded nodegraph should not end the shared renderer font frame");

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
