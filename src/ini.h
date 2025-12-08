#ifndef INI_H
#define INI_H

typedef struct ini_t ini_t;

ini_t*      ini_load(const char *filename);
void        ini_free(ini_t *ini);
const char* ini_get(const ini_t *ini, const char *section, const char *key);

#endif
