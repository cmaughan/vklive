#include <cstdlib>
#include <iostream>

#include "config_app.h"

#include <zep/buffer.h>
#include <zep/display.h>
#include <zep/editor.h>
#include <zep/glyph_iterator.h>
#include <zep/syntax.h>
#include <zep/theme.h>

namespace
{

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
    Zep::ZepEditor editor(new Zep::ZepDisplayNull(), VKLIVE_ROOT, Zep::ZepEditorFlags::DisableThreads);
    auto* buffer = editor.GetEmptyBuffer("rt_trace.metal");
    buffer->SetText("kernel void vklive_ray_trace(texture2d<float, access::write> outImage) { float4 color; }\n");

    bool ok = true;
    ok &= require(buffer->GetSyntax() != nullptr, ".metal files should have a Zep syntax provider");
    if (buffer->GetSyntax())
    {
        ok &= require(buffer->GetSyntax()->GetSyntaxAt(Zep::GlyphIterator(buffer, 0)).foreground == Zep::ThemeColor::Keyword,
            "Metal keyword 'kernel' should be highlighted");
        ok &= require(buffer->GetSyntax()->GetSyntaxAt(Zep::GlyphIterator(buffer, 7)).foreground == Zep::ThemeColor::Keyword,
            "Metal keyword 'void' should be highlighted");
        ok &= require(buffer->GetSyntax()->GetSyntaxAt(Zep::GlyphIterator(buffer, 29)).foreground == Zep::ThemeColor::Identifier,
            "Metal type 'texture2d' should be highlighted as an identifier");
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
