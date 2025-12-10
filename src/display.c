#include "display.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

// Include Fonts
#include "fonts/font1.h"
#include "fonts/font2.h"
#include "fonts/font3.h"
#include "fonts/font4.h"
#include "fonts/font5.h"

typedef struct {
    int fb_fd;
    void *fb_mem;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    uint32_t *render_buffer;
    int offset_x;
    int offset_y;
} Framebuffer;

// Renamed to g_fb to avoid shadowing
static Framebuffer g_fb;
static int current_font_idx = 0;

// Track background color to clear the waveform area
static uint32_t global_bg_color = 0xFF000000; 
static int prev_waveform_size = 0;

// --- Helper Functions ---

static const struct inline_font* get_font_ptr(int idx) {
    if(idx == 0) return &font_v1_small;
    if(idx == 1) return &font_v1_large;
    if(idx == 2) return &font_v2_small;
    if(idx == 3) return &font_v2_large;
    return &font_v2_huge;
}

static uint32_t get_bmp_pixel(const unsigned char* data, int x, int y) {
    if (!data) return 0;
    int32_t w = *(int32_t*)&data[18];
    int32_t h = *(int32_t*)&data[22];
    uint32_t data_offset = *(uint32_t*)&data[10];
    
    if (x < 0 || x >= w || y < 0 || y >= h) return 0;

    int row = (h - 1) - y;
    int row_size = ((w + 31) / 32) * 4;
    int byte_idx = data_offset + (row * row_size) + (x / 8);
    int bit_idx = 7 - (x % 8);
    
    return (data[byte_idx] >> bit_idx) & 1;
}

static void set_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= M8_WIDTH || y < 0 || y >= M8_HEIGHT) return;
    g_fb.render_buffer[y * M8_WIDTH + x] = color;
}

// --- Public Interface ---

void display_set_font(int font_index) {
    current_font_idx = font_index;
}

void display_init(void) {
    g_fb.fb_fd = open(app_config.fb_path, O_RDWR);
    if (g_fb.fb_fd == -1) { 
        fprintf(stderr, "Display Error: cannot open %s\n", app_config.fb_path); 
        exit(1); 
    }

    if (ioctl(g_fb.fb_fd, FBIOGET_FSCREENINFO, &g_fb.finfo) == -1) exit(2);
    if (ioctl(g_fb.fb_fd, FBIOGET_VSCREENINFO, &g_fb.vinfo) == -1) exit(3);

    long screensize = g_fb.vinfo.yres_virtual * g_fb.finfo.line_length;
    g_fb.fb_mem = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, g_fb.fb_fd, 0);
    if (g_fb.fb_mem == MAP_FAILED) exit(4);

    g_fb.render_buffer = malloc(M8_WIDTH * M8_HEIGHT * sizeof(uint32_t));
    memset(g_fb.render_buffer, 0, M8_WIDTH * M8_HEIGHT * sizeof(uint32_t));

    g_fb.offset_x = (g_fb.vinfo.xres - M8_WIDTH) / 2;
    g_fb.offset_y = (g_fb.vinfo.yres - M8_HEIGHT) / 2;
    if(g_fb.offset_x < 0) g_fb.offset_x = 0;
    if(g_fb.offset_y < 0) g_fb.offset_y = 0;
    
    printf("\033[?25l"); // Hide cursor
    fflush(stdout);
}

void display_close(void) {
    if (g_fb.render_buffer) free(g_fb.render_buffer);
    if (g_fb.fb_fd != -1) close(g_fb.fb_fd);
    printf("\033[?25h"); // Show cursor
}

void display_blit(void) {
    int bpp = g_fb.vinfo.bits_per_pixel;
    for (int y = 0; y < M8_HEIGHT; y++) {
        if((y + g_fb.offset_y) >= g_fb.vinfo.yres) break;
        
        uint32_t* src_row = g_fb.render_buffer + (y * M8_WIDTH);
        uint8_t* dst_ptr = (uint8_t*)g_fb.fb_mem + ((y + g_fb.offset_y) * g_fb.finfo.line_length) + (g_fb.offset_x * (bpp/8));

        if (bpp == 32) {
            memcpy(dst_ptr, src_row, M8_WIDTH * 4);
        } else if (bpp == 16) {
            uint16_t* dst_16 = (uint16_t*)dst_ptr;
            for (int x = 0; x < M8_WIDTH; x++) {
                uint32_t c = src_row[x];
                uint8_t r = (c >> 16) & 0xFF;
                uint8_t g = (c >> 8) & 0xFF;
                uint8_t b = c & 0xFF;
                dst_16[x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            }
        }
    }
}

void display_draw_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    const struct inline_font* font = get_font_ptr(current_font_idx);
    uint32_t color = (0xFF << 24) | (r << 16) | (g << 8) | b;
    
    // SPECIAL CASE: Full Screen Clear
    // If the M8 tries to clear the whole screen, we MUST ignore the font offset
    // and wipe the entire physical buffer.
    if (x == 0 && y <= 0 && w >= M8_WIDTH && h >= M8_HEIGHT) {
        global_bg_color = color;
        y = 0;
        h = M8_HEIGHT;
        w = M8_WIDTH;
    } else {
        // Normal UI Element: Apply screen offset to align rects with text
        // This fixes the "rainbow bar" smearing and ghosts
        y += font->screen_offset_y;
    }

    // Clipping
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > M8_WIDTH) w = M8_WIDTH - x;
    if (y + h > M8_HEIGHT) h = M8_HEIGHT - y;

    for (int j = 0; j < h; j++) {
        int idx = (y + j) * M8_WIDTH + x;
        for (int i = 0; i < w; i++) {
            g_fb.render_buffer[idx + i] = color;
        }
    }
}

void display_draw_char(char c, int x, int y, uint8_t fr, uint8_t fg, uint8_t fb, uint8_t br, uint8_t bg, uint8_t bb) {
    const struct inline_font* font = get_font_ptr(current_font_idx);
    
    y += font->text_offset_y + font->screen_offset_y;

    uint32_t fore = (0xFF << 24) | (fr << 16) | (fg << 8) | fb;
    uint32_t back = (0xFF << 24) | (br << 16) | (bg << 8) | bb;
    
    // Clear the character background
    if (fore != back) {
        int bg_x = x;
        int bg_y = y; 
        int bg_w = font->glyph_x;
        int bg_h = font->glyph_y;
        
        // Inline clipping
        if (bg_x < 0) { bg_w += bg_x; bg_x = 0; }
        if (bg_y < 0) { bg_h += bg_y; bg_y = 0; }
        if (bg_x + bg_w > M8_WIDTH) bg_w = M8_WIDTH - bg_x;
        if (bg_y + bg_h > M8_HEIGHT) bg_h = M8_HEIGHT - bg_y;

        for (int j = 0; j < bg_h; j++) {
            int idx = (bg_y + j) * M8_WIDTH + bg_x;
            for (int i = 0; i < bg_w; i++) {
                g_fb.render_buffer[idx + i] = back;
            }
        }
    }

    int chars_per_row = 94; 
    int char_idx = c - 33; // Offset for M8 Fonts starting at '!'

    if (char_idx < 0) return;

    int32_t bmp_w = *(int32_t*)&font->image_data[18];
    int src_x = (char_idx % chars_per_row) * (bmp_w / chars_per_row);
    int src_y = (char_idx / chars_per_row) * font->height;

    for(int j=0; j<font->glyph_y; j++) {
        for(int i=0; i<font->glyph_x; i++) {
            if(get_bmp_pixel(font->image_data, src_x + i, src_y + j)) {
                 set_pixel(x + i, y + j, fore);
            }
        }
    }
}

void display_draw_waveform(uint8_t r, uint8_t g, uint8_t b, uint8_t* data, int size) {
    uint32_t color = (0xFF << 24) | (r << 16) | (g << 8) | b;
    const struct inline_font* font = get_font_ptr(current_font_idx);
    int max_h = font->waveform_max_height;

    // 1. Clear the area used by the previous waveform
    // Using raw loop here because waveform is always at the absolute top (no font offsets)
    int clear_w = (size > 0) ? size : prev_waveform_size;
    int clear_x = M8_WIDTH - clear_w;
    
    for(int j=0; j <= max_h; j++) {
        int idx = j * M8_WIDTH + clear_x;
        for(int i=0; i < clear_w; i++) {
            if(clear_x + i < M8_WIDTH)
                g_fb.render_buffer[idx + i] = global_bg_color;
        }
    }

    prev_waveform_size = size;

    if (size == 0) return;

    // 2. Draw new waveform
    int prev_x = M8_WIDTH - size;
    int prev_y = data[0];
    
    for(int i=1; i<size; i++) {
        int x = (M8_WIDTH - size) + i;
        int y = data[i];
        if(y > max_h) y = max_h;
        
        // Bresenham line
        int x0=prev_x, y0=prev_y, x1=x, y1=y;
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy, e2;

        while (1) {
            set_pixel(x0, y0, color);
            if (x0 == x1 && y0 == y1) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
        prev_x = x;
        prev_y = y;
    }
}