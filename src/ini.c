#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ini.h"

struct ini_t {
  char *data;
  char *end;
};

static char *next(const ini_t *ini, char *p) {
  p += strlen(p);
  while (p < ini->end && *p == '\0') p++;
  return p;
}

static void trim_back(const ini_t *ini, char *p) {
  while (p >= ini->data && (*p == ' ' || *p == '\t' || *p == '\r')) *p-- = '\0';
}

static char *discard_line(const ini_t *ini, char *p) {
  while (p < ini->end && *p != '\n') *p++ = '\0';
  return p;
}

static void split_data(const ini_t *ini) {
  char *line_start;
  char *p = ini->data;
  while (p < ini->end) {
    switch (*p) {
    case '\r': case '\n': case '\t': case ' ':
      *p = '\0';
      p++;
      break;
    case '[':
      p += strcspn(p, "]\n");
      *p = '\0';
      break;
    case ';':
      p = discard_line(ini, p);
      break;
    default:
      line_start = p;
      p += strcspn(p, "=\n");
      if (*p != '=') { p = discard_line(ini, line_start); break; }
      trim_back(ini, p - 1);
      do { *p++ = '\0'; } while (*p == ' ' || *p == '\r' || *p == '\t');
      if (*p == '\n' || *p == '\0') { p = discard_line(ini, line_start); break; }
      p += strcspn(p, "\n");
      trim_back(ini, p - 1);
      break;
    }
  }
}

ini_t *ini_load(const char *filename) {
  ini_t *ini = NULL;
  FILE *fp = fopen(filename, "rb");
  if (!fp) return NULL;
  fseek(fp, 0, SEEK_END);
  int sz = ftell(fp);
  rewind(fp);
  ini = malloc(sizeof(*ini));
  ini->data = malloc(sz + 1);
  ini->data[sz] = '\0';
  ini->end = ini->data + sz;
  int n = fread(ini->data, 1, sz, fp);
  fclose(fp);
  if (n != sz) { ini_free(ini); return NULL; }
  split_data(ini);
  return ini;
}

void ini_free(ini_t *ini) {
  free(ini->data);
  free(ini);
}

const char *ini_get(const ini_t *ini, const char *section, const char *key) {
  const char *current_section = "";
  char *p = ini->data;
  if (*p == '\0') p = next(ini, p);
  while (p < ini->end) {
    if (*p == '[') {
      current_section = p + 1;
    } else {
      char *val = next(ini, p);
      if (!section || !strcasecmp(section, current_section)) {
        if (!strcasecmp(p, key)) return val;
      }
      p = val;
    }
    p = next(ini, p);
  }
  return NULL;
}
