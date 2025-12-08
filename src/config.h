#ifndef CONFIG_H_
#define CONFIG_H_

typedef struct config_params_s {
  char *filename;
  unsigned int wait_packets;

  unsigned int key_up;
  unsigned int key_left;
  unsigned int key_down;
  unsigned int key_right;
  unsigned int key_select;
  unsigned int key_select_alt;
  unsigned int key_start;
  unsigned int key_start_alt;
  unsigned int key_opt;
  unsigned int key_opt_alt;
  unsigned int key_edit;
  unsigned int key_edit_alt;
  unsigned int key_delete;
  unsigned int key_reset;
  unsigned int key_jazz_inc_octave;
  unsigned int key_jazz_dec_octave;
  unsigned int key_jazz_inc_velocity;
  unsigned int key_jazz_dec_velocity;
} config_params_s;

config_params_s config_initialize(char *filename);
void config_read(config_params_s *conf);
void write_config(const config_params_s *conf);

#endif
