#ifndef FVFX_GUI_HELPERS
#define FVFX_GUI_HELPERS

#include <stdlib.h>
#include "modules/font_freetype.h"

extern GlyphAtlas atlas;

typedef struct{
    float x;
    float y;
    float width;
    float height;
} Rect;

Rect fitRectangle(Rect outer, float innerWidth, float innerHeight);

void drawText(char* text, uint32_t color, float fontSize, Rect bounding);
float measureText(char* text, float fontSize);
bool pointInsideRect(float x, float y, Rect rect);

#endif