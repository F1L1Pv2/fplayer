#include "gui_helpers.h"

Rect fitRectangle(Rect outer, float innerWidth, float innerHeight){
    Rect out = {0};
    out.x = outer.x;
    out.y = outer.y;

    float innerAspect = (float)innerWidth / (float)innerHeight;
    float outerAspect = outer.width / outer.height;

    if (outerAspect < innerAspect) {
        float scaledH = outer.width / innerAspect;
        float yOffset = (outer.height - scaledH) * 0.5f;

        out.width = outer.width;
        out.height = scaledH;
        out.y += yOffset;
    } else {
        float scaledW = outer.height * innerAspect;
        float xOffset = (outer.width - scaledW) * 0.5f;
        
        out.width = scaledW;
        out.height = outer.height;
        out.x += xOffset;
    }

    return out;
}

#include "modules/bindlessTexturesManager.h"
#include "modules/spriteManager.h"

void hexToRgb(uint32_t hex, float* r, float *g, float *b){
    *b = (float)((hex >> (0*8)) & 0xFF) / 255.0f;
    *g = (float)((hex >> (1*8)) & 0xFF) / 255.0f;
    *r = (float)((hex >> (2*8)) & 0xFF) / 255.0f;
}

void drawText(char* text, uint32_t color, float fontSize, Rect bounding){
    float r,g,b;
    hexToRgb(color, &r,&g,&b);

    float initialX = bounding.x;
    size_t len = strlen(text);
    for(int i = 0; i < len; i++){
        char ch = text[i];
        if(ch == '\n'){
            bounding.x = initialX;
            bounding.y += fontSize;
        }
        if ((unsigned char)ch >= GLYPH_METRICS_CAPACITY) ch = '?';
        
        GlyphMetric metric = atlas.glyphMetrics[ch];

        float x2 = bounding.x + metric.bl * fontSize / FREE_GLYPH_FONT_SIZE;
        float y2 = bounding.y + fontSize - metric.bt * fontSize / FREE_GLYPH_FONT_SIZE;

        float w = metric.bw * fontSize / FREE_GLYPH_FONT_SIZE;
        float h = metric.bh * fontSize / FREE_GLYPH_FONT_SIZE;

        bounding.x += metric.ax * fontSize / FREE_GLYPH_FONT_SIZE;
        bounding.y += metric.ay * fontSize / FREE_GLYPH_FONT_SIZE;

        float r,g,b;

        hexToRgb(color, &r,&g,&b);

        drawSprite((SpriteDrawCommand){
            .position = (vec2){x2,y2},
            .scale = (vec2){w,h},
            .textureIDEffects = getTextureID("font") | TEXTURE_EFFECT_SDF,
            .offset = (vec2){.x = metric.tx, .y = 0},
            .size = (vec2){
                .x = metric.bw / (float)atlas.width,
                .y = metric.bh / (float) atlas.height
            },
            .albedo = (vec3){r,g,b},
        });
    }
}

float measureText(char* text, float fontSize){
    float width = 0;
    float newWidth = 0;

    size_t len = strlen(text);
    for(int i = 0; i < len; i++){
        char ch = text[i];
        if(ch == '\n'){
            if (newWidth > width) width = newWidth;
            newWidth = 0;
            continue;        
        }

        GlyphMetric metric = atlas.glyphMetrics[ch];
        newWidth += metric.ax * fontSize / FREE_GLYPH_FONT_SIZE;
    }

    if (newWidth > width) width = newWidth;

    return width;
}

bool pointInsideRect(float x, float y, Rect rect){
    return !(
        (x < rect.x) ||
        (x > rect.x + rect.width) ||
        (y < rect.y) ||
        (y > rect.y + rect.height)
    );
}