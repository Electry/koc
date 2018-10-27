#include <vitasdk.h>
#include "font_sun12x22.h"

#define MAX_STRING_LENGTH 512

static SceDisplayFrameBuf frameBuf;
uint8_t fontScale = 1;
uint32_t colorFg = 0xFFFFFFFF;
uint32_t colorBg = 0xFF000000;

void updateFrameBuf(const SceDisplayFrameBuf *pParam) {
    memcpy(&frameBuf, pParam, sizeof(SceDisplayFrameBuf));
}

void setTextColor(uint8_t r, uint8_t g, uint8_t b) {
    colorFg = (0xFF << 24) + (b << 16) + (g << 8) + r;
}
void setBgColor(uint8_t r, uint8_t g, uint8_t b, uint8_t transparent) {
    colorBg = (transparent ? 0 : (0xFF << 24)) + (b << 16) + (g << 8) + r;
}

void setTextScale(uint8_t scale) {
    fontScale = scale;
}

void drawCharacter(int character, int x, int y) {	
    for (int yy = 0; yy < FONT_HEIGHT; yy++) {
        int xDisplacement = x;
        int yDisplacement = (y + (yy * fontScale)) * frameBuf.pitch;
        uint32_t *screenPos = (uint32_t *)frameBuf.base + xDisplacement + yDisplacement;

        for (int xx = 0; xx < FONT_WIDTH; xx++) {
            uint32_t charPos = character * (FONT_HEIGHT * ((FONT_WIDTH / 8) + 1));
            uint32_t charPosH = charPos + (yy * ((FONT_WIDTH / 8) + 1));
            uint8_t charByte = font[charPosH + (xx / 8)];
            uint32_t clr = ((charByte >> (7 - (xx % 8))) & 1) ? colorFg : colorBg;
            if (!(clr & (0xFF << 24)))
                continue;

            for (int sx = 0; sx < fontScale; sx++) {
                for (int sy = 0; sy < fontScale; sy++) {
                     *(screenPos + (frameBuf.pitch * sy) + sx) = clr;
                }
            }

            screenPos += fontScale;
        }
    }
}

void drawString(int x, int y, const char *str) {
    for (size_t i = 0; i < strlen(str); i++)
        drawCharacter(str[i], x + i * FONT_WIDTH * fontScale, y);
}

void drawStringF(int x, int y, const char *format, ...) {
    char buffer[MAX_STRING_LENGTH] = { 0 };
    va_list va;

    va_start(va, format);
    vsnprintf(buffer, MAX_STRING_LENGTH, format, va);
    va_end(va);

    drawString(x, y, buffer);
}

