#ifndef INPUT_H
#define INPUT_H

#include "common.h"
#include "config.h"

int input_init(config_params_s *conf);
void input_poll(struct app_context *ctx);
void input_close();

#endif
