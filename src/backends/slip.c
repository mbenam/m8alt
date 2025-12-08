#include "slip.h"
#include <assert.h>
#include <stddef.h>

slip_error_t slip_init(slip_handler_s *slip, const slip_descriptor_s *descriptor) {
  slip->descriptor = descriptor;
  slip->state = SLIP_STATE_NORMAL;
  slip->size = 0;
  return SLIP_NO_ERROR;
}

static void reset_rx(slip_handler_s *slip) {
  slip->state = SLIP_STATE_NORMAL;
  slip->size = 0;
}

static slip_error_t put_byte(slip_handler_s *slip, const uint8_t byte) {
  if (slip->size >= slip->descriptor->buf_size) {
    reset_rx(slip);
    return SLIP_ERROR_BUFFER_OVERFLOW;
  }
  slip->descriptor->buf[slip->size++] = byte;
  return SLIP_NO_ERROR;
}

slip_error_t slip_read_byte(slip_handler_s *slip, uint8_t byte) {
  if (slip->state == SLIP_STATE_NORMAL) {
    if (byte == SLIP_SPECIAL_BYTE_END) {
        slip->descriptor->recv_message(slip->descriptor->buf, slip->size);
        reset_rx(slip);
    } else if (byte == SLIP_SPECIAL_BYTE_ESC) {
        slip->state = SLIP_STATE_ESCAPED;
    } else {
        put_byte(slip, byte);
    }
  } else {
    if (byte == SLIP_ESCAPED_BYTE_END) byte = SLIP_SPECIAL_BYTE_END;
    else if (byte == SLIP_ESCAPED_BYTE_ESC) byte = SLIP_SPECIAL_BYTE_ESC;
    else { reset_rx(slip); return SLIP_ERROR_UNKNOWN_ESCAPED_BYTE; }
    
    put_byte(slip, byte);
    slip->state = SLIP_STATE_NORMAL;
  }
  return SLIP_NO_ERROR;
}
