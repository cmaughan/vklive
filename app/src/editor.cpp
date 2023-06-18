
#if ZEP_SINGLE_HEADER == 1
#define ZEP_SINGLE_HEADER_BUILD
#endif

//#define ZEP_CONSOLE
#include "app/editor.h"
#include "app/config.h"
#include "config_app.h"

#include <vklive/file/file.h>
#include <vklive/message.h>

#include <clip/clip.h>

#include <functional>
#ifdef ZEP_CONSOLE
#include <zep\imgui\console_imgui.h>
#endif
#include <zep/filesystem.h>

using namespace Zep;

std::map<Zep::ZepBuffer*, std::set<std::string>> FileMessages;

class ZepEvaluateCommand : public ZepExCommand
{
public:
    ZepEvaluateCommand(ZepEditor& editor, const BufferUpdateCB& eval)
        : ZepExCommand(editor)
        , m_fnCallback(eval)
    {
        keymap_add(m_keymap, { "<C-Return>" }, ExCommandId());
    }

    static void Register(ZepEditor& editor, const BufferUpdateCB& cb)
    {
        editor.RegisterExCommand(std::make_shared<ZepEvaluateCommand>(editor, cb));
    }

    virtual void Run(const std::vector<std::string>& tokens) override
    {
        ZEP_UNUSED(tokens);
        if (!GetEditor().GetActiveTabWindow())
        {
            return;
        }

        auto& buffer = GetEditor().GetActiveTabWindow()->GetActiveWindow()->GetBuffer();
        auto cursor = GetEditor().GetActiveTabWindow()->GetActiveWindow()->GetBufferCursor();

        m_fnCallback(buffer, cursor);
    }

    virtual void Notify(std::shared_ptr<ZepMessage> message) override
    {
        ZEP_UNUSED(message);
    }
    virtual const char* ExCommandName() const override
    {
        return "VkEvaluateCommand";
    }
    virtual const KeyMap* GetKeyMappings(ZepMode&) const override
    {
        return &m_keymap;
    }

private:
    KeyMap m_keymap;
    BufferUpdateCB m_fnCallback;
};

struct ZepWrapper : public Zep::IZepComponent
{
    ZepWrapper(const fs::path& configRoot, const Zep::NVec2f& pixelScale, std::function<void(std::shared_ptr<Zep::ZepMessage>)> fnCommandCB)
        : zepEditor(configRoot, pixelScale)
        , Callback(fnCommandCB)
    {
        zepEditor.RegisterCallback(this);
    }

    virtual Zep::ZepEditor& GetEditor() const override
    {
        return (Zep::ZepEditor&)zepEditor;
    }

    virtual void Notify(std::shared_ptr<Zep::ZepMessage> message) override
    {
        Callback(message);

        return;
    }

    virtual void HandleInput()
    {
        zepEditor.HandleInput();
    }

    Zep::ZepEditor_ImGui zepEditor;
    std::function<void(std::shared_ptr<Zep::ZepMessage>)> Callback;
};

#ifdef ZEP_CONSOLE
std::shared_ptr<ImGui::ZepConsole> spZep;
#else
std::shared_ptr<ZepWrapper> spZep;
#endif

void zep_init(const fs::path& configRoot, const Zep::NVec2f& pixelScale, const BufferUpdateCB& fnBufferUpdate)
{
#ifdef ZEP_CONSOLE
    spZep = std::make_shared<ImGui::ZepConsole>(Zep::ZepPath(VKLIVE_ROOT));
#else
    // Initialize the editor and watch for changes
    spZep = std::make_shared<ZepWrapper>(configRoot, Zep::NVec2f(pixelScale.x, pixelScale.y), [](std::shared_ptr<ZepMessage> spMessage) -> void {
        if (spMessage->messageId == Zep::Msg::GetClipBoard)
        {
            clip::get_text(spMessage->str);
            spMessage->handled = true;
        }
        else if (spMessage->messageId == Zep::Msg::SetClipBoard)
        {
            clip::set_text(spMessage->str);
            spMessage->handled = true;
        }
    });
#endif

    ZepEvaluateCommand::Register(spZep->GetEditor(), fnBufferUpdate);

    auto& display = spZep->GetEditor().GetDisplay();
    auto pImFont = ImGui::GetIO().Fonts[0].Fonts[0];
    auto pixelHeight = pImFont->FontSize;
    display.SetFont(ZepTextType::UI, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pixelHeight)));
    display.SetFont(ZepTextType::Text, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pixelHeight)));
    display.SetFont(ZepTextType::Heading1, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pixelHeight * 1.5)));
    display.SetFont(ZepTextType::Heading2, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pixelHeight * 1.25)));
    display.SetFont(ZepTextType::Heading3, std::make_shared<ZepFont_ImGui>(display, pImFont, int(pixelHeight * 1.125)));
}

void zep_update()
{
}

void zep_destroy()
{
    spZep.reset();
}

ZepEditor& zep_get_editor()
{
    return spZep->GetEditor();
}

void zep_load(const fs::path& file, bool activate, uint32_t flags)
{
    // Get the buffer, or create
    auto pBuffer = zep_get_editor().GetFileBuffer(file, flags, true);

    // TODO: Theme/color.  Hijack Zep's theme for now
    auto& theme = pBuffer->GetTheme();
    if (file.extension() == ".scenegraph")
    {
        pBuffer->SetToneColor(theme.GetColor(theme.GetUniqueColor(0)));
    }
    else if (file.extension() == ".vert")
    {
        pBuffer->SetToneColor(theme.GetColor(theme.GetUniqueColor(1)));
    }
    else if (file.extension() == ".frag")
    {
        pBuffer->SetToneColor(theme.GetColor(theme.GetUniqueColor(4)));
    }
    else if (file.extension() == ".geom")
    {
        pBuffer->SetToneColor(theme.GetColor(theme.GetUniqueColor(3)));
    }

    // Find the buffer
    auto windows = zep_get_editor().FindBufferWindows(pBuffer);

    ZepTabWindow* pTabWindow = nullptr;

    // No window, put it in a tab
    if (windows.empty())
    {
        // Add a tab, add the buffer
        pTabWindow = zep_get_editor().AddTabWindow();
        zep_get_editor().SetCurrentWindow(pTabWindow->AddWindow(pBuffer));
    }
    else
    {
        if (activate)
        {
            zep_get_editor().SetCurrentWindow(windows[0]);
        }
    }
}

void zep_show(bool focus)
{
    // Required for CTRL+P and flashing cursor.
    spZep->GetEditor().RefreshRequired();

    auto windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar;
    if (appConfig.transparent_editor && appConfig.draw_on_background)
    {
        // With transparent editor, we don't draw the window back.
        windowFlags |= ImGuiWindowFlags_NoBackground;
    }


    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 50), ImGuiCond_FirstUseEver);

    static int focus_count = 0;
    if (focus)
    {
        focus_count = 0; 
    }
    
    if (focus_count++ < 4)
    {
        ImGui::SetNextWindowFocus();
    }

    if (!ImGui::Begin("Zep", nullptr, windowFlags))
    {
        ImGui::End();
        return;
    }

    auto min = ImGui::GetCursorScreenPos();
    auto max = ImGui::GetContentRegionAvail();
    if (max.x <= 0)
        max.x = 1;
    if (max.y <= 0)
        max.y = 1;
    ImGui::InvisibleButton("ZepContainer", max);

    // Fill the window
    max.x = min.x + max.x;
    max.y = min.y + max.y;

    spZep->zepEditor.GetConfig().style = (appConfig.draw_on_background && appConfig.transparent_editor) ? Zep::EditorStyle::Minimal : Zep::EditorStyle::Normal;
    spZep->zepEditor.SetDisplayRegion(Zep::NVec2f(min.x, min.y), Zep::NVec2f(max.x, max.y));
    spZep->zepEditor.Display();
    bool zep_focused = ImGui::IsWindowFocused();
    if (zep_focused)
    {
        spZep->zepEditor.HandleInput();
    }

    ImGui::End();
}

void zep_update_files(const fs::path& root, bool reset)
{
    zep_get_editor().GetFileSystem().SetWorkingDirectory(root);

    if (reset)
    {
        auto& buffers = zep_get_editor().GetBuffers();
        for (auto& buff : buffers)
        {
            zep_get_editor().RemoveBuffer(buff.get());
        }
    }

    auto files = file_gather_files(root);
    for (auto& f : files)
    {
        if (f.extension() == ".vert" || f.extension() == ".frag" || f.extension() == ".geom" || f.extension() == ".scenegraph" || f.extension() == ".h")
        {
            zep_load(f.string());
        }
    }
}

void zep_clear_all_messages()
{
    FileMessages.clear();

    // Clear out all the buffer markers
    for (auto& pBuffer : zep_get_editor().GetBuffers())
    {
        pBuffer->ClearRangeMarkers(Zep::RangeMarkerType::Mark);
        pBuffer->ClearFileFlags(Zep::FileFlags::HasErrors);
        pBuffer->ClearFileFlags(Zep::FileFlags::HasWarnings);
    }
}

void zep_add_file_message(Message& err)
{
    if (err.path.empty())
    {
        return;
    }
    // We fill up the shader file with errors, but try not to duplicate
    auto pBuffer = zep_get_editor().GetFileBuffer(err.path.string(), 0, true);
    if (!pBuffer)
    {
        return;
    }

    // Ugly; track message duplicates for now
    if (FileMessages[pBuffer].find(err.text) != FileMessages[pBuffer].end())
    {
        return;
    }
    FileMessages[pBuffer].insert(err.text);

    auto spMarker = std::make_shared<Zep::RangeMarker>(*pBuffer);
    spMarker->SetDescription(err.text);
    spMarker->SetName(err.text);
    spMarker->displayType = Zep::RangeMarkerDisplayType::All;

    // No line, so stack them along the first few lines for now
    // (zep is no good at stacking marker messages yet)
    if (err.line == -1)
    {
        err.line = FileMessages[pBuffer].size();
    }

    // Highlight the first line
    if (pBuffer->GetLineCount() < err.line || err.line < 0)
    {
        err.range = std::make_pair(0, 1);
    }

    if (err.severity == MessageSeverity::Error)
    {
        spMarker->SetHighlightColor(Zep::ThemeColor::Error);
        spMarker->SetBackgroundColor(Zep::ThemeColor::Error);
        pBuffer->SetFileFlags(Zep::FileFlags::HasErrors);
    }
    else
    {
        spMarker->SetHighlightColor(Zep::ThemeColor::Warning);
        spMarker->SetBackgroundColor(Zep::ThemeColor::Warning);
        pBuffer->SetFileFlags(Zep::FileFlags::HasWarnings);
    }

    Zep::ByteRange range;
    pBuffer->GetLineOffsets(err.line >= 0 ? err.line : 0, range);

    if (err.range.first != -1)
    {
        auto startLoc = range.first + err.range.first;
        if (pBuffer->GetWorkingBuffer().size() > startLoc && pBuffer->GetWorkingBuffer()[startLoc] != '\n')
        {
            range.first += err.range.first;
            if (err.range.second != -1)
            {
                range.second = range.first + (err.range.second - err.range.first);
                spMarker->SetRange(range);
            }
        }
        else
        {
            // If we are pointed at the end of a line for an error, ignore it and just highlight the line
            range.second = range.second - 1;
        }
    }
    else
    {
        // No given colum range, so we are just covering the line
        range.second = range.second - 1;
        if (range.second == range.first)
        {
            range.second = range.first + 1;
        }
    }
    spMarker->SetRange(range);
    pBuffer->AddRangeMarker(spMarker);
}
