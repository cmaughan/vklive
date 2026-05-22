#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <set>
#include <string>

#include <app/project.h>

#include <vklive/IDevice.h>
#include <vklive/scene.h>

#include <zest/file/runtree.h>
#include <zest/logger/logger.h>

namespace fs = std::filesystem;

namespace Zest
{
Logger logger{ false, LT::DBG };
bool Log::disabled = false;
}

namespace
{

class FakeDevice : public IDevice
{
public:
    explicit FakeDevice(std::set<std::string> shaderExtensions)
        : shaderExtensions(std::move(shaderExtensions))
    {
    }

    void InitScene(Scene&) override {}
    void DestroyScene(Scene&) override {}
    void ImGui_Render(ImDrawData*) override {}
    RenderOutput Render_3D(Scene&, const glm::vec2&) override { return {}; }
    void WriteToFile(Scene&, const fs::path&) override {}
    void WaitIdle() override {}
    void ValidateSwapChain() override {}
    bool Present() override { return true; }
    std::string GetDeviceString() const override { return "FakeDevice"; }
    std::set<std::string> ShaderFileExtensions() override { return shaderExtensions; }
    RenderBackend Backend() const override { return RenderBackend::Vulkan; }
    std::vector<RenderTargetView> TargetViews(Scene&) override { return {}; }
    DeviceContext& Context() override { return context; }

private:
    std::set<std::string> shaderExtensions;
    DeviceContext context;
};

FakeDevice* activeDevice = nullptr;

bool require(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << message << "\n";
        return false;
    }
    return true;
}

std::string read_text_file(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return {};
    }

    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

} // namespace

IDevice* GetDevice()
{
    return activeDevice;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: vklive_project_copy_tests <source-root>\n";
        return EXIT_FAILURE;
    }

    bool ok = true;
    const fs::path sourceRoot = argv[1];
    const fs::path rayProjectRoot = sourceRoot / "run_tree/projects/ray_tracer";
    const std::string metalRayShader = read_text_file(rayProjectRoot / "rt_trace.metal");
    Zest::runtree_init(sourceRoot.string().c_str(), sourceRoot.string().c_str());

    ok &= require(!metalRayShader.empty(), "Metal ray template shader should be readable");
    ok &= require(metalRayShader.find("const float2 ndc = uv * 2.0f - 1.0f;") != std::string::npos, "Metal ray template should use Vulkan-parity unflipped NDC coordinates");
    ok &= require(metalRayShader.find("(1.0f - uv.y)") == std::string::npos, "Metal ray template should not invert uv.y before writing to the renderer target");

    FakeDevice vulkanDevice({
        ".fs",
        ".gs",
        ".vs",
        ".frag",
        ".geom",
        ".vert",
        ".rchit",
        ".rgen",
        ".rmiss",
    });
    activeDevice = &vulkanDevice;

    auto scene = scene_build(rayProjectRoot);
    ok &= require(scene && scene->valid, "ray tracing template should parse before copy test");

    Project project;
    project.rootPath = fs::canonical(rayProjectRoot);
    project.spScene = scene;

    const fs::path copyRoot = fs::temp_directory_path() / "vklive_project_copy_tests";
    std::error_code removeError;
    fs::remove_all(copyRoot, removeError);
    fs::create_directories(copyRoot);

    std::string error;
    ok &= require(project_copy(project, copyRoot, error), "project_copy failed: " + error);
    ok &= require(fs::exists(copyRoot / "rt_trace.metal"), "Vulkan-side project copy should preserve native Metal ray shader");
    ok &= require(fs::exists(copyRoot / "rt_gen.rgen"), "project copy should preserve ray generation shader");
    ok &= require(fs::exists(copyRoot / "rt_miss.rmiss"), "project copy should preserve ray miss shader");
    ok &= require(fs::exists(copyRoot / "rt_closest.rchit"), "project copy should preserve ray closest-hit shader");
    ok &= require(fs::exists(copyRoot / "cornell-box.obj"), "ray tracing template copy should preserve its model asset");
    ok &= require(fs::exists(copyRoot / "cornell-box.mtl"), "ray tracing template copy should preserve its material library");

    fs::remove_all(copyRoot, removeError);
    Zest::runtree_destroy();
    scene_destroy_parser();
    activeDevice = nullptr;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
