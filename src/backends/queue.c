#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void init_queue(message_queue_s *queue) {
    queue->front = 0;
    queue->rear = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

void destroy_queue(message_queue_s *queue) {
  pthread_mutex_lock(&queue->mutex);
  while (queue->front != queue->rear) {
    free(queue->messages[queue->front]);
    queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
  }
  pthread_mutex_unlock(&queue->mutex);
  pthread_mutex_destroy(&queue->mutex);
  pthread_cond_destroy(&queue->cond);
}

void push_message(message_queue_s *queue, const unsigned char *message, size_t length) {
    pthread_mutex_lock(&queue->mutex);
    if ((queue->rear + 1) % MAX_QUEUE_SIZE != queue->front) {
        queue->messages[queue->rear] = malloc(length);
        memcpy(queue->messages[queue->rear], message, length);
        queue->lengths[queue->rear] = length;
        queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
        pthread_cond_signal(&queue->cond);
    }
    pthread_mutex_unlock(&queue->mutex);
}

unsigned char *pop_message(message_queue_s *queue, size_t *length) {
  pthread_mutex_lock(&queue->mutex);
  if (queue->front == queue->rear) {
    pthread_mutex_unlock(&queue->mutex);
    return NULL;
  }
  *length = queue->lengths[queue->front];
  unsigned char *message = queue->messages[queue->front];
  queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
  pthread_mutex_unlock(&queue->mutex);
  return message;
}

unsigned int queue_size(const message_queue_s *queue) {
  pthread_mutex_lock(&queue->mutex);
  unsigned int size = (queue->rear - queue->front + MAX_QUEUE_SIZE) % MAX_QUEUE_SIZE;
  pthread_mutex_unlock(&queue->mutex);
  return size;
}
