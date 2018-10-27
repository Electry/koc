#ifndef _DISPLAY_H_
#define _DISPLAY_H_

void updateFrameBuf(const SceDisplayFrameBuf *param);
void setBgColor(uint8_t r, uint8_t g, uint8_t b);
void setTextColor(uint8_t r, uint8_t g, uint8_t b);
void setTextScale(uint8_t scale);

void drawCharacter(int character, int x, int y);
void drawString(int x, int y, const char *str);
void drawStringF(int x, int y, const char *format, ...);

#endif
