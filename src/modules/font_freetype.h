#ifndef FVFX_FONT_FREETYPE
#define FVFX_FONT_FREETYPE

#define MAX_SDF_COUNT 1024

#define FREE_GLYPH_FONT_SIZE 64

#include <stdbool.h>
#include <vulkan/vulkan.h>

#include "modules/gmath.h"

typedef struct {
    float ax; // advance.x
    float ay; // advance.y

    float bw; // bitmap.width;
    float bh; // bitmap.rows;

    float bl; // bitmap_left;
    float bt; // bitmap_top;

    float tx; // x offset of glyph in texture coordinates
} GlyphMetric;

#define GLYPH_METRICS_CAPACITY 128

typedef struct {
    GlyphMetric* items;
    size_t count;
    size_t capacity;
} GlyphMetrics;

typedef struct{
    uint32_t width;
    uint32_t height;

    GlyphMetric glyphMetrics[GLYPH_METRICS_CAPACITY];
} GlyphAtlas;

bool GetFontSDFAtlas(const char* filename, VkImage* image, VkDeviceMemory* imageMemory, VkImageView* imageView, GlyphAtlas* glyphAtlas);

#define TEXTURE_EFFECT_SDF (1 << 2*8)

#endif