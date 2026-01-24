#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

typedef struct {
    bool enabled;
    char input_name[32];
    char output_name[32];
    int period_size;
    int period_count;
} AudioConfig;

extern AudioConfig audio_config;

void audio_start_thread(void);

#endif

