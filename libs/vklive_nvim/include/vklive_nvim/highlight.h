#pragma once

#include <cstdint>
#include <vklive_nvim/types.h>
#include <functional>
#include <unordered_map>
#include <utility>

namespace vklive_nvim
{

struct HlAttr
{
    Color fg = Color(1.0f, 1.0f, 1.0f, 1.0f);
    Color bg = Color(0.0f, 0.0f, 0.0f, 1.0f);
    Color sp = Color(1.0f, 1.0f, 1.0f, 1.0f);
    bool has_fg = false;
    bool has_bg = false;
    bool has_sp = false;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool undercurl = false;
    bool strikethrough = false;
    bool reverse = false;

    bool operator==(const HlAttr&) const = default;

    uint32_t style_flags() const
    {
        uint32_t f = 0;
        if (bold)
            f |= STYLE_FLAG_BOLD;
        if (italic)
            f |= STYLE_FLAG_ITALIC;
        if (underline)
            f |= STYLE_FLAG_UNDERLINE;
        if (strikethrough)
            f |= STYLE_FLAG_STRIKETHROUGH;
        if (undercurl)
            f |= STYLE_FLAG_UNDERCURL;
        return f;
    }
};

// Hash functor for HlAttr — covers all fields using the boost hash-combine
// pattern so that std::unordered_map<HlAttr, uint16_t, HlAttrHash> gives O(1)
// attribute-id lookup in TerminalHostBase::attr_id().
struct HlAttrHash
{
    size_t operator()(const HlAttr& a) const noexcept
    {
        // Hash-combine helper (boost-style).
        auto combine = [](size_t& h, size_t v) {
            h ^= v + 0x9e3779b9u + (h << 6) + (h >> 2);
        };

        // Hash each float component of the three Color fields.
        auto hash_float = [](float f) { return std::hash<float>{}(f); };
        auto hash_bool = [](bool b) { return std::hash<bool>{}(b); };

        size_t h = hash_float(a.fg.r);
        combine(h, hash_float(a.fg.g));
        combine(h, hash_float(a.fg.b));
        combine(h, hash_float(a.fg.a));
        combine(h, hash_float(a.bg.r));
        combine(h, hash_float(a.bg.g));
        combine(h, hash_float(a.bg.b));
        combine(h, hash_float(a.bg.a));
        combine(h, hash_float(a.sp.r));
        combine(h, hash_float(a.sp.g));
        combine(h, hash_float(a.sp.b));
        combine(h, hash_float(a.sp.a));
        combine(h, hash_bool(a.has_fg));
        combine(h, hash_bool(a.has_bg));
        combine(h, hash_bool(a.has_sp));
        combine(h, hash_bool(a.bold));
        combine(h, hash_bool(a.italic));
        combine(h, hash_bool(a.underline));
        combine(h, hash_bool(a.undercurl));
        combine(h, hash_bool(a.strikethrough));
        combine(h, hash_bool(a.reverse));
        return h;
    }
};

class HighlightTable
{
public:
    void set(uint16_t id, const HlAttr& attr)
    {
        attrs_[id] = attr;
    }

    const HlAttr& get(uint16_t id) const
    {
        auto it = attrs_.find(id);
        if (it != attrs_.end())
            return it->second;
        return default_;
    }

    bool contains(uint16_t id) const
    {
        return attrs_.find(id) != attrs_.end();
    }

    void set_default_fg(Color c)
    {
        default_fg_ = c;
        default_.fg = c;
        default_.has_fg = true;
    }

    void set_default_bg(Color c)
    {
        default_bg_ = c;
        default_.bg = c;
        default_.has_bg = true;
    }

    void set_default_sp(Color c)
    {
        default_.sp = c;
        default_.has_sp = true;
    }

    Color default_fg() const
    {
        return default_fg_;
    }

    Color default_bg() const
    {
        return default_bg_;
    }

    void resolve(const HlAttr& attr, Color& fg, Color& bg, Color* sp = nullptr) const
    {
        fg = attr.has_fg ? attr.fg : default_fg_;
        bg = attr.has_bg ? attr.bg : default_bg_;
        if (attr.reverse)
            std::swap(fg, bg);
        if (sp)
            *sp = attr.has_sp ? attr.sp : fg;
    }

    void clear()
    {
        attrs_.clear();
    }

private:
    std::unordered_map<uint16_t, HlAttr> attrs_;
    Color default_fg_ = Color(1.0f, 1.0f, 1.0f, 1.0f);
    Color default_bg_ = Color(0.1f, 0.1f, 0.1f, 1.0f);
    HlAttr default_ = {};
};

} // namespace vklive_nvim
