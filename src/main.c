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
#include "audio.h"

Config app_config;
bool g_dirty = false;

static int config_get_int(ini_t *ini, const char *section, const char *key, int default_val) {
    const char *str = ini_get(ini, section, key);
    if (str) return atoi(str);
    return default_val;
}

static void config_get_str(ini_t *ini, const char *section, const char *key, char *dst, size_t max_len) {
    const char *str = ini_get(ini, section, key);
    if (str) {
        strncpy(dst, str, max_len - 1);
        dst[max_len - 1] = '\0';
    }
}

void load_configuration(const char* filename) {
    // Set Defaults
    strcpy(app_config.serial_path, "/dev/ttyACM0");
    strcpy(app_config.fb_path, "/dev/fb0");
    strcpy(app_config.input_path, "/dev/input/event0");
    
    app_config.key_map[0] = 103; // UP
    app_config.key_map[1] = 108; // DOWN
    app_config.key_map[2] = 105; // LEFT
    app_config.key_map[3] = 106; // RIGHT
    app_config.key_map[4] = 42;  // SELECT
    app_config.key_map[5] = 57;  // START
    app_config.key_map[6] = 29;  // OPT
    app_config.key_map[7] = 56;  // EDIT

    // Audio Defaults
    audio_config.enabled = 0;
    strcpy(audio_config.input_name, "M8");
    strcpy(audio_config.output_name, "ALSA");
    audio_config.period_size = 256;
    audio_config.period_count = 4;

    ini_t *ini = ini_load(filename);
    if (!ini) return;

    config_get_str(ini, "system", "serial_device", app_config.serial_path, 64);
    config_get_str(ini, "system", "framebuffer_device", app_config.fb_path, 64);
    config_get_str(ini, "system", "input_device", app_config.input_path, 64);

    const char* names[] = {"key_up","key_down","key_left","key_right","key_select","key_start","key_opt","key_edit"};
    for(int i=0; i<8; i++) {
        app_config.key_map[i] = config_get_int(ini, "keyboard", names[i], app_config.key_map[i]);
    }

    audio_config.enabled = config_get_int(ini, "audio", "enabled", audio_config.enabled);
    config_get_str(ini, "audio", "input_device_name", audio_config.input_name, 32);
    config_get_str(ini, "audio", "output_device_name", audio_config.output_name, 32);
    audio_config.period_size = config_get_int(ini, "audio", "period_size", audio_config.period_size);
    audio_config.period_count = config_get_int(ini, "audio", "period_count", audio_config.period_count);

    ini_free(ini);
}

int main(int argc, char** argv) {
    load_configuration("config.ini");

    display_init();
    input_init();
    serial_init();

    if (audio_config.enabled) {
        audio_start_thread();
    }

    struct pollfd fds[2]; 
    while (1) {
        if (!serial_is_connected()) {
            serial_connect();
            if (!serial_is_connected()) usleep(500000); 
        }

        int nfds = 0;
        int ser_fd = serial_get_fd();
        int inp_fd = input_get_fd();

        if (ser_fd != -1) { fds[nfds].fd = ser_fd; fds[nfds].events = POLLIN; nfds++; }
        if (inp_fd != -1) { fds[nfds].fd = inp_fd; fds[nfds].events = POLLIN; nfds++; }

        if (nfds == 0) { usleep(10000); continue; }

        int ret = poll(fds, nfds, 10); 
        if (ret > 0) {
            if (ser_fd != -1 && (fds[0].revents & POLLIN)) serial_read();
            int inp_idx = (ser_fd != -1) ? 1 : 0;
            if (inp_fd != -1 && inp_idx < nfds && (fds[inp_idx].revents & POLLIN)) input_process();
        }

        if (g_dirty) {
            display_blit();
            g_dirty = false;
        }
    }

    display_close();
    return 0;
}

