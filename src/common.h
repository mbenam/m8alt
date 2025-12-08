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
