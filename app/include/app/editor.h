#pragma once

#include <functional>

#include <Zest/file/file.h>

#include <zep.h>

#include <vklive/message.h>

using cmdFunc = std::function<void(const std::vector<std::string>&)>;
class ZepCmd : public Zep::ZepExCommand
{
public:
    ZepCmd(Zep::ZepEditor& editor, const std::string name, cmdFunc fn)
        : Zep::ZepExCommand(editor)
        , m_name(name)
        , m_func(fn)
    {
    }

    virtual void Run(const std::vector<std::string>& args) override
    {
        m_func(args);
    }

    virtual const char* ExCommandName() const override
    {
        return m_name.c_str();
    }

private:
    std::string m_name;
    cmdFunc m_func;
};

using BufferUpdateCB = std::function<void(Zep::ZepBuffer&, const Zep::GlyphIterator& itr)>;

// Helpers to create zep editor
Zep::ZepEditor& zep_get_editor();
void zep_init(const fs::path& root, const Zep::NVec2f& pixelScale, const BufferUpdateCB& fnBufferUpdate);
void zep_show(bool focus = false);
void zep_destroy();
void zep_load(const fs::path& file, bool activate = false, uint32_t flags = 0);
void zep_update_files(const fs::path& root, bool reset);
void zep_add_file_message(Message& err);
void zep_clear_all_messages();
