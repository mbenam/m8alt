#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

void display_init(void);
void display_close(void);
void display_blit(void);

// Drawing primitives called by the Serial module
void display_draw_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
void display_draw_char(char c, int x, int y, uint8_t fr, uint8_t fg, uint8_t fb, uint8_t br, uint8_t bg, uint8_t bb);
void display_draw_waveform(uint8_t r, uint8_t g, uint8_t b, uint8_t* data, int size);
void display_set_font(int font_index);

#endif