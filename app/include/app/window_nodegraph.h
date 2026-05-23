#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

struct IDevice;

namespace NodeGraph
{
class CanvasImGui;
class FrameNeutralFontTexture;
class Node;
} // namespace NodeGraph

namespace Zest
{
struct IFontTexture;
} // namespace Zest

class NodeGraphWindow
{
public:
    NodeGraphWindow();
    ~NodeGraphWindow();

    void Draw(bool* open, IDevice& device);

    bool HasCanvas() const;
    size_t NodeCountForTests() const;
    void BuildDemoGraphForTests();

private:
    void ensure_canvas(IDevice& device);
    void build_demo_graph();
    void draw_connections();

    std::unique_ptr<NodeGraph::FrameNeutralFontTexture> m_fontTexture;
    std::unique_ptr<NodeGraph::CanvasImGui> m_canvas;
    Zest::IFontTexture* m_backendFontTexture = nullptr;
    std::vector<std::shared_ptr<NodeGraph::Node>> m_demoNodes;
    bool m_graphBuilt = false;
};

void window_nodegraph(IDevice& device);
void window_nodegraph_shutdown();
