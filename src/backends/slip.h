#ifndef SLIP_H_
#define SLIP_H_

#include <stdint.h>

#define SLIP_SPECIAL_BYTE_END 0xC0
#define SLIP_SPECIAL_BYTE_ESC 0xDB
#define SLIP_ESCAPED_BYTE_END 0xDC
#define SLIP_ESCAPED_BYTE_ESC 0xDD

typedef enum { SLIP_STATE_NORMAL, SLIP_STATE_ESCAPED } slip_state_t;

typedef struct {
        uint8_t *buf;
        uint32_t buf_size;
        int (*recv_message)(uint8_t *data, uint32_t size);
} slip_descriptor_s;

typedef struct {
        slip_state_t state;
        uint32_t size;
        const slip_descriptor_s *descriptor;
} slip_handler_s;

typedef enum {
        SLIP_NO_ERROR,
        SLIP_ERROR_BUFFER_OVERFLOW,
        SLIP_ERROR_UNKNOWN_ESCAPED_BYTE,
        SLIP_ERROR_INVALID_PACKET
} slip_error_t;

slip_error_t slip_init(slip_handler_s *slip, const slip_descriptor_s *descriptor);
slip_error_t slip_read_byte(slip_handler_s *slip, uint8_t byte);

#endif
