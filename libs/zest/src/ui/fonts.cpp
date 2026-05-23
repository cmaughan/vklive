#include <algorithm>
#include <stdio.h>
#include <stdlib.h>

#include <zest/math/imgui_glm.h>

#define FONTSTASH_IMPLEMENTATION
#include <zest/ui/fonts.h>

#include <zest/string/string_utils.h>

namespace Zest {

namespace {

struct NVGvertex
{
    float x, y, u, v;
};
typedef struct NVGvertex NVGvertex;
std::vector<NVGvertex> vertices;

struct NVGscissor
{
    float xform[6];
    float extent[2];
};
typedef struct NVGscissor NVGscissor;

float quantize(float a, float d)
{
    return ((int)(a / d + 0.5f)) * d;
}

float get_average_scale(float* t)
{
    float sx = sqrtf(t[0] * t[0] + t[2] * t[2]);
    float sy = sqrtf(t[1] * t[1] + t[3] * t[3]);
    return (sx + sy) * 0.5f;
}

static void set_transform_identity(float* t)
{
    t[0] = 1.0f;
    t[1] = 0.0f;
    t[2] = 0.0f;
    t[3] = 1.0f;
    t[4] = 0.0f;
    t[5] = 0.0f;
}

float get_font_scale(FontContext& ctx)
{
    return std::min(quantize(get_average_scale(ctx.xform), 0.01f), 6.0f);
}

int is_transform_flipped(const float* xform)
{
    float det = xform[0] * xform[3] - xform[2] * xform[1];
    return (det < 0);
}

NVGvertex* alloc_temp_verts(FontContext& ctx, size_t size)
{
    vertices.resize(size);
    return &vertices[0];
}

// Texture handling bits using the imgui API
int update_texture(FontContext& ctx, int image, int x, int y, int w, int h, const unsigned char* data)
{
    return ctx.pFontTexture->UpdateTexture(image, x, y, w, h, data);
}

int create_texture(FontContext& ctx, int w, int h, const unsigned char* data)
{
    return ctx.pFontTexture->CreateTexture(w, h, data);
}

void delete_texture(FontContext& ctx, int image)
{
    ctx.pFontTexture->DeleteTexture(image);
}

void get_texture_size(FontContext& ctx, int image, int* w, int* h)
{
    ctx.pFontTexture->GetTextureSize(image, w, h);
}

void render_text(FontContext& ctx, NVGvertex* verts, int nverts, uint32_t color)
{
    auto pDraw = ImGui::GetWindowDrawList();
    auto image = ctx.fontImages[ctx.fontImageIdx];
    if (image == 0)
    {
        return;
    }

    auto id = ctx.pFontTexture->GetTexture(image);
    if (id == 0)
    {
        return;
    }

    pDraw->PushTextureID((ImTextureID)id);

    pDraw->PrimReserve(nverts * 3, nverts * 2);

    for (int i = 0; i < nverts; i += 2)
    {
        pDraw->PrimRectUV(ImVec2(verts[i].x, verts[i].y), ImVec2(verts[i + 1].x, verts[i + 1].y), ImVec2(verts[i].u, verts[i].v), ImVec2(verts[i + 1].u, verts[i + 1].v), color);
    }

    pDraw->PopTextureID();
}

void flush_texture(FontContext& ctx)
{
//    #define ALWAYS_SEND_TEX
#ifdef ALWAYS_SEND_TEX
    {
        // Force
        int fontImage = ctx.fontImages[ctx.fontImageIdx];
        if (fontImage != 0)
        {
            int iw, ih;
            const unsigned char* data = fonsGetTextureData(ctx.fs, &iw, &ih);
            update_texture(ctx, fontImage, 0, 0, iw, ih, data);
        }
        return;
    }
#endif

    int dirty[4];

    // Figure out the dirty are of the texture and update it
    if (fonsValidateTexture(ctx.fs, dirty))
    {
        int fontImage = ctx.fontImages[ctx.fontImageIdx];
        // Update texture
        if (fontImage != 0)
        {
            int iw, ih;
            const unsigned char* data = fonsGetTextureData(ctx.fs, &iw, &ih);
            int x = dirty[0];
            int y = dirty[1];
            int w = dirty[2] - dirty[0];
            int h = dirty[3] - dirty[1];
            update_texture(ctx, fontImage, x, y, w, h, data);
        }
    }
}

int alloc_text_atlas(FontContext& ctx)
{
    int iw, ih;
    flush_texture(ctx);
    if (ctx.fontImageIdx >= NVG_MAX_FONTIMAGES - 1)
    {
        return 0;
    }
    // if next fontImage already have a texture
    if (ctx.fontImages[ctx.fontImageIdx + 1] != 0)
    {
        get_texture_size(ctx, ctx.fontImages[ctx.fontImageIdx + 1], &iw, &ih);
    }
    else
    { // calculate the new font image size and create it.
        get_texture_size(ctx, ctx.fontImages[ctx.fontImageIdx], &iw, &ih);
        if (iw > ih)
        {
            ih *= 2;
        }
        else
        {
            iw *= 2;
        }
        if (iw > NVG_MAX_FONTIMAGE_SIZE || ih > NVG_MAX_FONTIMAGE_SIZE)
        {
            iw = ih = NVG_MAX_FONTIMAGE_SIZE;
        }
        ctx.fontImages[ctx.fontImageIdx + 1] = create_texture(ctx, iw, ih, nullptr);
    }
    ++ctx.fontImageIdx;
    fonsResetAtlas(ctx.fs, iw, ih);
    return 1;
}

} // namespace

void fonts_init(FontContext& ctx, IFontTexture* pFontTexture)
{
    FONSparams fontParams;
    for (uint32_t i = 0; i < NVG_MAX_FONTIMAGES; i++)
    {
        ctx.fontImages[i] = 0;
    }

    ctx.pFontTexture = pFontTexture;
    set_transform_identity(ctx.xform);

    // Init font rendering
    memset(&fontParams, 0, sizeof(fontParams));
    fontParams.width = NVG_INIT_FONTIMAGE_SIZE;
    fontParams.height = NVG_INIT_FONTIMAGE_SIZE;
    fontParams.flags = FONS_ZERO_TOPLEFT;
    fontParams.renderCreate = NULL;
    fontParams.renderUpdate = NULL;
    fontParams.renderDraw = NULL;
    fontParams.renderDelete = NULL;
    fontParams.userPtr = NULL;
    ctx.fs = ::fonsCreateInternal(&fontParams);

    // Create font texture
    ctx.fontImages[0] = create_texture(ctx, fontParams.width, fontParams.height, nullptr);
    ctx.fontImageIdx = 0;
}

void fonts_destroy(FontContext& ctx)
{
    if (ctx.fs)
    {
        fonsDeleteInternal(ctx.fs);
    }
    for (uint32_t i = 0; i < NVG_MAX_FONTIMAGES; i++)
    {
        if (ctx.fontImages[i] != 0)
        {
            delete_texture(ctx, ctx.fontImages[i]);
            ctx.fontImages[i] = 0;
        }
    }
}

// Add a font file to the cache of fonts we can use
int fonts_create(FontContext& ctx, const char* name, const char* filename)
{
    return fonsAddFont(ctx.fs, name, filename, 0);
}

// Add a font file and store it in a given index
int fonts_create(FontContext& ctx, const char* name, const char* filename, const int fontIndex)
{
    return fonsAddFont(ctx.fs, name, filename, fontIndex);
}

// Add a font stored in memory
int fonts_create_mem(FontContext& ctx, const char* name, unsigned char* data, int ndata, int freeData)
{
    return fonsAddFontMem(ctx.fs, name, data, ndata, freeData, 0);
}

// Add a font stored in memory at the given index
int fonts_create_mem(FontContext& ctx, const char* name, unsigned char* data, int ndata, int freeData, const int fontIndex)
{
    return fonsAddFontMem(ctx.fs, name, data, ndata, freeData, fontIndex);
}

// Find a font id by name
int fonts_find(FontContext& ctx, const char* name)
{
    if (name == NULL)
        return -1;
    return fonsGetFontByName(ctx.fs, name);
}

// I think this is in case the current font isn't found, or the glyph?
int fonts_add_fallback(FontContext& ctx, int baseFont, int fallbackFont)
{
    if (baseFont == -1 || fallbackFont == -1)
        return 0;
    return fonsAddFallbackFont(ctx.fs, baseFont, fallbackFont);
}

int fonts_add_fallback(FontContext& ctx, const char* baseFont, const char* fallbackFont)
{
    return fonts_add_fallback(ctx, fonts_find(ctx, baseFont), fonts_find(ctx, fallbackFont));
}

void fonts_reset_fallback(FontContext& ctx, int baseFont)
{
    fonsResetFallbackFont(ctx.fs, baseFont);
}

void fonts_reset_fallback(FontContext& ctx, const char* baseFont)
{
    fonts_reset_fallback(ctx, fonts_find(ctx, baseFont));
}

// Set the current font by name
void fonts_set_face(FontContext& ctx, const char* font)
{
    ctx.fontId = fonsGetFontByName(ctx.fs, font);
}

void fonts_set_face(FontContext& ctx, int fontId)
{
    ctx.fontId = fontId;
}


void fonts_set_align(FontContext& ctx, int align)
{
    ctx.textAlign = align;
}

void fonts_set_size(FontContext& ctx, float size)
{
    ctx.fontSize = size;
}

void fonts_text_metrics(FontContext& ctx, float* ascender, float* descender, float* lineh)
{
    float scale = get_font_scale(ctx) * ctx.devicePxRatio;
    float invscale = 1.0f / scale;

    if (ctx.fontId == FONS_INVALID)
        return;

    fonsSetSize(ctx.fs, ctx.fontSize * scale);
    fonsSetSpacing(ctx.fs, ctx.letterSpacing * scale);
    fonsSetBlur(ctx.fs, ctx.fontBlur * scale);
    fonsSetAlign(ctx.fs, ctx.textAlign);
    fonsSetFont(ctx.fs, ctx.fontId);

    fonsVertMetrics(ctx.fs, ascender, descender, lineh);
    if (ascender != NULL)
        *ascender *= invscale;
    if (descender != NULL)
        *descender *= invscale;
    if (lineh != NULL)
        *lineh *= invscale;
}

void fonts_set_scale(FontContext& ctx, float scale)
{
    set_transform_identity(ctx.xform);
    ctx.xform[0] = scale;
    ctx.xform[1] = 0.0f;
    ctx.xform[2] = 0.0f;
    ctx.xform[3] = scale;
    ctx.xform[4] = 0.0f;
    ctx.xform[5] = 0.0f;
}

float fonts_draw_text(FontContext& ctx, float x, float y, uint32_t color, const char* string, const char* end)
{
    FONStextIter iter, prevIter;
    FONSquad q;
    NVGvertex* verts;
    float scale = get_font_scale(ctx) * ctx.devicePxRatio;
    float invscale = 1.0f / scale;
    int cverts = 0;
    int nverts = 0;
    int isFlipped = is_transform_flipped(ctx.xform);

    if (end == NULL)
        end = string + Zest::string_utf8_length(string);

    if (ctx.fontId == FONS_INVALID)
        return x;

    fonsSetSize(ctx.fs, ctx.fontSize * scale);
    fonsSetSpacing(ctx.fs, ctx.letterSpacing * scale);
    fonsSetBlur(ctx.fs, ctx.fontBlur * scale);
    fonsSetAlign(ctx.fs, ctx.textAlign);
    fonsSetFont(ctx.fs, ctx.fontId);

    cverts = std::max(2, (int)(end - string)) * 4; // conservative estimate.
    verts = alloc_temp_verts(ctx, cverts);
    if (verts == NULL)
        return x;

    fonsTextIterInit(ctx.fs, &iter, x * scale, y * scale, string, end, FONS_GLYPH_BITMAP_REQUIRED);

    prevIter = iter;
    while (fonsTextIterNext(ctx.fs, &iter, &q))
    {
        float c[4 * 2];
        if (iter.prevGlyphIndex == -1)
        { // can not retrieve glyph?
            if (nverts != 0)
            {
                render_text(ctx, verts, nverts, color);
                nverts = 0;
            }
            if (!alloc_text_atlas(ctx))
                break; // no memory :(
            iter = prevIter;
            fonsTextIterNext(ctx.fs, &iter, &q); // try again
            if (iter.prevGlyphIndex == -1) // still can not find glyph?
                break;
        }
        prevIter = iter;
        if (isFlipped)
        {
            float tmp;

            tmp = q.y0;
            q.y0 = q.y1;
            q.y1 = tmp;
            tmp = q.t0;
            q.t0 = q.t1;
            q.t1 = tmp;
        }

        auto transformPoint = [](float* dx, float* dy, const float* t, float sx, float sy) {
            *dx = sx * t[0] + sy * t[2] + t[4];
            *dy = sx * t[1] + sy * t[3] + t[5];
        };

        // Transform corners.
        transformPoint(&c[0], &c[1], ctx.xform, q.x0 * invscale, q.y0 * invscale);
        transformPoint(&c[2], &c[3], ctx.xform, q.x1 * invscale, q.y1 * invscale);

        auto vset = [](NVGvertex* vtx, float x, float y, float u, float v) {
            vtx->x = x;
            vtx->y = y;
            vtx->u = u;
            vtx->v = v;
        };

        // Create triangles
        if (nverts + 2 <= cverts)
        {
            vset(&verts[nverts], c[0], c[1], q.s0, q.t0);
            nverts++;
            vset(&verts[nverts], c[2], c[3], q.s1, q.t1);
            nverts++;
        }
    }

    // TODO: add back-end bit to do this just once per frame.
    //flush_texture(ctx);

    render_text(ctx, verts, nverts, color);

    return iter.nextx / scale;
}

enum NVGcodepointType
{
    NVG_SPACE,
    NVG_NEWLINE,
    NVG_CHAR,
    NVG_CJK_CHAR,
};

int fonts_break_lines(FontContext& ctx, const char* string, const char* end, float breakRowWidth, NVGtextRow* rows, int maxRows)
{
    float scale = get_font_scale(ctx) * ctx.devicePxRatio;
    float invscale = 1.0f / scale;
    FONStextIter iter, prevIter;
    FONSquad q;
    int nrows = 0;
    float rowStartX = 0;
    float rowWidth = 0;
    float rowMinX = 0;
    float rowMaxX = 0;
    const char* rowStart = NULL;
    const char* rowEnd = NULL;
    const char* wordStart = NULL;
    float wordStartX = 0;
    float wordMinX = 0;
    const char* breakEnd = NULL;
    float breakWidth = 0;
    float breakMaxX = 0;
    int type = NVG_SPACE, ptype = NVG_SPACE;
    unsigned int pcodepoint = 0;

    if (maxRows == 0)
        return 0;
    if (ctx.fontId == FONS_INVALID)
        return 0;

    if (end == NULL)
        end = string + Zest::string_utf8_length(string);

    if (string == end)
        return 0;

    fonsSetSize(ctx.fs, ctx.fontSize * scale);
    fonsSetSpacing(ctx.fs, ctx.letterSpacing * scale);
    fonsSetBlur(ctx.fs, ctx.fontBlur * scale);
    fonsSetAlign(ctx.fs, ctx.textAlign);
    fonsSetFont(ctx.fs, ctx.fontId);

    breakRowWidth *= scale;

    fonsTextIterInit(ctx.fs, &iter, 0, 0, string, end, FONS_GLYPH_BITMAP_OPTIONAL);
    prevIter = iter;
    while (fonsTextIterNext(ctx.fs, &iter, &q))
    {
        if (iter.prevGlyphIndex < 0 && alloc_text_atlas(ctx))
        { // can not retrieve glyph?
            iter = prevIter;
            fonsTextIterNext(ctx.fs, &iter, &q); // try again
        }
        prevIter = iter;
        switch (iter.codepoint)
        {
        case 9: // \t
        case 11: // \v
        case 12: // \f
        case 32: // space
        case 0x00a0: // NBSP
            type = NVG_SPACE;
            break;
        case 10: // \n
            type = pcodepoint == 13 ? NVG_SPACE : NVG_NEWLINE;
            break;
        case 13: // \r
            type = pcodepoint == 10 ? NVG_SPACE : NVG_NEWLINE;
            break;
        case 0x0085: // NEL
            type = NVG_NEWLINE;
            break;
        default:
            if ((iter.codepoint >= 0x4E00 && iter.codepoint <= 0x9FFF) || (iter.codepoint >= 0x3000 && iter.codepoint <= 0x30FF) || (iter.codepoint >= 0xFF00 && iter.codepoint <= 0xFFEF) || (iter.codepoint >= 0x1100 && iter.codepoint <= 0x11FF) || (iter.codepoint >= 0x3130 && iter.codepoint <= 0x318F) || (iter.codepoint >= 0xAC00 && iter.codepoint <= 0xD7AF))
                type = NVG_CJK_CHAR;
            else
                type = NVG_CHAR;
            break;
        }

        if (type == NVG_NEWLINE)
        {
            // Always handle new lines.
            rows[nrows].start = rowStart != NULL ? rowStart : iter.str;
            rows[nrows].end = rowEnd != NULL ? rowEnd : iter.str;
            rows[nrows].width = rowWidth * invscale;
            rows[nrows].minx = rowMinX * invscale;
            rows[nrows].maxx = rowMaxX * invscale;
            rows[nrows].next = iter.next;
            nrows++;
            if (nrows >= maxRows)
                return nrows;
            // Set null break point
            breakEnd = rowStart;
            breakWidth = 0.0;
            breakMaxX = 0.0;
            // Indicate to skip the white space at the beginning of the row.
            rowStart = NULL;
            rowEnd = NULL;
            rowWidth = 0;
            rowMinX = rowMaxX = 0;
        }
        else
        {
            if (rowStart == NULL)
            {
                // Skip white space until the beginning of the line
                if (type == NVG_CHAR || type == NVG_CJK_CHAR)
                {
                    // The current char is the row so far
                    rowStartX = iter.x;
                    rowStart = iter.str;
                    rowEnd = iter.next;
                    rowWidth = iter.nextx - rowStartX;
                    rowMinX = q.x0 - rowStartX;
                    rowMaxX = q.x1 - rowStartX;
                    wordStart = iter.str;
                    wordStartX = iter.x;
                    wordMinX = q.x0 - rowStartX;
                    // Set null break point
                    breakEnd = rowStart;
                    breakWidth = 0.0;
                    breakMaxX = 0.0;
                }
            }
            else
            {
                float nextWidth = iter.nextx - rowStartX;

                // track last non-white space character
                {
                    rowEnd = iter.next;
                    rowWidth = iter.nextx - rowStartX;
                    rowMaxX = q.x1 - rowStartX;
                }
                // track last end of a word
                if (((ptype == NVG_CHAR || ptype == NVG_CJK_CHAR) && type == NVG_SPACE) || type == NVG_CJK_CHAR)
                {
                    breakEnd = iter.str;
                    breakWidth = rowWidth;
                    breakMaxX = rowMaxX;
                }
                // track last beginning of a word
                if ((ptype == NVG_SPACE && (type == NVG_CHAR || type == NVG_CJK_CHAR)) || type == NVG_CJK_CHAR)
                {
                    wordStart = iter.str;
                    wordStartX = iter.x;
                    wordMinX = q.x0;
                }

                // Break to new line when a character is beyond break width.
                if ((type == NVG_CHAR || type == NVG_CJK_CHAR) && nextWidth > breakRowWidth)
                {
                    // The run length is too long, need to break to new line.
                    if (breakEnd == rowStart)
                    {
                        // The current word is longer than the row length, just break it from here.
                        rows[nrows].start = rowStart;
                        rows[nrows].end = iter.str;
                        rows[nrows].width = rowWidth * invscale;
                        rows[nrows].minx = rowMinX * invscale;
                        rows[nrows].maxx = rowMaxX * invscale;
                        rows[nrows].next = iter.str;
                        nrows++;
                        if (nrows >= maxRows)
                            return nrows;
                        rowStartX = iter.x;
                        rowStart = iter.str;
                        rowEnd = iter.next;
                        rowWidth = iter.nextx - rowStartX;
                        rowMinX = q.x0 - rowStartX;
                        rowMaxX = q.x1 - rowStartX;
                        wordStart = iter.str;
                        wordStartX = iter.x;
                        wordMinX = q.x0 - rowStartX;
                    }
                    else
                    {
                        // Break the line from the end of the last word, and start new line from the beginning of the new.
                        rows[nrows].start = rowStart;
                        rows[nrows].end = breakEnd;
                        rows[nrows].width = breakWidth * invscale;
                        rows[nrows].minx = rowMinX * invscale;
                        rows[nrows].maxx = breakMaxX * invscale;
                        rows[nrows].next = wordStart;
                        nrows++;
                        if (nrows >= maxRows)
                            return nrows;
                        // Update row
                        rowStartX = wordStartX;
                        rowStart = wordStart;
                        rowEnd = iter.next;
                        rowWidth = iter.nextx - rowStartX;
                        rowMinX = wordMinX - rowStartX;
                        rowMaxX = q.x1 - rowStartX;
                    }
                    // Set null break point
                    breakEnd = rowStart;
                    breakWidth = 0.0;
                    breakMaxX = 0.0;
                }
            }
        }

        pcodepoint = iter.codepoint;
        ptype = type;
    }

    // Break the line from the end of the last word, and start new line from the beginning of the new.
    if (rowStart != NULL)
    {
        rows[nrows].start = rowStart;
        rows[nrows].end = rowEnd;
        rows[nrows].width = rowWidth * invscale;
        rows[nrows].minx = rowMinX * invscale;
        rows[nrows].maxx = rowMaxX * invscale;
        rows[nrows].next = end;
        nrows++;
    }

    return nrows;
}

void fonts_text_box(FontContext& ctx, float x, float y, float breakRowWidth, uint32_t color, const char* string, const char* end)
{
    NVGtextRow rows[2];
    int nrows = 0, i;
    int oldAlign = ctx.textAlign;
    int haling = ctx.textAlign & (NVG_ALIGN_LEFT | NVG_ALIGN_CENTER | NVG_ALIGN_RIGHT);
    int valign = ctx.textAlign & (NVG_ALIGN_TOP | NVG_ALIGN_MIDDLE | NVG_ALIGN_BOTTOM | NVG_ALIGN_BASELINE);
    float lineh = 0;

    if (ctx.fontId == FONS_INVALID)
        return;

    fonts_text_metrics(ctx, NULL, NULL, &lineh);

    ctx.textAlign = NVG_ALIGN_LEFT | valign;

    while ((nrows = fonts_break_lines(ctx, string, end, breakRowWidth, rows, 2)))
    {
        for (i = 0; i < nrows; i++)
        {
            NVGtextRow* row = &rows[i];
            if (haling & NVG_ALIGN_LEFT)
                fonts_draw_text(ctx, x, y, color, row->start, row->end);
            else if (haling & NVG_ALIGN_CENTER)
                fonts_draw_text(ctx, x + breakRowWidth * 0.5f - row->width * 0.5f, y, color, row->start, row->end);
            else if (haling & NVG_ALIGN_RIGHT)
                fonts_draw_text(ctx, x + breakRowWidth - row->width, y, color, row->start, row->end);
            y += lineh * ctx.lineHeight;
        }
        string = rows[nrows - 1].next;
    }

    ctx.textAlign = oldAlign;
}

int text_glyph_positions(FontContext& ctx, float x, float y, const char* string, const char* end, NVGglyphPosition* positions, int maxPositions)
{
    float scale = get_font_scale(ctx) * ctx.devicePxRatio;
    float invscale = 1.0f / scale;
    FONStextIter iter, prevIter;
    FONSquad q;
    int npos = 0;

    if (ctx.fontId == FONS_INVALID)
        return 0;

    if (end == NULL)
        end = string + Zest::string_utf8_length(string);

    if (string == end)
        return 0;

    fonsSetSize(ctx.fs, ctx.fontSize * scale);
    fonsSetSpacing(ctx.fs, ctx.letterSpacing * scale);
    fonsSetBlur(ctx.fs, ctx.fontBlur * scale);
    fonsSetAlign(ctx.fs, ctx.textAlign);
    fonsSetFont(ctx.fs, ctx.fontId);

    fonsTextIterInit(ctx.fs, &iter, x * scale, y * scale, string, end, FONS_GLYPH_BITMAP_OPTIONAL);
    prevIter = iter;
    while (fonsTextIterNext(ctx.fs, &iter, &q))
    {
        if (iter.prevGlyphIndex < 0 && alloc_text_atlas(ctx))
        { // can not retrieve glyph?
            iter = prevIter;
            fonsTextIterNext(ctx.fs, &iter, &q); // try again
        }
        prevIter = iter;
        positions[npos].str = iter.str;
        positions[npos].x = iter.x * invscale;
        positions[npos].minx = std::min(iter.x, q.x0) * invscale;
        positions[npos].maxx = std::max(iter.nextx, q.x1) * invscale;
        npos++;
        if (npos >= maxPositions)
            break;
    }

    return npos;
}

float fonts_text_bounds(FontContext& ctx, float x, float y, const char* string, const char* end, float* bounds)
{
    float scale = get_font_scale(ctx) * ctx.devicePxRatio;
    float invscale = 1.0f / scale;
    float width;

    if (ctx.fontId == FONS_INVALID)
        return 0;

    fonsSetSize(ctx.fs, ctx.fontSize * scale);
    fonsSetSpacing(ctx.fs, ctx.letterSpacing * scale);
    fonsSetBlur(ctx.fs, ctx.fontBlur * scale);
    fonsSetAlign(ctx.fs, ctx.textAlign);
    fonsSetFont(ctx.fs, ctx.fontId);

    width = fonsTextBounds(ctx.fs, x * scale, y * scale, string, end, bounds);
    if (bounds != NULL)
    {
        // Use line bounds for height.
        fonsLineBounds(ctx.fs, y * scale, &bounds[1], &bounds[3]);
        bounds[0] *= invscale;
        bounds[1] *= invscale;
        bounds[2] *= invscale;
        bounds[3] *= invscale;
    }
    return width * invscale;
}

void fonts_text_box_bounds(FontContext& ctx, float x, float y, float breakRowWidth, const char* string, const char* end, float* bounds)
{
    NVGtextRow rows[2];
    float scale = get_font_scale(ctx) * ctx.devicePxRatio;
    float invscale = 1.0f / scale;
    int nrows = 0, i;
    int oldAlign = ctx.textAlign;
    int haling = ctx.textAlign & (NVG_ALIGN_LEFT | NVG_ALIGN_CENTER | NVG_ALIGN_RIGHT);
    int valign = ctx.textAlign & (NVG_ALIGN_TOP | NVG_ALIGN_MIDDLE | NVG_ALIGN_BOTTOM | NVG_ALIGN_BASELINE);
    float lineh = 0, rminy = 0, rmaxy = 0;
    float minx, miny, maxx, maxy;

    if (ctx.fontId == FONS_INVALID)
    {
        if (bounds != NULL)
            bounds[0] = bounds[1] = bounds[2] = bounds[3] = 0.0f;
        return;
    }

    fonts_text_metrics(ctx, NULL, NULL, &lineh);

    ctx.textAlign = NVG_ALIGN_LEFT | valign;

    minx = maxx = x;
    miny = maxy = y;

    fonsSetSize(ctx.fs, ctx.fontSize * scale);
    fonsSetSpacing(ctx.fs, ctx.letterSpacing * scale);
    fonsSetBlur(ctx.fs, ctx.fontBlur * scale);
    fonsSetAlign(ctx.fs, ctx.textAlign);
    fonsSetFont(ctx.fs, ctx.fontId);
    fonsLineBounds(ctx.fs, 0, &rminy, &rmaxy);
    rminy *= invscale;
    rmaxy *= invscale;

    while ((nrows = fonts_break_lines(ctx, string, end, breakRowWidth, rows, 2)))
    {
        for (i = 0; i < nrows; i++)
        {
            NVGtextRow* row = &rows[i];
            float rminx, rmaxx, dx = 0;
            // Horizontal bounds
            if (haling & NVG_ALIGN_LEFT)
                dx = 0;
            else if (haling & NVG_ALIGN_CENTER)
                dx = breakRowWidth * 0.5f - row->width * 0.5f;
            else if (haling & NVG_ALIGN_RIGHT)
                dx = breakRowWidth - row->width;
            rminx = x + row->minx + dx;
            rmaxx = x + row->maxx + dx;
            minx = std::min(minx, rminx);
            maxx = std::max(maxx, rmaxx);
            // Vertical bounds.
            miny = std::min(miny, y + rminy);
            maxy = std::max(maxy, y + rmaxy);

            y += lineh * ctx.lineHeight;
        }
        string = rows[nrows - 1].next;
    }

    ctx.textAlign = oldAlign;

    if (bounds != NULL)
    {
        bounds[0] = minx;
        bounds[1] = miny;
        bounds[2] = maxx;
        bounds[3] = maxy;
    }
}

void fonts_begin_frame(FontContext& ctx)
{
    ctx.pFontTexture->BeginFrame();
}

void fonts_end_frame(FontContext& ctx)
{
    flush_texture(ctx);
    if (ctx.fontImageIdx != 0)
    {
        int fontImage = ctx.fontImages[ctx.fontImageIdx];
        ctx.fontImages[ctx.fontImageIdx] = 0;
        int i, j, iw, ih;
        // delete images that smaller than current one
        if (fontImage == 0)
            return;
        get_texture_size(ctx, fontImage, &iw, &ih);
        for (i = j = 0; i < ctx.fontImageIdx; i++)
        {
            if (ctx.fontImages[i] != 0)
            {
                int nw, nh;
                int image = ctx.fontImages[i];
                ctx.fontImages[i] = 0;
                get_texture_size(ctx, image, &nw, &nh);
                if (nw < iw || nh < ih)
                {
                    delete_texture(ctx, image);
                }
                else
                {
                    ctx.fontImages[j++] = image;
                }
            }
        }
        // make current font image to first
        ctx.fontImages[j] = ctx.fontImages[0];
        ctx.fontImages[0] = fontImage;
        ctx.fontImageIdx = 0;
    }

    ctx.pFontTexture->EndFrame();
}

} // Zest
