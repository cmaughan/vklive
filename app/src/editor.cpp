
#if ZEP_SINGLE_HEADER == 1
#define ZEP_SINGLE_HEADER_BUILD
#endif

#include <fmt/format.h>

#include <zest/file/file.h>
#include <zest/file/runtree.h>
#include <vklive/process/process.h>

// #define ZEP_CONSOLE
#include "app/config.h"
#include "app/editor.h"
#include "config_app.h"

#include <vklive/message.h>

#include <clip/clip.h>

#include <functional>
#ifdef ZEP_CONSOLE
#include <zep\imgui\console_imgui.h>
#endif
#include <zep/filesystem.h>

#include <vklive/scene.h>

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

class ZepFormatCommand : public ZepExCommand
{
public:
    ZepFormatCommand(ZepEditor& editor, const BufferUpdateCB& fmt)
        : ZepExCommand(editor)
        , m_fnCallback(fmt)
    {
        keymap_add(m_keymap, { "<C-i><C-d>" }, ExCommandId());
    }

    static void Register(ZepEditor& editor, const BufferUpdateCB& cb)
    {
        editor.RegisterExCommand(std::make_shared<ZepFormatCommand>(editor, cb));
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
        return "VkFormatCommand";
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

void zep_init(const fs::path& configRoot, const Zep::NVec2f& pixelScale, const ZepEditorCB& editorCB)
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
        else if (spMessage->messageId == Zep::Msg::ModifyCommand)
        {
            auto spModify = std::static_pointer_cast<ModifyCommandMessage>(spMessage);
            auto& ctx = spModify->context;
            if (ctx.keymap.foundMapping == id_InsertCarriageReturn)
            {
                // Add 4 spaces after open bracket
                long numSpaces = 0;
                auto lastChar = ctx.buffer.GetLinePos(ctx.bufferCursor, LineLocation::LineLastGraphChar);
                if (lastChar.Valid() )
                {
                    if (*lastChar == '{')
                    {
                        numSpaces += 4;
                    }
                    else if (*lastChar == '}')
                    {
                        /*
                        auto found = ctx.buffer.FindMatchingParen(lastChar);
                        if (found.Valid())
                        {
                            auto firstChar = ctx.buffer.GetLinePos(found, LineLocation::LineBegin);
                            numSpaces += found.Index() - firstChar.Index();
                        }
                        */
                    }
                }

                // Add matching indent for new line
                auto firstPos = ctx.buffer.GetLinePos(ctx.bufferCursor, LineLocation::LineFirstGraphChar);
                auto begin = ctx.buffer.GetLinePos(ctx.bufferCursor, LineLocation::LineBegin);
                numSpaces += (firstPos.Index() - begin.Index());
                if (numSpaces > 0)
                {
                    spModify->context.tempReg.text.append(numSpaces, ' ');
                }
            }
        }
    });
#endif

    ZepEvaluateCommand::Register(spZep->GetEditor(), editorCB.updateCB);
    ZepFormatCommand::Register(spZep->GetEditor(), editorCB.formatCB);

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
        pBuffer->SetToneColor(theme.GetColor(theme.GetUniqueColor(2)));
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

void zep_modify_style()
{
    if (!spZep)
    {
        return;
    }

    // There is some conflict here with Dock that I don't understand; pushing the colors doesn't work.  Setting them does
    // Note we override the style everywhere here!
    auto oldBg = ImGui::GetStyleColorVec4(ImGuiCol_TitleBg);
    auto oldBgActive = ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive);
    auto oldMenuBg = ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg);
    auto oldWindowBg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    auto back = toImVec4(spZep->GetEditor().ModifyBackgroundColor(toNVec4f(oldBg)));
    auto backActive = toImVec4(spZep->GetEditor().ModifyBackgroundColor(toNVec4f(oldBgActive)));
    auto menuBg = toImVec4(spZep->GetEditor().ModifyBackgroundColor(toNVec4f(oldMenuBg)));
    auto windowBg = toImVec4(spZep->GetEditor().ModifyBackgroundColor(toNVec4f(oldWindowBg)));

    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_TitleBg] = back;
    style.Colors[ImGuiCol_TitleBgActive] = backActive;
    style.Colors[ImGuiCol_MenuBarBg] = menuBg;
    style.Colors[ImGuiCol_WindowBg] = windowBg;
}

void zep_reset_style()
{
    if (!spZep)
    {
        return;
    }

    // There is some conflict here with Dock that I don't understand; pushing the colors doesn't work.  Setting them does
    // Note we override the style everywhere here!
    auto oldBg = ImGui::GetStyleColorVec4(ImGuiCol_TitleBg);
    auto oldBgActive = ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive);
    auto oldMenuBg = ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg);
    auto oldWindowBg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    oldWindowBg.w = 255.0f;
    oldMenuBg.w = 255.0f;
    oldBg.w = 255.0f;
    oldBgActive.w = 255.0f;

    auto& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_TitleBg] = oldBg;
    style.Colors[ImGuiCol_TitleBgActive] = oldBgActive;
    style.Colors[ImGuiCol_MenuBarBg] = oldMenuBg;
    style.Colors[ImGuiCol_WindowBg] = oldWindowBg;
}

void zep_show(uint32_t focusFlags)
{
    zep_modify_style();

    // Required for CTRL+P and flashing cursor.
    spZep->GetEditor().RefreshRequired();

    auto windowFlags = uint32_t(ImGuiWindowFlags_NoScrollbar); // | ImGuiWindowFlags_MenuBar;
    if (appConfig.transparent_editor && appConfig.draw_on_background)
    {
        // With transparent editor, we don't draw the window back.
        windowFlags |= ImGuiWindowFlags_NoBackground;
    }

    // TODO: Set theme at start of day
    spZep->GetEditor().GetTheme().SetColor(ThemeColor::TabActive, NVec4f(0.5f, 0.7f, 0.5f, 1.0f));

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 50), ImGuiCond_FirstUseEver);

    static int focus_count = 0;

    // Focus this window if another window hasn't captured keyboard and we are being asked to check the focus
    if (focusFlags & ZepFocusFlags::CheckFocus)
    {
        focusFlags |= ZepFocusFlags::Focus;
    }

    if (focusFlags & ZepFocusFlags::Focus)
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

    zep_reset_style();
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

    auto files = Zest::file_gather_files(root);
    for (auto& f : files)
    {
        // TODO: Some helper functions to figure these extensions out.
        if (scene_is_edit_file(f))
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

void zep_replace_text(ZepBuffer& buffer, const std::string& text)
{
    auto cmd = std::make_shared<ZepCommand_ReplaceRange>(
        buffer,
        ReplaceRangeMode::Replace,
        buffer.Begin(),
        buffer.End(),
        text,
        buffer.GetMode()->GetCurrentWindow()->GetBufferCursor(),
        buffer.GetMode()->GetCurrentWindow()->GetBufferCursor());
    
    buffer.GetMode()->AddCommand(cmd);
}

bool format_file(const fs::path& path, std::string& out, uint32_t& cursorIndex)
{
    if (!path.has_extension() || !scene_is_shader(path))
    {
        return false;
    }

#ifdef WIN32
    auto tool_path = Zest::runtree_find_path("bin/win/clang-format.exe");
#elif defined(__APPLE__)
    auto tool_path = Zest::runtree_find_path("bin/mac/clang-format");
#elif defined(__linux__)
    auto tool_path = Zest::runtree_find_path("bin/linux/clang-format");
#endif

    if (!fs::exists(tool_path))
    {
        LOG(ERR, "Can't find format tool");
        return false;
    }

    if (!fs::exists(path))
    {
        LOG(ERR, "Can't find format tool");
        return false;
    }

    // Note; I couldn't get passing the style file to work, whatever I tried!
    // Manually supply these values; which I guess might make it easy for the user to
    // change them in a dialog, since we could build this dynamically
    std::string error;
    auto ret = run_process(
        { tool_path.string(),
            R"L(--style={
                Language: Cpp,
                IndentWidth: 4,
                TabWidth: 4,
                BreakBeforeBraces: Custom,
                BraceWrapping: {
                  AfterClass:      true,
                  AfterControlStatement: true,
                  AfterCaseLabel: true,
                  AfterEnum:       true,
                  AfterFunction:   true,
                  AfterNamespace:  true,
                  AfterStruct:     true,
                  AfterUnion:      true,
                  BeforeCatch:     true,
                  BeforeElse:      true,
                  IndentBraces:    false,
                },
                UseTab: Never,
                MaxEmptyLinesToKeep: 1})L",
            fmt::format("--cursor={}", cursorIndex),
            path.string() },
        &out);

    if (ret || out.empty())
    {
        LOG(DBG, "Could not run clang-format");
        LOG(DBG, out);
        return false;
    }
    cursorIndex = Zest::string_extract_integer(Zest::string_remove_first_line(out));


    return true;
}

void zep_format_buffer(ZepBuffer& buffer, uint32_t cursor)
{
    // Save the buffers
    if (buffer.HasFileFlags(Zep::FileFlags::Dirty))
    {
        int64_t sz;
        buffer.Save(sz);
    }

    std::string out;
    if (format_file(buffer.GetFilePath(), out, cursor) && !out.empty())
    {
        zep_replace_text(buffer, out);

        buffer.GetMode()->GetCurrentWindow()->SetBufferCursor(buffer.Begin() + cursor);
    }
}
