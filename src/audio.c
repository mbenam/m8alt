#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>

#include "audio.h"
#include "common.h"
#include "tinyalsa/asoundlib.h"

AudioConfig audio_config;

static int find_card_by_name(const char *name) {
    FILE *f = fopen("/proc/asound/cards", "r");
    if (!f) return -1;
    char line[256];
    int card_idx = -1;
    while (fgets(line, sizeof(line), f)) {
        int curr_card;
        if (sscanf(line, " %d [", &curr_card) == 1) {
            if (strstr(line, name)) { card_idx = curr_card; break; }
            if (fgets(line, sizeof(line), f) && strstr(line, name)) {
                card_idx = curr_card; break;
            }
        }
    }
    fclose(f);
    return card_idx;
}

void* audio_thread_fn(void* arg) {
    // 1. Thread Affinity (Pin to Core)
    #ifdef AUDIO_PIN_CORE
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(AUDIO_PIN_CORE, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    #endif

    // 2. Set Real-Time Priority
    struct sched_param param;
    param.sched_priority = 90;
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        fprintf(stderr, "Audio Warning: Could not set RT priority (run with sudo)\n");
    }

    int in_card = find_card_by_name(audio_config.input_name);
    int out_card = find_card_by_name(audio_config.output_name);

    if (in_card < 0 || out_card < 0) {
        fprintf(stderr, "Audio Error: Cards not found (In: %s -> %d, Out: %s -> %d)\n", 
                audio_config.input_name, in_card, audio_config.output_name, out_card);
        return NULL;
    }

    struct pcm_config config;
    memset(&config, 0, sizeof(config));
    config.channels = 2;
    config.rate = 44100;
    config.period_size = audio_config.period_size;
    config.period_count = audio_config.period_count;
    config.format = PCM_FORMAT_S16_LE;
    config.start_threshold = audio_config.period_size;
    config.stop_threshold = audio_config.period_size * audio_config.period_count;
    config.silence_threshold = 0;

    struct pcm *pcm_in = pcm_open(in_card, 0, PCM_IN, &config);
    struct pcm *pcm_out = pcm_open(out_card, 0, PCM_OUT, &config);

    if (!pcm_is_ready(pcm_in) || !pcm_is_ready(pcm_out)) {
        fprintf(stderr, "Audio PCM Error: In(%s) Out(%s)\n", pcm_get_error(pcm_in), pcm_get_error(pcm_out));
        if (pcm_in) pcm_close(pcm_in);
        if (pcm_out) pcm_close(pcm_out);
        return NULL;
    }

    // Modern TinyALSA uses frame counts for readi/writei
    unsigned int frame_count = pcm_get_buffer_size(pcm_in);
    unsigned int bytes_per_buffer = pcm_frames_to_bytes(pcm_in, frame_count);
    void *buffer = malloc(bytes_per_buffer);

    printf("Audio Passthrough Started: %s -> %s\n", audio_config.input_name, audio_config.output_name);

    while (1) {
        // pcm_readi and pcm_writei take frame count, not byte count
        if (pcm_readi(pcm_in, buffer, frame_count) < 0) {
            fprintf(stderr, "Audio capture error\n");
            break;
        }
        if (pcm_writei(pcm_out, buffer, frame_count) < 0) {
            fprintf(stderr, "Audio playback error\n");
            break;
        }
    }

    free(buffer);
    pcm_close(pcm_in);
    pcm_close(pcm_out);
    return NULL;
}

void audio_start_thread(void) {
    if (!audio_config.enabled) return;
    pthread_t thread;
    pthread_create(&thread, NULL, audio_thread_fn, NULL);
    pthread_detach(thread);
}

