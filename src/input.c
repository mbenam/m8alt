#include "input.h"
#include "common.h"
#include "serial.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

static int inp_fd = -1;
static uint8_t input_state = 0;

void input_init(void) {
    inp_fd = open(app_config.input_path, O_RDONLY | O_NONBLOCK);
    if (inp_fd == -1) {
        fprintf(stderr, "Input Warning: Could not open %s\n", app_config.input_path);
    }
}

int input_get_fd(void) {
    return inp_fd;
}

static void update_key_mask(uint16_t code, int value) {
    if (value == 2) return; // Ignore repeat

    uint8_t mask = 0;
    // LEFT=0x80, UP=0x40, DOWN=0x20, SELECT=0x10, START=0x08, RIGHT=0x04, OPT=0x02, EDIT=0x01
    
    if (code == app_config.key_map[0]) mask = 0x40; // UP
    else if (code == app_config.key_map[1]) mask = 0x20; // DOWN
    else if (code == app_config.key_map[2]) mask = 0x80; // LEFT
    else if (code == app_config.key_map[3]) mask = 0x04; // RIGHT
    else if (code == app_config.key_map[4]) mask = 0x10; // SELECT
    else if (code == app_config.key_map[5]) mask = 0x08; // START
    else if (code == app_config.key_map[6]) mask = 0x02; // OPT
    else if (code == app_config.key_map[7]) mask = 0x01; // EDIT

    if (mask == 0) return;

    if (value == 1) input_state |= mask;
    else input_state &= ~mask;

    serial_send_input(input_state);
}

void input_process(void) {
    if (inp_fd == -1) return;
    
    struct input_event ev;
    while (read(inp_fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_KEY) {
            update_key_mask(ev.code, ev.value);
        }
    }
}