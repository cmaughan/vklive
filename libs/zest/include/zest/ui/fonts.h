#pragma once
#include <cstdint>
#include <zest/ui/fontstash.h>
#include <zest/ui/nanovg.h>
#include <vector>

namespace Zest {

#define NVG_INIT_FONTIMAGE_SIZE 512
#define NVG_MAX_FONTIMAGE_SIZE 2048
#define NVG_MAX_FONTIMAGES 4

    /*
enum NVGalign
{
    // Horizontal align
    NVG_ALIGN_LEFT = 1 << 0, // Default, align text horizontally to left.
    NVG_ALIGN_CENTER = 1 << 1, // Align text horizontally to center.
    NVG_ALIGN_RIGHT = 1 << 2, // Align text horizontally to right.
    // Vertical align
    NVG_ALIGN_TOP = 1 << 3, // Align text vertically to top.
    NVG_ALIGN_MIDDLE = 1 << 4, // Align text vertically to middle.
    NVG_ALIGN_BOTTOM = 1 << 5, // Align text vertically to bottom.
    NVG_ALIGN_BASELINE = 1 << 6, // Default, align text vertically to baseline.
};*/

// Need your engine to implement these; simple texture management 
struct IFontTexture
{
    virtual int UpdateTexture(int image, int x, int y, int w, int h, const unsigned char* data) = 0;
    virtual int CreateTexture(int w, int h, const unsigned char* data) = 0;
    virtual void DeleteTexture(int image) = 0;
    virtual void GetTextureSize(int image, int* w, int* h) = 0;
    virtual void* GetTexture(int image) = 0;
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
};

struct FontContext
{
    struct FONScontext* fs;
    IFontTexture* pFontTexture = nullptr;
    int fontImages[NVG_MAX_FONTIMAGES];
    int fontImageIdx = 0;
    float fontSize = 150.0f;
    float letterSpacing = 0.0f;
    float lineHeight = 1.0f;
    float fontBlur = 0.0f;
    int textAlign = NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER;  // NVGalign flags
    int fontId;     // Current font
    float xform[6]; //?
    float devicePxRatio = 1.0f;
    void* userPtr = nullptr;
    float alpha = 1.0f;
};

struct NVGglyphPosition
{
    const char* str; // Position of the glyph in the input string.
    float x; // The x-coordinate of the logical glyph position.
    float minx, maxx; // The bounds of the glyph shape.
};
typedef struct NVGglyphPosition NVGglyphPosition;

struct NVGtextRow
{
    const char* start; // Pointer to the input text where the row starts.
    const char* end; // Pointer to the input text where the row ends (one past the last character).
    const char* next; // Pointer to the beginning of the next row.
    float width; // Logical width of the row.
    float minx, maxx; // Actual bounds of the row. Logical with and bounds can differ because of kerning and some parts over extending.
};
typedef struct NVGtextRow NVGtextRow;


void fonts_init(FontContext& ctx, IFontTexture* pFontTexture);
void fonts_destroy(FontContext& ctx);
int fonts_create(FontContext& ctx, const char* name, const char* filename);
int fonts_create(FontContext& ctx, const char* name, const char* filename, const int fontIndex);
int fonts_create_mem(FontContext& ctx, const char* name, unsigned char* data, int ndata, int freeData);
int fonts_create_mem(FontContext& ctx, const char* name, unsigned char* data, int ndata, int freeData, const int fontIndex);
int fonts_add_fallback(FontContext& ctx, int baseFont, int fallbackFont);
int fonts_add_fallback(FontContext& ctx, const char* baseFont, const char* fallbackFont);
void fonts_reset_fallback(FontContext& ctx, int baseFont);
void fonts_reset_fallback(FontContext& ctx, const char* baseFont);

void fonts_set_face(FontContext& ctx, const char* font);
void fonts_set_face(FontContext& ctx, int fontId);
void fonts_set_scale(FontContext& ctx, float scale);
void fonts_set_align(FontContext& ctx, int align);
void fonts_set_size(FontContext& ctx, float size);

int fonts_find(FontContext& ctx, const char* name);
void fonts_text_metrics(FontContext& ctx, float* ascender, float* descender, float* lineh);
float fonts_draw_text(FontContext& ctx, float x, float y, uint32_t color, const char* string, const char* end);
void fonts_text_box(FontContext& ctx, float x, float y, float breakRowWidth, uint32_t color, const char* string, const char* end);
float fonts_text_bounds(FontContext& ctx, float x, float y, const char* string, const char* end, float* bounds);
void fonts_text_box_bounds(FontContext& ctx, float x, float y, float breakRowWidth, const char* string, const char* end, float* bounds);

void fonts_end_frame(FontContext& ctx);
void fonts_begin_frame(FontContext& ctx);

} // Zest
