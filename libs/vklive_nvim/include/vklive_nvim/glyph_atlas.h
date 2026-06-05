#pragma once

#include <vklive_nvim/types.h>

#include <cstdint>
#include <string>

namespace vklive_nvim
{

struct AtlasDirtyRect
{
    glm::ivec2 pos = {};
    glm::ivec2 size = {};
};

class IGlyphAtlas
{
public:
    virtual ~IGlyphAtlas() = default;

    virtual AtlasRegion resolve_cluster(const std::string& text, bool is_bold, bool is_italic) = 0;
    virtual int ligature_cell_span(const std::string& text, bool is_bold, bool is_italic) = 0;

    virtual bool atlas_dirty() const = 0;
    virtual bool consume_atlas_reset() = 0;
    virtual void clear_atlas_dirty() = 0;
    virtual const uint8_t* atlas_data() const = 0;
    virtual int atlas_width() const = 0;
    virtual int atlas_height() const = 0;
    virtual AtlasDirtyRect atlas_dirty_rect() const = 0;
};

} // namespace vklive_nvim
