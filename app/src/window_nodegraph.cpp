#include <app/window_nodegraph.h>

#include <algorithm>
#include <cstdint>
#include <map>

#include <app/menu.h>
#include <app/nodegraph_theme.h>

#include <nodegraph/canvas_imgui.h>
#include <nodegraph/font_texture_bridge.h>
#include <nodegraph/widgets/layout.h>
#include <nodegraph/widgets/node.h>
#include <nodegraph/widgets/widget_knob.h>
#include <nodegraph/widgets/widget_label.h>
#include <nodegraph/widgets/widget_slider.h>
#include <nodegraph/widgets/widget_socket.h>

#include <vklive/IDevice.h>

#include <zest/imgui/imgui.h>

namespace
{

class NullFontTexture : public Zest::IFontTexture
{
public:
    int UpdateTexture(int image, int, int, int, int, const unsigned char*) override
    {
        return image;
    }

    int CreateTexture(int w, int h, const unsigned char*) override
    {
        const int id = m_nextId++;
        m_textures[id] = glm::ivec2(w, h);
        return id;
    }

    void DeleteTexture(int image) override
    {
        m_textures.erase(image);
    }

    void GetTextureSize(int image, int* w, int* h) override
    {
        const auto itr = m_textures.find(image);
        const glm::ivec2 size = itr == m_textures.end() ? glm::ivec2(0) : itr->second;
        *w = size.x;
        *h = size.y;
    }

    void* GetTexture(int image) override
    {
        return image == 0 ? nullptr : reinterpret_cast<void*>(static_cast<intptr_t>(image));
    }

    void BeginFrame() override {}
    void EndFrame() override {}

private:
    int m_nextId = 1;
    std::map<int, glm::ivec2> m_textures;
};

std::shared_ptr<NodeGraph::Slider> make_slider(const std::string& label, float value, NodeGraph::SliderType type = NodeGraph::SliderType::Magnitude)
{
    NodeGraph::SliderValue sliderValue;
    sliderValue.name = label;
    sliderValue.value = value;
    sliderValue.step = 0.05f;
    sliderValue.type = type;

    auto slider = std::make_shared<NodeGraph::Slider>(label, sliderValue);
    slider->SetRect(Zest::NRectf(0.0f, 0.0f, 230.0f, 34.0f));
    slider->SetConstraints(glm::uvec2(NodeGraph::LayoutConstraint::Expanding, NodeGraph::LayoutConstraint::Preferred));
    return slider;
}

std::shared_ptr<NodeGraph::Socket> make_socket(const std::string& label, NodeGraph::SocketType type)
{
    auto socket = std::make_shared<NodeGraph::Socket>(label, type);
    socket->SetRect(Zest::NRectf(0.0f, 0.0f, 230.0f, 28.0f));
    socket->SetConstraints(glm::uvec2(NodeGraph::LayoutConstraint::Expanding, NodeGraph::LayoutConstraint::Preferred));
    return socket;
}

std::shared_ptr<NodeGraph::Knob> make_knob(const std::string& label)
{
    auto knob = std::make_shared<NodeGraph::Knob>(label);
    knob->SetRect(Zest::NRectf(0.0f, 0.0f, 74.0f, 86.0f));
    knob->SetConstraints(glm::uvec2(NodeGraph::LayoutConstraint::Preferred, NodeGraph::LayoutConstraint::Preferred));
    return knob;
}

} // namespace

NodeGraphWindow::NodeGraphWindow() = default;
NodeGraphWindow::~NodeGraphWindow() = default;

void NodeGraphWindow::ensure_canvas(IDevice& device)
{
    auto* fontTexture = device.FontTexture();
    if (!fontTexture)
    {
        return;
    }

    if (m_canvas && m_backendFontTexture == fontTexture)
    {
        return;
    }

    m_demoNodes.clear();
    m_graphBuilt = false;
    m_canvas.reset();
    m_fontTexture = std::make_unique<NodeGraph::FrameNeutralFontTexture>(*fontTexture);
    m_backendFontTexture = fontTexture;

    m_canvas = std::make_unique<NodeGraph::CanvasImGui>(m_fontTexture.get(), 1.0f, glm::vec2(0.15f, 6.0f), ImGui::GetFont());
    m_canvas->SetDevicePixelRatio(device.Context().vdpi);
    m_canvas->SetPixelRegionSize(glm::vec2(960.0f, 540.0f));
    m_canvas->SetWorldAtCenter(glm::vec2(0.0f));
}

void NodeGraphWindow::build_demo_graph()
{
    if (!m_canvas || m_graphBuilt)
    {
        return;
    }

    nodegraph_seed_default_theme();

    auto source = std::make_shared<NodeGraph::Node>("Oscillator");
    source->SetRect(Zest::NRectf(-390.0f, -160.0f, 285.0f, 238.0f));
    source->GetLayout()->SetContentsMargins(glm::vec4(12.0f, 12.0f, 12.0f, 12.0f));
    source->GetLayout()->SetSpacing(10.0f);
    source->GetLayout()->AddChild(make_socket("Audio Out", NodeGraph::SocketType::Right));
    source->GetLayout()->AddChild(make_slider("Pitch", 0.62f));
    source->GetLayout()->AddChild(make_slider("Shape", 0.38f, NodeGraph::SliderType::Mark));

    auto filter = std::make_shared<NodeGraph::Node>("Filter");
    filter->SetRect(Zest::NRectf(-45.0f, -130.0f, 300.0f, 246.0f));
    filter->GetLayout()->SetContentsMargins(glm::vec4(12.0f, 12.0f, 12.0f, 12.0f));
    filter->GetLayout()->SetSpacing(10.0f);
    filter->GetLayout()->AddChild(make_socket("Audio In", NodeGraph::SocketType::Left));
    filter->GetLayout()->AddChild(make_slider("Cutoff", 0.74f));
    filter->GetLayout()->AddChild(make_slider("Resonance", 0.29f, NodeGraph::SliderType::Mark));

    auto output = std::make_shared<NodeGraph::Node>("Output");
    output->SetRect(Zest::NRectf(330.0f, -105.0f, 230.0f, 210.0f));
    output->GetLayout()->SetContentsMargins(glm::vec4(12.0f, 12.0f, 12.0f, 12.0f));
    output->GetLayout()->SetSpacing(10.0f);
    output->GetLayout()->AddChild(make_socket("Master In", NodeGraph::SocketType::Left));
    output->GetLayout()->AddChild(make_knob("Level"));

    auto* root = m_canvas->GetRootLayout();
    root->AddChild(source);
    root->AddChild(filter);
    root->AddChild(output);

    m_demoNodes = { source, filter, output };
    m_graphBuilt = true;
}

void NodeGraphWindow::draw_connections()
{
    if (!m_canvas || m_demoNodes.size() < 3)
    {
        return;
    }

    const auto sourceRect = m_demoNodes[0]->GetWorldRect();
    const auto filterRect = m_demoNodes[1]->GetWorldRect();
    const auto outputRect = m_demoNodes[2]->GetWorldRect();

    auto draw_connection = [&](const Zest::NRectf& fromRect, const Zest::NRectf& toRect, float yOffset) {
        const glm::vec2 from(fromRect.Right(), fromRect.Top() + yOffset);
        const glm::vec2 to(toRect.Left(), toRect.Top() + yOffset);
        m_canvas->DrawCubicBezier(from, from + glm::vec2(85.0f, 0.0f), to - glm::vec2(85.0f, 0.0f), to, glm::vec4(0.72f, 0.72f, 0.72f, 1.0f), 3.0f);
    };

    draw_connection(sourceRect, filterRect, 72.0f);
    draw_connection(filterRect, outputRect, 72.0f);
}

void NodeGraphWindow::Draw(bool* open, IDevice& device)
{
    if (!open || !*open)
    {
        return;
    }

    ensure_canvas(device);

    if (ImGui::Begin("Node Graph", open))
    {
        if (!m_canvas)
        {
            ImGui::TextUnformatted("Node graph canvas unavailable: font texture is not initialized.");
        }
        else
        {
            build_demo_graph();

            if (ImGui::BeginChild("##NodeGraphCanvas", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
            {
                const ImVec2 available = ImGui::GetContentRegionAvail();
                const glm::vec2 canvasSize(std::max(available.x, 1.0f), std::max(available.y, 1.0f));
                m_canvas->SetPixelRegionSize(canvasSize);

                const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                NodeGraph::canvas_imgui_update_state(*m_canvas, canvasSize, hovered);
                m_canvas->HandleMouse();

                m_canvas->Begin(glm::vec4(0.08f, 0.08f, 0.085f, 1.0f));
                m_canvas->DrawGrid(50.0f);
                draw_connections();
                m_canvas->Draw();
                m_canvas->End();
            }
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

bool NodeGraphWindow::HasCanvas() const
{
    return m_canvas != nullptr;
}

size_t NodeGraphWindow::NodeCountForTests() const
{
    return m_demoNodes.size();
}

void NodeGraphWindow::BuildDemoGraphForTests()
{
    static NullFontTexture fontTexture;
    static NodeGraph::FrameNeutralFontTexture neutralFontTexture(fontTexture);

    if (!m_canvas)
    {
        m_canvas = std::make_unique<NodeGraph::CanvasImGui>(&neutralFontTexture, 1.0f, glm::vec2(0.15f, 6.0f), reinterpret_cast<ImFont*>(1));
        m_canvas->SetPixelRegionSize(glm::vec2(960.0f, 540.0f));
        m_canvas->SetWorldAtCenter(glm::vec2(0.0f));
    }
    build_demo_graph();
}

namespace
{

std::unique_ptr<NodeGraphWindow>& nodegraph_window_instance()
{
    static std::unique_ptr<NodeGraphWindow> window;
    return window;
}

} // namespace

void window_nodegraph(IDevice& device)
{
    auto& window = nodegraph_window_instance();
    if (!window)
    {
        window = std::make_unique<NodeGraphWindow>();
    }
    window->Draw(&g_WindowEnables.nodeGraph, device);
}

void window_nodegraph_shutdown()
{
    nodegraph_window_instance().reset();
}
