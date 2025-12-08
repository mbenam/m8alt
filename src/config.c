#include "config.h"
#include "ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>
#include <ctype.h>

static int strcmpci(const char *a, const char *b) {
  for (;;) {
    const int d = tolower(*a) - tolower(*b);
    if (d != 0 || !*a) return d;
    a++, b++;
  }
}

config_params_s config_initialize(char *filename) {
  config_params_s c;
  c.filename = filename ? filename : "config.ini";
  c.wait_packets = 256;

  // Default Linux Input Event Codes
  c.key_up = KEY_UP;
  c.key_left = KEY_LEFT;
  c.key_down = KEY_DOWN;
  c.key_right = KEY_RIGHT;
  
  c.key_select = KEY_LEFTSHIFT;
  c.key_select_alt = KEY_Z;
  
  c.key_start = KEY_SPACE;
  c.key_start_alt = KEY_X;
  
  c.key_opt = KEY_LEFTALT;
  c.key_opt_alt = KEY_A;
  
  c.key_edit = KEY_LEFTCTRL;
  c.key_edit_alt = KEY_S;
  
  c.key_delete = KEY_DELETE;
  c.key_reset = KEY_R;
  
  // Keyjazz defaults
  c.key_jazz_inc_octave = KEY_KPASTERISK;
  c.key_jazz_dec_octave = KEY_KPSLASH;
  c.key_jazz_inc_velocity = KEY_KPPLUS;
  c.key_jazz_dec_velocity = KEY_KPMINUS;
  
  return c;
}

void write_config(const config_params_s *conf) {
  char config_path[1024];
  snprintf(config_path, sizeof(config_path), "./%s", conf->filename);
  
  FILE *fp = fopen(config_path, "w");
  if (fp) {
    fprintf(fp, "[graphics]\nwait_packets=%d\n", conf->wait_packets);
    fprintf(fp, "[keyboard]\n");
    fprintf(fp, "key_up=%d\n", conf->key_up);
    fprintf(fp, "key_left=%d\n", conf->key_left);
    fprintf(fp, "key_down=%d\n", conf->key_down);
    fprintf(fp, "key_right=%d\n", conf->key_right);
    fprintf(fp, "key_select=%d\n", conf->key_select);
    fprintf(fp, "key_select_alt=%d\n", conf->key_select_alt);
    fprintf(fp, "key_start=%d\n", conf->key_start);
    fprintf(fp, "key_start_alt=%d\n", conf->key_start_alt);
    fprintf(fp, "key_opt=%d\n", conf->key_opt);
    fprintf(fp, "key_opt_alt=%d\n", conf->key_opt_alt);
    fprintf(fp, "key_edit=%d\n", conf->key_edit);
    fprintf(fp, "key_edit_alt=%d\n", conf->key_edit_alt);
    fprintf(fp, "key_delete=%d\n", conf->key_delete);
    fprintf(fp, "key_reset=%d\n", conf->key_reset);
    fprintf(fp, "key_jazz_inc_octave=%d\n", conf->key_jazz_inc_octave);
    fprintf(fp, "key_jazz_dec_octave=%d\n", conf->key_jazz_dec_octave);
    fprintf(fp, "key_jazz_inc_velocity=%d\n", conf->key_jazz_inc_velocity);
    fprintf(fp, "key_jazz_dec_velocity=%d\n", conf->key_jazz_dec_velocity);
    fclose(fp);
  }
}

void read_key_config(const ini_t *ini, config_params_s *conf) {
  const char *val;
  #define READ_KEY(name) if((val = ini_get(ini, "keyboard", #name))) conf->name = atoi(val)
  
  READ_KEY(key_up);
  READ_KEY(key_left);
  READ_KEY(key_down);
  READ_KEY(key_right);
  READ_KEY(key_select);
  READ_KEY(key_select_alt);
  READ_KEY(key_start);
  READ_KEY(key_start_alt);
  READ_KEY(key_opt);
  READ_KEY(key_opt_alt);
  READ_KEY(key_edit);
  READ_KEY(key_edit_alt);
  READ_KEY(key_delete);
  READ_KEY(key_reset);
  READ_KEY(key_jazz_inc_octave);
  READ_KEY(key_jazz_dec_octave);
  READ_KEY(key_jazz_inc_velocity);
  READ_KEY(key_jazz_dec_velocity);
}

void config_read(config_params_s *conf) {
  char config_path[1024];
  snprintf(config_path, sizeof(config_path), "./%s", conf->filename);
  
  ini_t *ini = ini_load(config_path);
  if (ini == NULL) {
    write_config(conf);
    return;
  }
  
  const char *wait = ini_get(ini, "graphics", "wait_packets");
  if(wait) conf->wait_packets = atoi(wait);
  
  read_key_config(ini, conf);
  ini_free(ini);
  write_config(conf);
}
