/* SPDX-License-Identifier: Apache-2.0 */
#ifndef AGENTGUARD_SERIAL_EVENT_H
#define AGENTGUARD_SERIAL_EVENT_H
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

typedef ssize_t (*ag_serial_event_write_fn)(int fd, const void *buffer,
                                             size_t size);

#define AG_SERIAL_EVENT_RECORD_SIZE 64
#define AG_SERIAL_EVENT_QUEUE_CAPACITY 4

struct ag_serial_event_queue
{
  char records[AG_SERIAL_EVENT_QUEUE_CAPACITY][AG_SERIAL_EVENT_RECORD_SIZE];
  size_t lengths[AG_SERIAL_EVENT_QUEUE_CAPACITY];
  unsigned int head;
  unsigned int count;
};

struct ag_serial_event_sender
{
  pthread_t thread;
  pthread_mutex_t lock;
  pthread_cond_t condition;
  struct ag_serial_event_queue queue;
  const char *path;
  bool started;
  bool stopping;
};

int ag_serial_event_format(char *buffer, size_t size,
                           const char *event_name);
int ag_serial_event_write_all(int fd, const void *buffer, size_t size,
                              ag_serial_event_write_fn writer);
void ag_serial_event_queue_init(struct ag_serial_event_queue *queue);
int ag_serial_event_queue_push(struct ag_serial_event_queue *queue,
                               const void *record, size_t size);
int ag_serial_event_queue_pop(struct ag_serial_event_queue *queue,
                              void *record, size_t capacity, size_t *size);
int ag_serial_event_sender_start(struct ag_serial_event_sender *sender,
                                 const char *path);
int ag_serial_event_sender_submit(struct ag_serial_event_sender *sender,
                                  const char *event_name);
void ag_serial_event_sender_stop(struct ag_serial_event_sender *sender);
#endif
