#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stddef.h>

#define MAX_QUEUE_SIZE 8192

typedef struct {
  unsigned char *messages[MAX_QUEUE_SIZE];
  size_t lengths[MAX_QUEUE_SIZE];
  int front;
  int rear;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
} message_queue_s;

void init_queue(message_queue_s *queue);
void destroy_queue(message_queue_s *queue);
unsigned char *pop_message(message_queue_s *queue, size_t *length);
void push_message(message_queue_s *queue, const unsigned char *message, size_t length);
unsigned int queue_size(const message_queue_s *queue);

#endif
