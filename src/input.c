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
