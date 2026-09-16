/* SPDX-License-Identifier: Apache-2.0 */
#include "agentguard/serial_event.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int ag_serial_event_format(char *buffer, size_t size,
                           const char *event_name)
{
  int length;
  if (buffer == NULL || size == 0 || event_name == NULL)
    {
      return -1;
    }

  length = snprintf(buffer, size, "AGENTGUARD_EVENT {\"event\":\"%s\"}\n",
                    event_name);
  return length > 0 && (size_t)length < size ? length : -1;
}

int ag_serial_event_write_all(int fd, const void *buffer, size_t size,
                              ag_serial_event_write_fn writer)
{
  const char *cursor = buffer;
  size_t remaining = size;
  ssize_t written;

  if (buffer == NULL || writer == NULL)
    {
      return -1;
    }

  while (remaining > 0)
    {
      do
        {
          written = writer(fd, cursor, remaining);
        }
      while (written < 0 && errno == EINTR);

      if (written <= 0)
        {
          return -1;
        }

      cursor += written;
      remaining -= (size_t)written;
    }

  return 0;
}

void ag_serial_event_queue_init(struct ag_serial_event_queue *queue)
{
  if (queue != NULL)
    {
      memset(queue, 0, sizeof(*queue));
    }
}

int ag_serial_event_queue_push(struct ag_serial_event_queue *queue,
                               const void *record, size_t size)
{
  unsigned int tail;

  if (queue == NULL || record == NULL || size == 0 ||
      size > AG_SERIAL_EVENT_RECORD_SIZE ||
      queue->count >= AG_SERIAL_EVENT_QUEUE_CAPACITY)
    {
      return -1;
    }

  tail = (queue->head + queue->count) % AG_SERIAL_EVENT_QUEUE_CAPACITY;
  memcpy(queue->records[tail], record, size);
  queue->lengths[tail] = size;
  queue->count++;
  return 0;
}

int ag_serial_event_queue_pop(struct ag_serial_event_queue *queue,
                              void *record, size_t capacity, size_t *size)
{
  size_t length;

  if (queue == NULL || record == NULL || size == NULL || queue->count == 0)
    {
      return -1;
    }

  length = queue->lengths[queue->head];
  if (capacity < length)
    {
      return -1;
    }

  memcpy(record, queue->records[queue->head], length);
  *size = length;
  queue->head = (queue->head + 1) % AG_SERIAL_EVENT_QUEUE_CAPACITY;
  queue->count--;
  return 0;
}

static void *ag_serial_event_worker(void *argument)
{
  struct ag_serial_event_sender *sender = argument;
  char record[AG_SERIAL_EVENT_RECORD_SIZE];
  size_t length;

  for (;;)
    {
      pthread_mutex_lock(&sender->lock);
      while (sender->queue.count == 0 && !sender->stopping)
        {
          pthread_cond_wait(&sender->condition, &sender->lock);
        }

      if (sender->queue.count == 0 && sender->stopping)
        {
          pthread_mutex_unlock(&sender->lock);
          break;
        }

      (void)ag_serial_event_queue_pop(&sender->queue, record,
                                      sizeof(record), &length);
      pthread_mutex_unlock(&sender->lock);

      int fd = open(sender->path, O_WRONLY | O_NOCTTY);
      if (fd >= 0)
        {
          (void)ag_serial_event_write_all(fd, record, length, write);
          close(fd);
        }
    }

  return NULL;
}

int ag_serial_event_sender_start(struct ag_serial_event_sender *sender,
                                 const char *path)
{
  if (sender == NULL || path == NULL)
    {
      return -1;
    }

  memset(sender, 0, sizeof(*sender));
  sender->path = path;
  ag_serial_event_queue_init(&sender->queue);
  if (pthread_mutex_init(&sender->lock, NULL) != 0)
    {
      return -1;
    }

  if (pthread_cond_init(&sender->condition, NULL) != 0)
    {
      pthread_mutex_destroy(&sender->lock);
      return -1;
    }

  if (pthread_create(&sender->thread, NULL, ag_serial_event_worker,
                     sender) != 0)
    {
      pthread_cond_destroy(&sender->condition);
      pthread_mutex_destroy(&sender->lock);
      return -1;
    }

  sender->started = true;
  return 0;
}

int ag_serial_event_sender_submit(struct ag_serial_event_sender *sender,
                                  const char *event_name)
{
  char record[AG_SERIAL_EVENT_RECORD_SIZE];
  int length;
  int result;

  if (sender == NULL || !sender->started)
    {
      return -1;
    }

  length = ag_serial_event_format(record, sizeof(record), event_name);
  if (length < 0)
    {
      return -1;
    }

  if (pthread_mutex_trylock(&sender->lock) != 0)
    {
      return -1;
    }

  result = ag_serial_event_queue_push(&sender->queue, record,
                                      (size_t)length);
  if (result == 0)
    {
      pthread_cond_signal(&sender->condition);
    }
  pthread_mutex_unlock(&sender->lock);
  return result;
}

void ag_serial_event_sender_stop(struct ag_serial_event_sender *sender)
{
  if (sender == NULL || !sender->started)
    {
      return;
    }

  pthread_mutex_lock(&sender->lock);
  sender->stopping = true;
  pthread_cond_signal(&sender->condition);
  pthread_mutex_unlock(&sender->lock);
  pthread_join(sender->thread, NULL);
  pthread_cond_destroy(&sender->condition);
  pthread_mutex_destroy(&sender->lock);
  sender->started = false;
}
