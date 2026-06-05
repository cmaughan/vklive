#include <app/editor_nvim_session.h>

#include <cassert>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace
{

class RecordingNvimHost final : public INvimHostAdapter
{
public:
    bool start(const vklive_nvim::NvimHostOptions& options) override
    {
        started = true;
        ++startCount;
        lastOptions = options;
        return true;
    }

    void stop() override
    {
        started = false;
    }

    bool running() const override
    {
        return started;
    }

    void resize(int columns, int rows) override
    {
        lastColumns = columns;
        lastRows = rows;
        ++resizeCount;
    }

    void open_project_files(const vklive_nvim::NvimProjectFiles& project) override
    {
        lastProject = project;
        ++openCount;
    }

    void pump() override
    {
        ++pumpCount;
    }

    void send_input(std::string_view input) override
    {
        sentInputs.emplace_back(input);
    }

    const vklive_nvim::RenderModel& render_model() const override
    {
        return model;
    }

    const vklive_nvim::HighlightTable& highlights() const override
    {
        return highlightTable;
    }

    bool started = false;
    int startCount = 0;
    int resizeCount = 0;
    int openCount = 0;
    int pumpCount = 0;
    int lastColumns = 0;
    int lastRows = 0;
    vklive_nvim::NvimHostOptions lastOptions;
    vklive_nvim::NvimProjectFiles lastProject;
    std::vector<std::string> sentInputs;
    vklive_nvim::RenderModel model;
    vklive_nvim::HighlightTable highlightTable;
};

} // namespace

int main()
{
    auto host = std::make_unique<RecordingNvimHost>();
    auto* hostPtr = host.get();

    NvimEditorSession session(std::move(host));
    session.set_project_root("D:/projects/demo");
    session.set_files({ "D:/projects/demo/main.scenegraph", "D:/projects/demo/shaders/a.frag" }, true);

    assert(session.ensure_started(100, 40));
    assert(hostPtr->startCount == 1);
    assert(hostPtr->lastOptions.project_root == std::filesystem::path("D:/projects/demo"));
    assert(hostPtr->lastOptions.columns == 100);
    assert(hostPtr->lastOptions.rows == 40);
    assert(hostPtr->openCount == 1);
    assert(hostPtr->lastProject.files.size() == 2);
    assert(hostPtr->lastProject.files[0] == std::filesystem::path("D:/projects/demo/main.scenegraph"));
    assert(hostPtr->lastProject.files[1] == std::filesystem::path("D:/projects/demo/shaders/a.frag"));

    assert(session.ensure_started(120, 50));
    assert(hostPtr->startCount == 1);
    assert(hostPtr->resizeCount == 1);
    assert(hostPtr->lastColumns == 120);
    assert(hostPtr->lastRows == 50);
    assert(hostPtr->openCount == 1);

    session.set_files({ "D:/projects/demo/other.frag" }, true);
    assert(hostPtr->openCount == 2);
    assert(hostPtr->lastProject.files.size() == 1);
    assert(hostPtr->lastProject.files[0] == std::filesystem::path("D:/projects/demo/other.frag"));

    session.pump();
    assert(hostPtr->pumpCount == 1);

    session.send_input("<Esc>");
    assert(hostPtr->sentInputs.size() == 1);
    assert(hostPtr->sentInputs[0] == "<Esc>");
}
