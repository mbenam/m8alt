#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>

#define M8_WIDTH 320
#define M8_HEIGHT 240

// Global Dirty Flag (set by Serial, read by Main/Display)
extern bool g_dirty;

// Application Configuration
typedef struct {
    char serial_path[64];
    char fb_path[64];
    char input_path[64];
    int key_map[8]; // UP, DOWN, LEFT, RIGHT, SELECT, START, OPT, EDIT
} Config;

extern Config app_config;

#endif