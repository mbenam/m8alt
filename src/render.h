#ifndef RENDER_H_
#define RENDER_H_

#include "command.h"
#include "config.h"

int renderer_initialize(config_params_s *conf);
void renderer_close(void);
void renderer_set_font_mode(int mode);
void renderer_clear_screen(void);
void render_screen(config_params_s *conf);

void draw_waveform(struct draw_oscilloscope_waveform_command *command);
void draw_rectangle(struct draw_rectangle_command *command);
int draw_character(struct draw_character_command *command);

#endif
