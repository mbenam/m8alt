#include "display.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
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
    void *render_buffer; // Void pointer to support 16 or 32 bit dynamically
    int offset_x;
    int offset_y;
    int bpp;             // Bytes per pixel (2 or 4)
    int stride;          // Buffer stride in bytes (width * bpp)
} Framebuffer;

static Framebuffer g_fb;
static int current_font_idx = 0;

// Track background color (stored in native format)
static uint32_t global_bg_color = 0; 
static int prev_waveform_size = 0;

// --- Optimization: Dirty Rectangle Tracking ---
static int dirty_min_x = M8_WIDTH;
static int dirty_min_y = M8_HEIGHT;
static int dirty_max_x = -1;
static int dirty_max_y = -1;

// --- Helper Functions ---

static inline void mark_dirty(int x, int y, int w, int h) {
    // Padding ensures artifacts (like text tails or offset mismatches) are cleared
    int pad_x = 2;
    int pad_y = 6; 

    int nx = x - pad_x;
    int ny = y - pad_y;
    int nw = w + (pad_x * 2);
    int nh = h + (pad_y * 2);

    if (nx < dirty_min_x) dirty_min_x = nx;
    if (ny < dirty_min_y) dirty_min_y = ny;
    if (nx + nw > dirty_max_x) dirty_max_x = nx + nw;
    if (ny + nh > dirty_max_y) dirty_max_y = ny + nh;
    
    // Clamp to screen bounds
    if (dirty_min_x < 0) dirty_min_x = 0;
    if (dirty_min_y < 0) dirty_min_y = 0;
    if (dirty_max_x > M8_WIDTH) dirty_max_x = M8_WIDTH;
    if (dirty_max_y > M8_HEIGHT) dirty_max_y = M8_HEIGHT;
}

// Convert M8 RGB (8-8-8) to Native Format (16 or 32)
static inline uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b) {
    if (g_fb.bpp == 4) {
        // ARGB8888
        return (0xFF << 24) | (r << 16) | (g << 8) | b;
    } else {
        // RGB565
        return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }
}

static const struct inline_font* get_font_ptr(int idx) {
    if(idx == 0) return &font_v1_small;
    if(idx == 1) return &font_v1_large;
    if(idx == 2) return &font_v2_small;
    if(idx == 3) return &font_v2_large;
    return &font_v2_huge;
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

    // Detect pixel depth
    g_fb.bpp = g_fb.vinfo.bits_per_pixel / 8;
    if (g_fb.bpp != 2 && g_fb.bpp != 4) {
        // Fallback for uncommon depths (e.g. 24bit), treat as 32 for buffer allocation
        g_fb.bpp = 4; 
    }
    g_fb.stride = M8_WIDTH * g_fb.bpp;

    long screensize = g_fb.vinfo.yres_virtual * g_fb.finfo.line_length;
    g_fb.fb_mem = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, g_fb.fb_fd, 0);
    if (g_fb.fb_mem == MAP_FAILED) exit(4);

    // Allocate buffer in NATIVE size
    g_fb.render_buffer = malloc(M8_WIDTH * M8_HEIGHT * g_fb.bpp);
    memset(g_fb.render_buffer, 0, M8_WIDTH * M8_HEIGHT * g_fb.bpp);

    g_fb.offset_x = (g_fb.vinfo.xres - M8_WIDTH) / 2;
    g_fb.offset_y = (g_fb.vinfo.yres - M8_HEIGHT) / 2;
    if(g_fb.offset_x < 0) g_fb.offset_x = 0;
    if(g_fb.offset_y < 0) g_fb.offset_y = 0;
    
    // Set default black in native format
    global_bg_color = pack_color(0, 0, 0);

    // Force full redraw on init
    dirty_min_x = 0;
    dirty_min_y = 0;
    dirty_max_x = M8_WIDTH;
    dirty_max_y = M8_HEIGHT;
    
    printf("\033[?25l"); // Hide cursor
    fflush(stdout);
}

void display_close(void) {
    if (g_fb.render_buffer) free(g_fb.render_buffer);
    if (g_fb.fb_fd != -1) close(g_fb.fb_fd);
    printf("\033[?25h"); // Show cursor
}

void display_blit(void) {
    if (dirty_min_x >= dirty_max_x || dirty_min_y >= dirty_max_y) return;

    // Prevent tearing
    int dummy = 0;
    ioctl(g_fb.fb_fd, FBIO_WAITFORVSYNC, &dummy);

    int fb_stride = g_fb.finfo.line_length;
    int dst_x_offset_bytes = (g_fb.offset_x * g_fb.bpp);

    // OPTIMIZATION: Render buffer is native format -> simple memcpy
    for (int y = dirty_min_y; y < dirty_max_y; y++) {
        if ((y + g_fb.offset_y) >= g_fb.vinfo.yres) break;

        uint8_t* src_row = (uint8_t*)g_fb.render_buffer + (y * g_fb.stride);
        uint8_t* dst_row = (uint8_t*)g_fb.fb_mem + 
                           ((y + g_fb.offset_y) * fb_stride) + 
                           dst_x_offset_bytes;

        int start_offset = dirty_min_x * g_fb.bpp;
        int copy_size = (dirty_max_x - dirty_min_x) * g_fb.bpp;

        memcpy(dst_row + start_offset, src_row + start_offset, copy_size);
    }

    dirty_min_x = M8_WIDTH;
    dirty_min_y = M8_HEIGHT;
    dirty_max_x = -1;
    dirty_max_y = -1;
}

void display_draw_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    const struct inline_font* font = get_font_ptr(current_font_idx);
    
    // Pre-calculate native color
    uint32_t color = pack_color(r, g, b);
    
    // Check for Full Screen / Background Clear
    if (w >= M8_WIDTH && h >= M8_HEIGHT) {
        global_bg_color = color;
        // Fast clear
        if (color == 0 || (g_fb.bpp == 4 && color == 0xFF000000)) {
            memset(g_fb.render_buffer, 0, M8_WIDTH * M8_HEIGHT * g_fb.bpp);
        } else {
            // Manual fill
            if (g_fb.bpp == 4) {
                uint32_t *ptr = (uint32_t*)g_fb.render_buffer;
                int total = M8_WIDTH * M8_HEIGHT;
                while(total--) *ptr++ = color;
            } else {
                uint16_t *ptr = (uint16_t*)g_fb.render_buffer;
                int total = M8_WIDTH * M8_HEIGHT;
                while(total--) *ptr++ = (uint16_t)color;
            }
        }
        dirty_min_x = 0; dirty_min_y = 0;
        dirty_max_x = M8_WIDTH; dirty_max_y = M8_HEIGHT;
        return;
    } 
    
    // Absolute vs Relative logic
    bool is_absolute = (r == 0 && g == 0 && b == 0) || (color == global_bg_color);
    if (!is_absolute) y += font->screen_offset_y;

    // Clipping
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > M8_WIDTH) w = M8_WIDTH - x;
    if (y + h > M8_HEIGHT) h = M8_HEIGHT - y;
    
    if (w <= 0 || h <= 0) return;

    mark_dirty(x, y, w, h);

    // Drawing loops
    if (g_fb.bpp == 4) {
        uint32_t c = (uint32_t)color;
        for (int j = 0; j < h; j++) {
            uint32_t* row_ptr = (uint32_t*)g_fb.render_buffer + ((y + j) * M8_WIDTH) + x;
            int cols = w;
            while(cols--) *row_ptr++ = c;
        }
    } else {
        uint16_t c = (uint16_t)color;
        for (int j = 0; j < h; j++) {
            uint16_t* row_ptr = (uint16_t*)g_fb.render_buffer + ((y + j) * M8_WIDTH) + x;
            int cols = w;
            while(cols--) *row_ptr++ = c;
        }
    }
}

void display_draw_char(char c, int x, int y, uint8_t fr, uint8_t fg, uint8_t fb, uint8_t br, uint8_t bg, uint8_t bb) {
    const struct inline_font* font = get_font_ptr(current_font_idx);
    
    y += font->text_offset_y + font->screen_offset_y;

    uint32_t fore = pack_color(fr, fg, fb);
    uint32_t back = pack_color(br, bg, bb);

    int w = font->glyph_x;
    int h = font->glyph_y;

    int draw_x = x, draw_y = y;
    int draw_w = w, draw_h = h;
    int img_off_x = 0, img_off_y = 0;

    // Clipping
    if (draw_x < 0) { draw_w += draw_x; img_off_x = -draw_x; draw_x = 0; }
    if (draw_y < 0) { draw_h += draw_y; img_off_y = -draw_y; draw_y = 0; }
    if (draw_x + draw_w > M8_WIDTH) draw_w = M8_WIDTH - draw_x;
    if (draw_y + draw_h > M8_HEIGHT) draw_h = M8_HEIGHT - draw_y;

    if (draw_w <= 0 || draw_h <= 0) return;

    mark_dirty(draw_x, draw_y, draw_w, draw_h);

    // Handle Space
    if (c == 32) {
        if (g_fb.bpp == 4) {
            for(int j = 0; j < draw_h; j++) {
                uint32_t* buf = (uint32_t*)g_fb.render_buffer + ((draw_y + j) * M8_WIDTH) + draw_x;
                int k = draw_w; while(k--) *buf++ = back;
            }
        } else {
            for(int j = 0; j < draw_h; j++) {
                uint16_t* buf = (uint16_t*)g_fb.render_buffer + ((draw_y + j) * M8_WIDTH) + draw_x;
                int k = draw_w; while(k--) *buf++ = (uint16_t)back;
            }
        }
        return;
    }

    int char_idx = c - 33; 
    if (char_idx < 0) return; 

    int32_t bmp_w = *(int32_t*)&font->image_data[18];
    int32_t bmp_h = *(int32_t*)&font->image_data[22];
    uint32_t data_offset = *(uint32_t*)&font->image_data[10];
    
    int row_stride = ((bmp_w + 31) / 32) * 4;
    int chars_per_row = 94; 
    int src_base_x = (char_idx % chars_per_row) * (bmp_w / chars_per_row);
    int src_base_y = (char_idx / chars_per_row) * font->height;

    // Split loop to avoid 'if(bpp)' inside pixel iteration
    if (g_fb.bpp == 4) {
        for(int j = 0; j < draw_h; j++) {
            int buf_y = draw_y + j;
            int src_y_local = src_base_y + j + img_off_y;
            int bmp_row = (bmp_h - 1) - src_y_local;
            const uint8_t* bmp_row_data = &font->image_data[data_offset + (bmp_row * row_stride)];
            
            uint32_t* buf_ptr = (uint32_t*)g_fb.render_buffer + (buf_y * M8_WIDTH) + draw_x;
            int start_x = src_base_x + img_off_x;

            for(int i = 0; i < draw_w; i++) {
                int cur_x = start_x + i;
                // Manual bit extraction
                uint8_t byte = bmp_row_data[cur_x >> 3];
                uint8_t pixel = (byte >> (7 - (cur_x & 7))) & 1;
                if(pixel) buf_ptr[i] = fore;
                else if (fore != back) buf_ptr[i] = back;
            }
        }
    } else {
        // 16-bit Path
        uint16_t f16 = (uint16_t)fore;
        uint16_t b16 = (uint16_t)back;
        for(int j = 0; j < draw_h; j++) {
            int buf_y = draw_y + j;
            int src_y_local = src_base_y + j + img_off_y;
            int bmp_row = (bmp_h - 1) - src_y_local;
            const uint8_t* bmp_row_data = &font->image_data[data_offset + (bmp_row * row_stride)];
            
            uint16_t* buf_ptr = (uint16_t*)g_fb.render_buffer + (buf_y * M8_WIDTH) + draw_x;
            int start_x = src_base_x + img_off_x;

            for(int i = 0; i < draw_w; i++) {
                int cur_x = start_x + i;
                uint8_t byte = bmp_row_data[cur_x >> 3];
                uint8_t pixel = (byte >> (7 - (cur_x & 7))) & 1;
                if(pixel) buf_ptr[i] = f16;
                else if (f16 != b16) buf_ptr[i] = b16;
            }
        }
    }
}

void display_draw_waveform(uint8_t r, uint8_t g, uint8_t b, uint8_t* data, int size) {
    uint32_t color = pack_color(r, g, b);
    const struct inline_font* font = get_font_ptr(current_font_idx);
    int max_h = font->waveform_max_height;

    int clear_w = (size > 0) ? size : prev_waveform_size;
    int clear_x = M8_WIDTH - clear_w;
    
    mark_dirty(clear_x, 0, clear_w, max_h + 1);

    // Clear previous area
    if (g_fb.bpp == 4) {
        for(int j=0; j <= max_h; j++) {
            uint32_t* row = (uint32_t*)g_fb.render_buffer + (j * M8_WIDTH) + clear_x;
            for(int i=0; i < clear_w; i++) {
                if(clear_x + i < M8_WIDTH) row[i] = global_bg_color;
            }
        }
    } else {
        uint16_t bg16 = (uint16_t)global_bg_color;
        for(int j=0; j <= max_h; j++) {
            uint16_t* row = (uint16_t*)g_fb.render_buffer + (j * M8_WIDTH) + clear_x;
            for(int i=0; i < clear_w; i++) {
                if(clear_x + i < M8_WIDTH) row[i] = bg16;
            }
        }
    }

    prev_waveform_size = size;
    if (size == 0) return;

    mark_dirty(M8_WIDTH - size, 0, size, 255);

    int prev_x = M8_WIDTH - size;
    int prev_y = data[0];
    
    for(int i=1; i<size; i++) {
        int x = (M8_WIDTH - size) + i;
        int y = data[i];
        if(y > max_h) y = max_h;
        
        int x0=prev_x, y0=prev_y, x1=x, y1=y;
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy, e2;

        while (1) {
            if (x0 >= 0 && x0 < M8_WIDTH && y0 >= 0 && y0 < M8_HEIGHT) {
                if (g_fb.bpp == 4) {
                    ((uint32_t*)g_fb.render_buffer)[y0 * M8_WIDTH + x0] = color;
                } else {
                    ((uint16_t*)g_fb.render_buffer)[y0 * M8_WIDTH + x0] = (uint16_t)color;
                }
            }
            if (x0 == x1 && y0 == y1) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
        prev_x = x;
        prev_y = y;
    }
}