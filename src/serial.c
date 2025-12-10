#include "serial.h"
#include "common.h"
#include "display.h"
#include "slip.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

static int ser_fd = -1;
static bool ser_connected = false;
static uint8_t rx_buffer[1024];
static slip_handler_s slip;

// M8 "Running Status" - Persist color between commands
static uint8_t last_r = 255;
static uint8_t last_g = 255;
static uint8_t last_b = 255;

enum {
    CMD_DRAW_RECT = 0xFE,
    CMD_DRAW_CHAR = 0xFD,
    CMD_DRAW_WAVE = 0xFC,
    CMD_SYSTEM_INFO = 0xFF
};

// --- Command Processor ---
static int process_command(uint8_t *data, uint32_t size) {
    if (size == 0) return 0;
    
    uint8_t cmd = data[0];

    if (cmd == CMD_DRAW_RECT) {
        uint16_t x = data[1] | (data[2] << 8);
        uint16_t y = data[3] | (data[4] << 8);
        
        // Defaults
        uint16_t w = 1;
        uint16_t h = 1;
        
        // Use persistent color state by default
        uint8_t r = last_r;
        uint8_t g = last_g;
        uint8_t b = last_b;
        
        // Protocol:
        // Size 5:  Pos (w=1, h=1, use last color)
        // Size 8:  Pos + Color (w=1, h=1)
        // Size 9:  Pos + Size (use last color)
        // Size 12: Pos + Size + Color

        if (size >= 12) { 
             w = data[5] | (data[6] << 8);
             h = data[7] | (data[8] << 8);
             // Update persistent color
             last_r = data[9]; last_g = data[10]; last_b = data[11];
             r = last_r; g = last_g; b = last_b;
        } else if (size >= 9) { 
             w = data[5] | (data[6] << 8);
             h = data[7] | (data[8] << 8);
             // Use last_r/g/b (already set)
        } else if (size >= 8) { 
             // Update persistent color
             last_r = data[5]; last_g = data[6]; last_b = data[7];
             r = last_r; g = last_g; b = last_b;
        }
        // If size == 5, w/h are 1, and we use last_r/g/b
        
        display_draw_rect(x, y, w, h, r, g, b);
        g_dirty = true;
    }
    else if (cmd == CMD_DRAW_CHAR) {
        char c = data[1];
        uint16_t x = data[2] | (data[3] << 8);
        uint16_t y = data[4] | (data[5] << 8);
        display_draw_char(c, x, y, data[6], data[7], data[8], data[9], data[10], data[11]);
        g_dirty = true;
    }
    else if (cmd == CMD_DRAW_WAVE) {
        uint8_t r = data[1];
        uint8_t g = data[2];
        uint8_t b = data[3];
        display_draw_waveform(r, g, b, &data[4], size - 4);
        g_dirty = true;
    }
    else if (cmd == CMD_SYSTEM_INFO) {
        int hw = data[1]; // 3 = Model:02
        int font_mode = data[5];
        if(hw == 3) font_mode += 2;
        display_set_font(font_mode);
    }
    return 1;
}

static int recv_msg_cb(uint8_t *data, uint32_t size) {
    return process_command(data, size);
}

// --- Public Interface ---

void serial_init(void) {
    ser_fd = -1;
    ser_connected = false;
    static const slip_descriptor_s slip_desc = {
        .buf = rx_buffer,
        .buf_size = sizeof(rx_buffer),
        .recv_message = recv_msg_cb
    };
    slip_init(&slip, &slip_desc);
}

void serial_connect(void) {
    if (ser_connected) return;

    ser_fd = open(app_config.serial_path, O_RDWR | O_NOCTTY | O_NDELAY);
    if (ser_fd == -1) return;
    
    fcntl(ser_fd, F_SETFL, 0);

    struct termios options;
    tcgetattr(ser_fd, &options);
    cfsetspeed(&options, B115200);
    cfmakeraw(&options);
    tcsetattr(ser_fd, TCSANOW, &options);

    ser_connected = true;
    printf("M8 Connected on %s\n", app_config.serial_path);
    
    // Handshake
    write(ser_fd, "D", 1);
    usleep(20000);
    write(ser_fd, "E", 1);
    usleep(20000);
    write(ser_fd, "R", 1);
}

void serial_close(void) {
    if (ser_fd != -1) close(ser_fd);
    ser_connected = false;
    ser_fd = -1;
}

void serial_read(void) {
    if (!ser_connected) return;
    uint8_t buf[256];
    int n = read(ser_fd, buf, sizeof(buf));
    if (n > 0) {
        for (int i = 0; i < n; i++) slip_read_byte(&slip, buf[i]);
    } else if (n < 0 && errno != EAGAIN) {
        serial_close();
        printf("M8 Disconnected\n");
    }
}

void serial_send_input(uint8_t val) {
    if (!ser_connected) return;
    uint8_t buf[2] = {'C', val};
    if (write(ser_fd, buf, 2) < 0) {
        serial_close();
        printf("M8 Disconnected (Write fail)\n");
    }
}

int serial_get_fd(void) {
    return ser_fd;
}

bool serial_is_connected(void) {
    return ser_connected;
}