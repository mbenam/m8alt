/* 
 * M8C Embedded - Stripped down version for RPi/Linux FB
 * Renders directly to /dev/fb0 and reads from /dev/input/eventX
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#include "common.h"
#include "ini.h"
#include "display.h"
#include "input.h"
#include "serial.h"

// Global Definitions
Config app_config;
bool g_dirty = false;

// Helper to safely load integers from config
static int config_get_int(ini_t *ini, const char *section, const char *key, int default_val) {
    const char *str = ini_get(ini, section, key);
    if (str) return atoi(str);
    return default_val;
}

// Helper to safely load strings from config
static void config_get_str(ini_t *ini, const char *section, const char *key, char *dst, size_t max_len) {
    const char *str = ini_get(ini, section, key);
    if (str) {
        strncpy(dst, str, max_len - 1);
        dst[max_len - 1] = '\0';
    }
}

void load_configuration(const char* filename) {
    // 1. Set Defaults
    strcpy(app_config.serial_path, "/dev/ttyACM0");
    strcpy(app_config.fb_path, "/dev/fb0");
    strcpy(app_config.input_path, "/dev/input/event0");
    
    app_config.key_map[0] = 103; // UP
    app_config.key_map[1] = 108; // DOWN
    app_config.key_map[2] = 105; // LEFT
    app_config.key_map[3] = 106; // RIGHT
    app_config.key_map[4] = 42;  // SELECT (Shift)
    app_config.key_map[5] = 57;  // START (Space)
    app_config.key_map[6] = 29;  // OPT (L_CTRL - Note: Config usually 29 or 44 depending on Z vs Ctrl)
    app_config.key_map[7] = 56;  // EDIT (L_ALT)

    // 2. Load File
    ini_t *ini = ini_load(filename);
    if (!ini) {
        printf("Notice: '%s' not found or invalid, using defaults.\n", filename);
        return;
    }

    // 3. Parse System
    config_get_str(ini, "system", "serial_device", app_config.serial_path, 64);
    config_get_str(ini, "system", "framebuffer_device", app_config.fb_path, 64);
    config_get_str(ini, "system", "input_device", app_config.input_path, 64);

    // 4. Parse Keyboard
    app_config.key_map[0] = config_get_int(ini, "keyboard", "key_up", app_config.key_map[0]);
    app_config.key_map[1] = config_get_int(ini, "keyboard", "key_down", app_config.key_map[1]);
    app_config.key_map[2] = config_get_int(ini, "keyboard", "key_left", app_config.key_map[2]);
    app_config.key_map[3] = config_get_int(ini, "keyboard", "key_right", app_config.key_map[3]);
    app_config.key_map[4] = config_get_int(ini, "keyboard", "key_select", app_config.key_map[4]);
    app_config.key_map[5] = config_get_int(ini, "keyboard", "key_start", app_config.key_map[5]);
    app_config.key_map[6] = config_get_int(ini, "keyboard", "key_opt", app_config.key_map[6]);
    app_config.key_map[7] = config_get_int(ini, "keyboard", "key_edit", app_config.key_map[7]);

    ini_free(ini);
    printf("Config loaded from %s\n", filename);
}

int main(int argc, char** argv) {
    load_configuration("config.ini");

    display_init();
    input_init();
    serial_init();

    struct pollfd fds[2]; 
    printf("M8C Embedded Started. Ctrl+C to exit.\n");

    while (1) {
        // Auto-connect Serial
        if (!serial_is_connected()) {
            serial_connect();
            // If still not connected, wait a bit to avoid CPU spin
            if (!serial_is_connected()) usleep(500000); 
        }

        // Setup Poll
        int nfds = 0;
        int ser_fd = serial_get_fd();
        int inp_fd = input_get_fd();

        if (ser_fd != -1) {
            fds[nfds].fd = ser_fd;
            fds[nfds].events = POLLIN;
            nfds++;
        }
        if (inp_fd != -1) {
            fds[nfds].fd = inp_fd;
            fds[nfds].events = POLLIN;
            nfds++;
        }

        // If no devices are ready to poll, sleep briefly
        if (nfds == 0) {
            usleep(10000); 
            continue;
        }

        int ret = poll(fds, nfds, 10); 

        if (ret > 0) {
            // Check Serial (Assume index 0 if connected)
            if (ser_fd != -1 && (fds[0].revents & POLLIN)) {
                serial_read();
            }
            
            // Check Input
            // If serial is connected (index 0), input is index 1. 
            // If serial is NOT connected, input is index 0.
            int inp_idx = (ser_fd != -1) ? 1 : 0;
            
            // Bounds check nfds to ensure we actually added input to the array
            if (inp_fd != -1 && inp_idx < nfds && (fds[inp_idx].revents & POLLIN)) {
                input_process();
            }
        }

        if (g_dirty) {
            display_blit();
            g_dirty = false;
        }
    }

    display_close();
    return 0;
}