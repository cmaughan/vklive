#pragma once

#include <zest/ui/fonts.h>

namespace NodeGraph
{

using Zest::FontContext;
using Zest::IFontTexture;
using Zest::NVGglyphPosition;
using Zest::NVGtextRow;

using Zest::fonts_add_fallback;
using Zest::fonts_begin_frame;
using Zest::fonts_create;
using Zest::fonts_create_mem;
using Zest::fonts_destroy;
using Zest::fonts_draw_text;
using Zest::fonts_end_frame;
using Zest::fonts_find;
using Zest::fonts_init;
using Zest::fonts_reset_fallback;
using Zest::fonts_set_align;
using Zest::fonts_set_face;
using Zest::fonts_set_scale;
using Zest::fonts_set_size;
using Zest::fonts_text_bounds;
using Zest::fonts_text_box;
using Zest::fonts_text_box_bounds;
using Zest::fonts_text_metrics;

} // namespace NodeGraph
