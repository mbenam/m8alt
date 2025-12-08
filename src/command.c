#include "command.h"
#include "render.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define ArrayCount(x) sizeof(x) / sizeof((x)[1])

static uint16_t decodeInt16(const uint8_t *data, const uint8_t start) {
  return data[start] | (((uint16_t)data[start + 1] << 8) & 0xFFFF);
}

enum m8_command_bytes {
  draw_rectangle_command = 0xFE,
  draw_rectangle_command_pos_datalength = 5,
  draw_rectangle_command_pos_color_datalength = 8,
  draw_rectangle_command_pos_size_datalength = 9,
  draw_rectangle_command_pos_size_color_datalength = 12,
  draw_character_command = 0xFD,
  draw_character_command_datalength = 12,
  draw_oscilloscope_waveform_command = 0xFC,
  draw_oscilloscope_waveform_command_mindatalength = 1 + 3,
  draw_oscilloscope_waveform_command_maxdatalength = 1 + 3 + 480,
  joypad_keypressedstate_command = 0xFB,
  joypad_keypressedstate_command_datalength = 3,
  system_info_command = 0xFF,
  system_info_command_datalength = 6
};

int process_command(const uint8_t *recv_buf, uint32_t size) {
  switch (recv_buf[0]) {
  case draw_rectangle_command: {
    static struct draw_rectangle_command rectcmd;
    rectcmd.pos.x = decodeInt16(recv_buf, 1);
    rectcmd.pos.y = decodeInt16(recv_buf, 3);

    switch (size) {
    case draw_rectangle_command_pos_datalength:
      rectcmd.size.width = 1;
      rectcmd.size.height = 1;
      break;
    case draw_rectangle_command_pos_color_datalength:
      rectcmd.size.width = 1;
      rectcmd.size.height = 1;
      rectcmd.color.r = recv_buf[5];
      rectcmd.color.g = recv_buf[6];
      rectcmd.color.b = recv_buf[7];
      break;
    case draw_rectangle_command_pos_size_datalength:
      rectcmd.size.width = decodeInt16(recv_buf, 5);
      rectcmd.size.height = decodeInt16(recv_buf, 7);
      break;
    case draw_rectangle_command_pos_size_color_datalength:
      rectcmd.size.width = decodeInt16(recv_buf, 5);
      rectcmd.size.height = decodeInt16(recv_buf, 7);
      rectcmd.color.r = recv_buf[9];
      rectcmd.color.g = recv_buf[10];
      rectcmd.color.b = recv_buf[11];
      break;
    default:
      return 0;
    }
    draw_rectangle(&rectcmd);
    return 1;
  }

  case draw_character_command: {
    if (size != draw_character_command_datalength) return 0;
    struct draw_character_command charcmd = {
        recv_buf[1],
        {decodeInt16(recv_buf, 2), decodeInt16(recv_buf, 4)},
        {recv_buf[6], recv_buf[7], recv_buf[8]},
        {recv_buf[9], recv_buf[10], recv_buf[11]}};
    draw_character(&charcmd);
    return 1;
  }

  case draw_oscilloscope_waveform_command: {
    if (size < draw_oscilloscope_waveform_command_mindatalength ||
        size > draw_oscilloscope_waveform_command_maxdatalength) return 0;
    
    struct draw_oscilloscope_waveform_command osccmd = {0};
    osccmd.color = (struct color){recv_buf[1], recv_buf[2], recv_buf[3]};
    memcpy(osccmd.waveform, &recv_buf[4], size - 4);
    osccmd.waveform_size = (size & 0xFFFF) - 4;

    draw_waveform(&osccmd);
    return 1;
  }

  case system_info_command: {
    if (size != system_info_command_datalength) break;
    renderer_set_font_mode(recv_buf[5]);
    return 1;
  }

  default:
    break;
  }
  return 1;
}
