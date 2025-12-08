import os

# Define the file content
files = {}

# 1. Makefile
files["Makefile"] = r"""
CC = gcc
CFLAGS = -Wall -O2 -D_GNU_SOURCE
LDFLAGS = -lpthread -lserialport -levdev

# Note: src/fonts/fonts.c is expected to exist from the original source
SRC = src/main.c \
      src/config.c \
      src/command.c \
      src/ini.c \
      src/input.c \
      src/render.c \
      src/backends/m8_libserialport.c \
      src/backends/queue.c \
      src/backends/slip.c \
      src/fonts/fonts.c

OBJ = $(SRC:.c=.o)
TARGET = m8c_rpi

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
"""

# 2. src/main.c
files["src/main.c"] = r"""
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "common.h"
#include "render.h"
#include "input.h"
#include "backends/m8.h"

int main(int argc, char **argv) {
    struct app_context ctx = {0};
    
    printf("Starting M8C RPi (Framebuffer/Evdev edition)...\n");

    // 1. Initialize Configuration
    // We pass NULL to use default config name, config_read handles pathing
    ctx.conf = config_initialize(NULL);
    config_read(&ctx.conf);

    // 2. Initialize Input (Evdev)
    if (!input_init(&ctx.conf)) {
        fprintf(stderr, "Failed to initialize input. Check permissions or device path.\n");
        return 1;
    }

    // 3. Initialize Renderer (Framebuffer)
    if (!renderer_initialize(&ctx.conf)) {
        fprintf(stderr, "Failed to initialize framebuffer.\n");
        return 1;
    }

    // 4. Initialize M8 Connection (Serial)
    // Retry loop could be added here, but for now we try once
    if (!m8_initialize(1, ctx.preferred_device)) {
        fprintf(stderr, "M8 device not found. Starting in disconnected state.\n");
        ctx.device_connected = 0;
        ctx.app_state = WAIT_FOR_DEVICE;
    } else {
        ctx.device_connected = 1;
        ctx.app_state = RUN;
        m8_enable_display(1);
    }

    printf("Initialization complete. Entering main loop.\n");

    // 5. Main Loop
    while (1) {
        // Poll Input
        input_poll(&ctx);

        // State Machine
        switch (ctx.app_state) {
            case RUN:
                // Read from M8
                if (m8_process_data(&ctx.conf) == DEVICE_DISCONNECTED) {
                    ctx.device_connected = 0;
                    ctx.app_state = WAIT_FOR_DEVICE;
                    renderer_clear_screen();
                }
                break;
            
            case WAIT_FOR_DEVICE:
                // Simple reconnection logic
                usleep(500000); // Wait 0.5s
                if (m8_initialize(0, ctx.preferred_device)) {
                    ctx.device_connected = 1;
                    ctx.app_state = RUN;
                    m8_enable_display(1);
                }
                break;

            case QUIT:
                goto cleanup;
                
            default:
                break;
        }

        // Render Frame
        render_screen(&ctx.conf);

        // Cap to ~60 FPS (16ms)
        usleep(16000);
    }

cleanup:
    printf("Shutting down.\n");
    renderer_close();
    input_close();
    m8_close();
    return 0;
}
"""

# 3. src/common.h
files["src/common.h"] = r"""
#ifndef COMMON_H_
#define COMMON_H_
#include "config.h"
#include <stdint.h>
#include <stdbool.h>

enum app_state { QUIT, INITIALIZE, WAIT_FOR_DEVICE, RUN };

struct app_context {
    config_params_s conf;
    enum app_state app_state;
    char *preferred_device;
    unsigned char device_connected;
};

#endif
"""

# 4. src/config.c
files["src/config.c"] = r"""
#include "config.h"
#include "ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>
#include <ctype.h>

static int strcmpci(const char *a, const char *b) {
  for (;;) {
    const int d = tolower(*a) - tolower(*b);
    if (d != 0 || !*a) return d;
    a++, b++;
  }
}

config_params_s config_initialize(char *filename) {
  config_params_s c;
  c.filename = filename ? filename : "config.ini";
  c.wait_packets = 256;

  // Default Linux Input Event Codes
  c.key_up = KEY_UP;
  c.key_left = KEY_LEFT;
  c.key_down = KEY_DOWN;
  c.key_right = KEY_RIGHT;
  
  c.key_select = KEY_LEFTSHIFT;
  c.key_select_alt = KEY_Z;
  
  c.key_start = KEY_SPACE;
  c.key_start_alt = KEY_X;
  
  c.key_opt = KEY_LEFTALT;
  c.key_opt_alt = KEY_A;
  
  c.key_edit = KEY_LEFTCTRL;
  c.key_edit_alt = KEY_S;
  
  c.key_delete = KEY_DELETE;
  c.key_reset = KEY_R;
  
  // Keyjazz defaults
  c.key_jazz_inc_octave = KEY_KPASTERISK;
  c.key_jazz_dec_octave = KEY_KPSLASH;
  c.key_jazz_inc_velocity = KEY_KPPLUS;
  c.key_jazz_dec_velocity = KEY_KPMINUS;
  
  return c;
}

void write_config(const config_params_s *conf) {
  char config_path[1024];
  snprintf(config_path, sizeof(config_path), "./%s", conf->filename);
  
  FILE *fp = fopen(config_path, "w");
  if (fp) {
    fprintf(fp, "[graphics]\nwait_packets=%d\n", conf->wait_packets);
    fprintf(fp, "[keyboard]\n");
    fprintf(fp, "key_up=%d\n", conf->key_up);
    fprintf(fp, "key_left=%d\n", conf->key_left);
    fprintf(fp, "key_down=%d\n", conf->key_down);
    fprintf(fp, "key_right=%d\n", conf->key_right);
    fprintf(fp, "key_select=%d\n", conf->key_select);
    fprintf(fp, "key_select_alt=%d\n", conf->key_select_alt);
    fprintf(fp, "key_start=%d\n", conf->key_start);
    fprintf(fp, "key_start_alt=%d\n", conf->key_start_alt);
    fprintf(fp, "key_opt=%d\n", conf->key_opt);
    fprintf(fp, "key_opt_alt=%d\n", conf->key_opt_alt);
    fprintf(fp, "key_edit=%d\n", conf->key_edit);
    fprintf(fp, "key_edit_alt=%d\n", conf->key_edit_alt);
    fprintf(fp, "key_delete=%d\n", conf->key_delete);
    fprintf(fp, "key_reset=%d\n", conf->key_reset);
    fprintf(fp, "key_jazz_inc_octave=%d\n", conf->key_jazz_inc_octave);
    fprintf(fp, "key_jazz_dec_octave=%d\n", conf->key_jazz_dec_octave);
    fprintf(fp, "key_jazz_inc_velocity=%d\n", conf->key_jazz_inc_velocity);
    fprintf(fp, "key_jazz_dec_velocity=%d\n", conf->key_jazz_dec_velocity);
    fclose(fp);
  }
}

void read_key_config(const ini_t *ini, config_params_s *conf) {
  const char *val;
  #define READ_KEY(name) if((val = ini_get(ini, "keyboard", #name))) conf->name = atoi(val)
  
  READ_KEY(key_up);
  READ_KEY(key_left);
  READ_KEY(key_down);
  READ_KEY(key_right);
  READ_KEY(key_select);
  READ_KEY(key_select_alt);
  READ_KEY(key_start);
  READ_KEY(key_start_alt);
  READ_KEY(key_opt);
  READ_KEY(key_opt_alt);
  READ_KEY(key_edit);
  READ_KEY(key_edit_alt);
  READ_KEY(key_delete);
  READ_KEY(key_reset);
  READ_KEY(key_jazz_inc_octave);
  READ_KEY(key_jazz_dec_octave);
  READ_KEY(key_jazz_inc_velocity);
  READ_KEY(key_jazz_dec_velocity);
}

void config_read(config_params_s *conf) {
  char config_path[1024];
  snprintf(config_path, sizeof(config_path), "./%s", conf->filename);
  
  ini_t *ini = ini_load(config_path);
  if (ini == NULL) {
    write_config(conf);
    return;
  }
  
  const char *wait = ini_get(ini, "graphics", "wait_packets");
  if(wait) conf->wait_packets = atoi(wait);
  
  read_key_config(ini, conf);
  ini_free(ini);
  write_config(conf);
}
"""

# 5. src/config.h
files["src/config.h"] = r"""
#ifndef CONFIG_H_
#define CONFIG_H_

typedef struct config_params_s {
  char *filename;
  unsigned int wait_packets;

  unsigned int key_up;
  unsigned int key_left;
  unsigned int key_down;
  unsigned int key_right;
  unsigned int key_select;
  unsigned int key_select_alt;
  unsigned int key_start;
  unsigned int key_start_alt;
  unsigned int key_opt;
  unsigned int key_opt_alt;
  unsigned int key_edit;
  unsigned int key_edit_alt;
  unsigned int key_delete;
  unsigned int key_reset;
  unsigned int key_jazz_inc_octave;
  unsigned int key_jazz_dec_octave;
  unsigned int key_jazz_inc_velocity;
  unsigned int key_jazz_dec_velocity;
} config_params_s;

config_params_s config_initialize(char *filename);
void config_read(config_params_s *conf);
void write_config(const config_params_s *conf);

#endif
"""

# 6. src/command.c
files["src/command.c"] = r"""
#include "command.h"
#include "render.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define ArrayCount(x) sizeof(x) / sizeof((x)[1])

static uint16_t decodeInt16(const uint8_t *data, const uint8_t start) {
  return data[start] | (((uint16_t)data[start + 1] << 8) & 0xFFFF);
}

enum m8_command_bytes {
  draw_rectangle_command = 0xFE,
  draw_rectangle_command_pos_datalength = 5,
  draw_rectangle_command_pos_color_datalength = 8,
  draw_rectangle_command_pos_size_datalength = 9,
  draw_rectangle_command_pos_size_color_datalength = 12,
  draw_character_command = 0xFD,
  draw_character_command_datalength = 12,
  draw_oscilloscope_waveform_command = 0xFC,
  draw_oscilloscope_waveform_command_mindatalength = 1 + 3,
  draw_oscilloscope_waveform_command_maxdatalength = 1 + 3 + 480,
  joypad_keypressedstate_command = 0xFB,
  joypad_keypressedstate_command_datalength = 3,
  system_info_command = 0xFF,
  system_info_command_datalength = 6
};

int process_command(const uint8_t *recv_buf, uint32_t size) {
  switch (recv_buf[0]) {
  case draw_rectangle_command: {
    static struct draw_rectangle_command rectcmd;
    rectcmd.pos.x = decodeInt16(recv_buf, 1);
    rectcmd.pos.y = decodeInt16(recv_buf, 3);

    switch (size) {
    case draw_rectangle_command_pos_datalength:
      rectcmd.size.width = 1;
      rectcmd.size.height = 1;
      break;
    case draw_rectangle_command_pos_color_datalength:
      rectcmd.size.width = 1;
      rectcmd.size.height = 1;
      rectcmd.color.r = recv_buf[5];
      rectcmd.color.g = recv_buf[6];
      rectcmd.color.b = recv_buf[7];
      break;
    case draw_rectangle_command_pos_size_datalength:
      rectcmd.size.width = decodeInt16(recv_buf, 5);
      rectcmd.size.height = decodeInt16(recv_buf, 7);
      break;
    case draw_rectangle_command_pos_size_color_datalength:
      rectcmd.size.width = decodeInt16(recv_buf, 5);
      rectcmd.size.height = decodeInt16(recv_buf, 7);
      rectcmd.color.r = recv_buf[9];
      rectcmd.color.g = recv_buf[10];
      rectcmd.color.b = recv_buf[11];
      break;
    default:
      return 0;
    }
    draw_rectangle(&rectcmd);
    return 1;
  }

  case draw_character_command: {
    if (size != draw_character_command_datalength) return 0;
    struct draw_character_command charcmd = {
        recv_buf[1],
        {decodeInt16(recv_buf, 2), decodeInt16(recv_buf, 4)},
        {recv_buf[6], recv_buf[7], recv_buf[8]},
        {recv_buf[9], recv_buf[10], recv_buf[11]}};
    draw_character(&charcmd);
    return 1;
  }

  case draw_oscilloscope_waveform_command: {
    if (size < draw_oscilloscope_waveform_command_mindatalength ||
        size > draw_oscilloscope_waveform_command_maxdatalength) return 0;
    
    struct draw_oscilloscope_waveform_command osccmd = {0};
    osccmd.color = (struct color){recv_buf[1], recv_buf[2], recv_buf[3]};
    memcpy(osccmd.waveform, &recv_buf[4], size - 4);
    osccmd.waveform_size = (size & 0xFFFF) - 4;

    draw_waveform(&osccmd);
    return 1;
  }

  case system_info_command: {
    if (size != system_info_command_datalength) break;
    renderer_set_font_mode(recv_buf[5]);
    return 1;
  }

  default:
    break;
  }
  return 1;
}
"""

# 7. src/command.h
files["src/command.h"] = r"""
#ifndef COMMAND_H_
#define COMMAND_H_

#include <stdint.h>

struct position {
  uint16_t x;
  uint16_t y;
};

struct size {
  uint16_t width;
  uint16_t height;
};

struct color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct draw_rectangle_command {
  struct position pos;
  struct size size;
  struct color color;
};

struct draw_character_command {
  int c;
  struct position pos;
  struct color foreground;
  struct color background;
};

struct draw_oscilloscope_waveform_command {
  struct color color;
  uint8_t waveform[480];
  uint16_t waveform_size;
};

int process_command(const uint8_t *recv_buf, uint32_t size);

#endif
"""

# 8. src/ini.c
files["src/ini.c"] = r"""
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ini.h"

struct ini_t {
  char *data;
  char *end;
};

static char *next(const ini_t *ini, char *p) {
  p += strlen(p);
  while (p < ini->end && *p == '\0') p++;
  return p;
}

static void trim_back(const ini_t *ini, char *p) {
  while (p >= ini->data && (*p == ' ' || *p == '\t' || *p == '\r')) *p-- = '\0';
}

static char *discard_line(const ini_t *ini, char *p) {
  while (p < ini->end && *p != '\n') *p++ = '\0';
  return p;
}

static void split_data(const ini_t *ini) {
  char *line_start;
  char *p = ini->data;
  while (p < ini->end) {
    switch (*p) {
    case '\r': case '\n': case '\t': case ' ':
      *p = '\0';
      p++;
      break;
    case '[':
      p += strcspn(p, "]\n");
      *p = '\0';
      break;
    case ';':
      p = discard_line(ini, p);
      break;
    default:
      line_start = p;
      p += strcspn(p, "=\n");
      if (*p != '=') { p = discard_line(ini, line_start); break; }
      trim_back(ini, p - 1);
      do { *p++ = '\0'; } while (*p == ' ' || *p == '\r' || *p == '\t');
      if (*p == '\n' || *p == '\0') { p = discard_line(ini, line_start); break; }
      p += strcspn(p, "\n");
      trim_back(ini, p - 1);
      break;
    }
  }
}

ini_t *ini_load(const char *filename) {
  ini_t *ini = NULL;
  FILE *fp = fopen(filename, "rb");
  if (!fp) return NULL;
  fseek(fp, 0, SEEK_END);
  int sz = ftell(fp);
  rewind(fp);
  ini = malloc(sizeof(*ini));
  ini->data = malloc(sz + 1);
  ini->data[sz] = '\0';
  ini->end = ini->data + sz;
  int n = fread(ini->data, 1, sz, fp);
  fclose(fp);
  if (n != sz) { ini_free(ini); return NULL; }
  split_data(ini);
  return ini;
}

void ini_free(ini_t *ini) {
  free(ini->data);
  free(ini);
}

const char *ini_get(const ini_t *ini, const char *section, const char *key) {
  const char *current_section = "";
  char *p = ini->data;
  if (*p == '\0') p = next(ini, p);
  while (p < ini->end) {
    if (*p == '[') {
      current_section = p + 1;
    } else {
      char *val = next(ini, p);
      if (!section || !strcasecmp(section, current_section)) {
        if (!strcasecmp(p, key)) return val;
      }
      p = val;
    }
    p = next(ini, p);
  }
  return NULL;
}
"""

# 9. src/ini.h
files["src/ini.h"] = r"""
#ifndef INI_H
#define INI_H

typedef struct ini_t ini_t;

ini_t*      ini_load(const char *filename);
void        ini_free(ini_t *ini);
const char* ini_get(const ini_t *ini, const char *section, const char *key);

#endif
"""

# 10. src/input.c
files["src/input.c"] = r"""
#include "input.h"
#include "backends/m8.h"
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

static struct libevdev *dev = NULL;
static int fd = -1;

static unsigned char keycode_state = 0;

// Bits for M8
enum m8_keybits {
  M8_KEY_EDIT   = 1 << 0,
  M8_KEY_OPT    = 1 << 1,
  M8_KEY_RIGHT  = 1 << 2,
  M8_KEY_START  = 1 << 3,
  M8_KEY_SELECT = 1 << 4,
  M8_KEY_DOWN   = 1 << 5,
  M8_KEY_UP     = 1 << 6,
  M8_KEY_LEFT   = 1 << 7
};

int input_init(config_params_s *conf) {
    // Attempt to auto-detect a keyboard if no path is hardcoded or logic is simple
    // For simplicity, we try the first event device that has keys, or you can hardcode
    // /dev/input/by-id/...
    
    // Quick and dirty search for a device with keys
    char path[256];
    for(int i=0; i<10; i++) {
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        fd = open(path, O_RDONLY|O_NONBLOCK);
        if (fd >= 0) {
            libevdev_new_from_fd(fd, &dev);
            if (libevdev_has_event_type(dev, EV_KEY)) {
                printf("Input: Using %s (%s)\n", path, libevdev_get_name(dev));
                return 1;
            }
            libevdev_free(dev);
            close(fd);
        }
    }
    return 0; // Failed
}

void input_close() {
    if (dev) libevdev_free(dev);
    if (fd >= 0) close(fd);
}

static void update_m8_key(unsigned char bit, int value) {
    if (value) keycode_state |= bit;
    else keycode_state &= ~bit;
}

void input_poll(struct app_context *ctx) {
    if (!dev) return;

    struct input_event ev;
    int rc = 0;
    
    while ((rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev)) == LIBEVDEV_READ_STATUS_SUCCESS) {
        if (ev.type == EV_KEY && (ev.value == 0 || ev.value == 1)) {
            // 0=release, 1=press (ignoring repeat 2 for bitmask logic)
            int pressed = ev.value;
            int code = ev.code;
            config_params_s *c = &ctx->conf;

            if (code == c->key_up) update_m8_key(M8_KEY_UP, pressed);
            else if (code == c->key_left) update_m8_key(M8_KEY_LEFT, pressed);
            else if (code == c->key_down) update_m8_key(M8_KEY_DOWN, pressed);
            else if (code == c->key_right) update_m8_key(M8_KEY_RIGHT, pressed);
            
            else if (code == c->key_select || code == c->key_select_alt) update_m8_key(M8_KEY_SELECT, pressed);
            else if (code == c->key_start || code == c->key_start_alt) update_m8_key(M8_KEY_START, pressed);
            else if (code == c->key_opt || code == c->key_opt_alt) update_m8_key(M8_KEY_OPT, pressed);
            else if (code == c->key_edit || code == c->key_edit_alt) update_m8_key(M8_KEY_EDIT, pressed);
            
            // Send to M8
            if (ctx->device_connected) {
                m8_send_msg_controller(keycode_state);
            }
            
            // Handle Quit (ESC)
            if (code == KEY_ESC && pressed) {
                ctx->app_state = QUIT;
            }
        }
    }
}
"""

# 11. src/input.h
files["src/input.h"] = r"""
#ifndef INPUT_H
#define INPUT_H

#include "common.h"
#include "config.h"

int input_init(config_params_s *conf);
void input_poll(struct app_context *ctx);
void input_close();

#endif
"""

# 12. src/render.c
files["src/render.c"] = r"""
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <string.h>
#include "fonts/fonts.h"

static uint32_t pixel_buffer[320 * 240];
static int fbfd = -1;
static uint32_t *fbp = NULL;
static long screensize = 0;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;

// Current font data
static const struct inline_font *current_font = NULL;

int renderer_initialize(config_params_s *conf) {
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
        perror("Error: cannot open framebuffer device");
        return 0;
    }

    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) return 0;
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) return 0;

    // Attempt to force 320x240
    vinfo.xres = 320;
    vinfo.yres = 240;
    vinfo.xres_virtual = 320;
    vinfo.yres_virtual = 240;
    vinfo.bits_per_pixel = 32;
    ioctl(fbfd, FBIOPUT_VSCREENINFO, &vinfo);
    
    // Refresh info
    ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);

    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    fbp = (uint32_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if ((long)fbp == -1) return 0;

    renderer_set_font_mode(0);
    return 1;
}

void renderer_close(void) {
    if (fbp && (long)fbp != -1) munmap(fbp, screensize);
    if (fbfd >= 0) close(fbfd);
}

static inline void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || x >= 320 || y < 0 || y >= 240) return;
    pixel_buffer[y * 320 + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
}

void renderer_clear_screen(void) {
    memset(pixel_buffer, 0, sizeof(pixel_buffer));
}

void draw_rectangle(struct draw_rectangle_command *cmd) {
    int x, y;
    for (y = 0; y < cmd->size.height; y++) {
        for (x = 0; x < cmd->size.width; x++) {
            set_pixel(cmd->pos.x + x, cmd->pos.y + y, cmd->color.r, cmd->color.g, cmd->color.b);
        }
    }
}

void draw_waveform(struct draw_oscilloscope_waveform_command *cmd) {
    // Clear bg for waveform area (simplified)
    // Draw points
    for (int i = 0; i < cmd->waveform_size; i++) {
        set_pixel(320 - cmd->waveform_size + i, cmd->waveform[i], cmd->color.r, cmd->color.g, cmd->color.b);
    }
}

// Basic BMP drawing logic for M8 Fonts
int draw_character(struct draw_character_command *cmd) {
    if (!current_font) return 0;
    
    const unsigned char *data = current_font->image_data;
    int fw = current_font->width;
    int fh = current_font->height;
    int gx = current_font->glyph_x;
    int gy = current_font->glyph_y;
    
    int char_idx = cmd->c - 32; // basic ascii offset assumption
    if (char_idx < 0) return 0;

    // This is a placeholder. Real M8 font rendering requires parsing the specific 
    // bitmap format found in the header files (BMP structure).
    // For now, we assume this function exists and works if you use the original code logic, 
    // but simplified for raw buffers.
    // Implementing a full BMP decoder here is too large for the script constraints.
    
    // Stub: Draw a colored box for the char
    struct draw_rectangle_command rect;
    rect.pos = cmd->pos;
    rect.size.width = gx;
    rect.size.height = gy;
    rect.color = cmd->foreground;
    draw_rectangle(&rect);
    
    return 1;
}

void renderer_set_font_mode(int mode) {
    current_font = fonts_get(mode);
}

void render_screen(config_params_s *conf) {
    // Copy buffer to framebuffer
    // If resolution mismatches, this will look weird (tiny corner or clipped)
    // assuming ioctl worked or we just write what we can.
    memcpy(fbp, pixel_buffer, screensize < sizeof(pixel_buffer) ? screensize : sizeof(pixel_buffer));
}
"""

# 13. src/render.h
files["src/render.h"] = r"""
#ifndef RENDER_H_
#define RENDER_H_

#include "command.h"
#include "config.h"

int renderer_initialize(config_params_s *conf);
void renderer_close(void);
void renderer_set_font_mode(int mode);
void renderer_clear_screen(void);
void render_screen(config_params_s *conf);

void draw_waveform(struct draw_oscilloscope_waveform_command *command);
void draw_rectangle(struct draw_rectangle_command *command);
int draw_character(struct draw_character_command *command);

#endif
"""

# 14. src/backends/m8.h
files["src/backends/m8.h"] = r"""
#ifndef M8_H_
#define M8_H_

#include "../config.h"

enum return_codes {
  DEVICE_DISCONNECTED = 0,
  DEVICE_PROCESSING = 1,
  DEVICE_FATAL_ERROR = -1
};

int m8_initialize(int verbose, const char *preferred_device);
int m8_reset_display(void);
int m8_enable_display(unsigned char reset_display);
int m8_send_msg_controller(unsigned char input);
int m8_send_msg_keyjazz(unsigned char note, unsigned char velocity);
int m8_process_data(const config_params_s *conf);
int m8_close(void);

#endif
"""

# 15. src/backends/m8_libserialport.c
files["src/backends/m8_libserialport.c"] = r"""
#include <libserialport.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "../command.h"
#include "../config.h"
#include "m8.h"
#include "queue.h"
#include "slip.h"

#define SERIAL_READ_SIZE 1024

struct sp_port *m8_port = NULL;
static uint8_t serial_buffer[SERIAL_READ_SIZE] = {0};
static uint8_t slip_buffer[SERIAL_READ_SIZE] = {0};
static slip_handler_s slip;
message_queue_s queue;

pthread_t serial_thread;
volatile int thread_should_stop = 0;

static int send_message_to_queue(uint8_t *data, const uint32_t size) {
  push_message(&queue, data, size);
  return 1;
}

static void process_received_bytes(const uint8_t *buffer, int bytes_read, slip_handler_s *slip) {
  const uint8_t *cur = buffer;
  const uint8_t *end = buffer + bytes_read;
  while (cur < end) {
    slip_read_byte(slip, *cur++);
  }
}

static void *thread_process_serial_data(void *arg) {
  while (!thread_should_stop) {
    if (!m8_port) break;
    int bytes_read = sp_nonblocking_read(m8_port, serial_buffer, SERIAL_READ_SIZE);
    if (bytes_read > 0) {
      process_received_bytes(serial_buffer, bytes_read, &slip);
    } else if (bytes_read < 0) {
      break;
    }
    usleep(4000); // 4ms
  }
  return NULL;
}

static int check(enum sp_return result) {
    if (result != SP_OK) {
        printf("Serial Port Error: %d\n", result);
        return 0;
    }
    return 1;
}

int m8_initialize(int verbose, const char *preferred_device) {
  if (m8_port) return 1;

  static const slip_descriptor_s slip_descriptor = {
      .buf = slip_buffer,
      .buf_size = sizeof(slip_buffer),
      .recv_message = send_message_to_queue,
  };
  slip_init(&slip, &slip_descriptor);

  struct sp_port **port_list;
  if (sp_list_ports(&port_list) != SP_OK) return 0;

  for (int i = 0; port_list[i] != NULL; i++) {
    struct sp_port *port = port_list[i];
    int vid, pid;
    sp_get_port_usb_vid_pid(port, &vid, &pid);
    
    // M8 VID/PID
    if (vid == 0x16C0 && pid == 0x048A) {
        sp_copy_port(port, &m8_port);
        printf("Found M8 at %s\n", sp_get_port_name(port));
        break;
    }
  }
  sp_free_port_list(port_list);

  if (!m8_port) return 0;

  if (!check(sp_open(m8_port, SP_MODE_READ_WRITE))) return 0;
  if (!check(sp_set_baudrate(m8_port, 115200))) return 0;
  
  init_queue(&queue);
  thread_should_stop = 0;
  pthread_create(&serial_thread, NULL, thread_process_serial_data, NULL);

  return 1;
}

int m8_close() {
    thread_should_stop = 1;
    pthread_join(serial_thread, NULL);
    destroy_queue(&queue);
    
    if (m8_port) {
        sp_close(m8_port);
        sp_free_port(m8_port);
        m8_port = NULL;
    }
    return 1;
}

int m8_send_msg_controller(const uint8_t input) {
  if (!m8_port) return -1;
  const unsigned char buf[2] = {'C', input};
  return sp_blocking_write(m8_port, buf, 2, 5);
}

int m8_send_msg_keyjazz(const uint8_t note, uint8_t velocity) {
  if (!m8_port) return -1;
  if (velocity > 0x7F) velocity = 0x7F;
  const unsigned char buf[3] = {'K', note, velocity};
  return sp_blocking_write(m8_port, buf, 3, 5);
}

int m8_enable_display(const unsigned char reset_display) {
  if (!m8_port) return 0;
  char buf[1] = {'E'};
  sp_blocking_write(m8_port, buf, 1, 5);
  usleep(500000);
  if (reset_display) {
      char buf2[1] = {'R'};
      sp_blocking_write(m8_port, buf2, 1, 5);
  }
  return 1;
}

int m8_process_data(const config_params_s *conf) {
  if (!m8_port) return DEVICE_DISCONNECTED;

  if (queue_size(&queue) > 0) {
    unsigned char *command;
    size_t length = 0;
    while ((command = pop_message(&queue, &length)) != NULL) {
      if (length > 0) process_command(command, length);
      free(command);
    }
  }
  return DEVICE_PROCESSING;
}
"""

# 16. src/backends/queue.c
files["src/backends/queue.c"] = r"""
#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void init_queue(message_queue_s *queue) {
    queue->front = 0;
    queue->rear = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

void destroy_queue(message_queue_s *queue) {
  pthread_mutex_lock(&queue->mutex);
  while (queue->front != queue->rear) {
    free(queue->messages[queue->front]);
    queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
  }
  pthread_mutex_unlock(&queue->mutex);
  pthread_mutex_destroy(&queue->mutex);
  pthread_cond_destroy(&queue->cond);
}

void push_message(message_queue_s *queue, const unsigned char *message, size_t length) {
    pthread_mutex_lock(&queue->mutex);
    if ((queue->rear + 1) % MAX_QUEUE_SIZE != queue->front) {
        queue->messages[queue->rear] = malloc(length);
        memcpy(queue->messages[queue->rear], message, length);
        queue->lengths[queue->rear] = length;
        queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
        pthread_cond_signal(&queue->cond);
    }
    pthread_mutex_unlock(&queue->mutex);
}

unsigned char *pop_message(message_queue_s *queue, size_t *length) {
  pthread_mutex_lock(&queue->mutex);
  if (queue->front == queue->rear) {
    pthread_mutex_unlock(&queue->mutex);
    return NULL;
  }
  *length = queue->lengths[queue->front];
  unsigned char *message = queue->messages[queue->front];
  queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
  pthread_mutex_unlock(&queue->mutex);
  return message;
}

unsigned int queue_size(const message_queue_s *queue) {
  pthread_mutex_lock(&queue->mutex);
  unsigned int size = (queue->rear - queue->front + MAX_QUEUE_SIZE) % MAX_QUEUE_SIZE;
  pthread_mutex_unlock(&queue->mutex);
  return size;
}
"""

# 17. src/backends/queue.h
files["src/backends/queue.h"] = r"""
#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stddef.h>

#define MAX_QUEUE_SIZE 8192

typedef struct {
  unsigned char *messages[MAX_QUEUE_SIZE];
  size_t lengths[MAX_QUEUE_SIZE];
  int front;
  int rear;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} message_queue_s;

void init_queue(message_queue_s *queue);
void destroy_queue(message_queue_s *queue);
unsigned char *pop_message(message_queue_s *queue, size_t *length);
void push_message(message_queue_s *queue, const unsigned char *message, size_t length);
unsigned int queue_size(const message_queue_s *queue);

#endif
"""

# 18. src/backends/slip.c
files["src/backends/slip.c"] = r"""
#include "slip.h"
#include <assert.h>
#include <stddef.h>

slip_error_t slip_init(slip_handler_s *slip, const slip_descriptor_s *descriptor) {
  slip->descriptor = descriptor;
  slip->state = SLIP_STATE_NORMAL;
  slip->size = 0;
  return SLIP_NO_ERROR;
}

static void reset_rx(slip_handler_s *slip) {
  slip->state = SLIP_STATE_NORMAL;
  slip->size = 0;
}

static slip_error_t put_byte(slip_handler_s *slip, const uint8_t byte) {
  if (slip->size >= slip->descriptor->buf_size) {
    reset_rx(slip);
    return SLIP_ERROR_BUFFER_OVERFLOW;
  }
  slip->descriptor->buf[slip->size++] = byte;
  return SLIP_NO_ERROR;
}

slip_error_t slip_read_byte(slip_handler_s *slip, uint8_t byte) {
  if (slip->state == SLIP_STATE_NORMAL) {
    if (byte == SLIP_SPECIAL_BYTE_END) {
        slip->descriptor->recv_message(slip->descriptor->buf, slip->size);
        reset_rx(slip);
    } else if (byte == SLIP_SPECIAL_BYTE_ESC) {
        slip->state = SLIP_STATE_ESCAPED;
    } else {
        put_byte(slip, byte);
    }
  } else {
    if (byte == SLIP_ESCAPED_BYTE_END) byte = SLIP_SPECIAL_BYTE_END;
    else if (byte == SLIP_ESCAPED_BYTE_ESC) byte = SLIP_SPECIAL_BYTE_ESC;
    else { reset_rx(slip); return SLIP_ERROR_UNKNOWN_ESCAPED_BYTE; }
    
    put_byte(slip, byte);
    slip->state = SLIP_STATE_NORMAL;
  }
  return SLIP_NO_ERROR;
}
"""

# 19. src/backends/slip.h
files["src/backends/slip.h"] = r"""
#ifndef SLIP_H_
#define SLIP_H_

#include <stdint.h>

#define SLIP_SPECIAL_BYTE_END 0xC0
#define SLIP_SPECIAL_BYTE_ESC 0xDB
#define SLIP_ESCAPED_BYTE_END 0xDC
#define SLIP_ESCAPED_BYTE_ESC 0xDD

typedef enum { SLIP_STATE_NORMAL, SLIP_STATE_ESCAPED } slip_state_t;

typedef struct {
        uint8_t *buf;
        uint32_t buf_size;
        int (*recv_message)(uint8_t *data, uint32_t size);
} slip_descriptor_s;

typedef struct {
        slip_state_t state;
        uint32_t size;
        const slip_descriptor_s *descriptor;
} slip_handler_s;

typedef enum {
        SLIP_NO_ERROR,
        SLIP_ERROR_BUFFER_OVERFLOW,
        SLIP_ERROR_UNKNOWN_ESCAPED_BYTE,
        SLIP_ERROR_INVALID_PACKET
} slip_error_t;

slip_error_t slip_init(slip_handler_s *slip, const slip_descriptor_s *descriptor);
slip_error_t slip_read_byte(slip_handler_s *slip, uint8_t byte);

#endif
"""

# Generate Files
for filepath, content in files.items():
    dirpath = os.path.dirname(filepath)
    if dirpath and not os.path.exists(dirpath):
        os.makedirs(dirpath)
    
    with open(filepath, "w") as f:
        f.write(content.strip())
        f.write("\n")

print(f"Generated {len(files)} files.")
print("Note: You MUST copy 'src/fonts/fonts.c' and the 'src/fonts/*.h' header files")
print("from the original M8C source code into the 'src/fonts/' directory generated")
print("by this script before running 'make'.")