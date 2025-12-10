#ifndef SERIAL_H
#define SERIAL_H

#include <stdbool.h>
#include <stdint.h>

void serial_init(void);
void serial_connect(void);
int serial_get_fd(void);
bool serial_is_connected(void);
void serial_read(void);
void serial_send_input(uint8_t val);
void serial_close(void);

#endif