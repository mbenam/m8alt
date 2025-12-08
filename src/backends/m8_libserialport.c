#include <libserialport.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "../command.h"
#include "../config.h"
#include "m8.h"
#include "queue.h"
#include "slip.h"

#define SERIAL_READ_SIZE 1024

struct sp_port *m8_port = NULL;
static uint8_t serial_buffer[SERIAL_READ_SIZE] = {0};
static uint8_t slip_buffer[SERIAL_READ_SIZE] = {0};
static slip_handler_s slip;
message_queue_s queue;

pthread_t serial_thread;
volatile int thread_should_stop = 0;

static int send_message_to_queue(uint8_t *data, const uint32_t size) {
  push_message(&queue, data, size);
  return 1;
}

static void process_received_bytes(const uint8_t *buffer, int bytes_read, slip_handler_s *slip) {
  const uint8_t *cur = buffer;
  const uint8_t *end = buffer + bytes_read;
  while (cur < end) {
    slip_read_byte(slip, *cur++);
  }
}

static void *thread_process_serial_data(void *arg) {
  while (!thread_should_stop) {
    if (!m8_port) break;
    int bytes_read = sp_nonblocking_read(m8_port, serial_buffer, SERIAL_READ_SIZE);
    if (bytes_read > 0) {
      process_received_bytes(serial_buffer, bytes_read, &slip);
    } else if (bytes_read < 0) {
      break;
    }
    usleep(4000); // 4ms
  }
  return NULL;
}

static int check(enum sp_return result) {
    if (result != SP_OK) {
        printf("Serial Port Error: %d\n", result);
        return 0;
    }
    return 1;
}

int m8_initialize(int verbose, const char *preferred_device) {
  if (m8_port) return 1;

  static const slip_descriptor_s slip_descriptor = {
      .buf = slip_buffer,
      .buf_size = sizeof(slip_buffer),
      .recv_message = send_message_to_queue,
  };
  slip_init(&slip, &slip_descriptor);

  struct sp_port **port_list;
  if (sp_list_ports(&port_list) != SP_OK) return 0;

  for (int i = 0; port_list[i] != NULL; i++) {
    struct sp_port *port = port_list[i];
    int vid, pid;
    sp_get_port_usb_vid_pid(port, &vid, &pid);
    
    // M8 VID/PID
    if (vid == 0x16C0 && pid == 0x048A) {
        sp_copy_port(port, &m8_port);
        printf("Found M8 at %s\n", sp_get_port_name(port));
        break;
    }
  }
  sp_free_port_list(port_list);

  if (!m8_port) return 0;

  if (!check(sp_open(m8_port, SP_MODE_READ_WRITE))) return 0;
  if (!check(sp_set_baudrate(m8_port, 115200))) return 0;
  
  init_queue(&queue);
  thread_should_stop = 0;
  pthread_create(&serial_thread, NULL, thread_process_serial_data, NULL);

  return 1;
}

int m8_close() {
    thread_should_stop = 1;
    pthread_join(serial_thread, NULL);
    destroy_queue(&queue);
    
    if (m8_port) {
        sp_close(m8_port);
        sp_free_port(m8_port);
        m8_port = NULL;
    }
    return 1;
}

int m8_send_msg_controller(const uint8_t input) {
  if (!m8_port) return -1;
  const unsigned char buf[2] = {'C', input};
  return sp_blocking_write(m8_port, buf, 2, 5);
}

int m8_send_msg_keyjazz(const uint8_t note, uint8_t velocity) {
  if (!m8_port) return -1;
  if (velocity > 0x7F) velocity = 0x7F;
  const unsigned char buf[3] = {'K', note, velocity};
  return sp_blocking_write(m8_port, buf, 3, 5);
}

int m8_enable_display(const unsigned char reset_display) {
  if (!m8_port) return 0;
  char buf[1] = {'E'};
  sp_blocking_write(m8_port, buf, 1, 5);
  usleep(500000);
  if (reset_display) {
      char buf2[1] = {'R'};
      sp_blocking_write(m8_port, buf2, 1, 5);
  }
  return 1;
}

int m8_process_data(const config_params_s *conf) {
  if (!m8_port) return DEVICE_DISCONNECTED;

  if (queue_size(&queue) > 0) {
    unsigned char *command;
    size_t length = 0;
    while ((command = pop_message(&queue, &length)) != NULL) {
      if (length > 0) process_command(command, length);
      free(command);
    }
  }
  return DEVICE_PROCESSING;
}
