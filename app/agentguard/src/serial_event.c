/* SPDX-License-Identifier: Apache-2.0 */
#include "agentguard/serial_event.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define AG_SERIAL_EVENT_RECORD_SIZE 64

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

int ag_serial_event_try_write(const char *path, const char *event_name)
{
  char record[AG_SERIAL_EVENT_RECORD_SIZE];
  ssize_t written;
  int length;
  int fd;

  if (path == NULL)
    {
      return -1;
    }

  length = ag_serial_event_format(record, sizeof(record), event_name);
  if (length < 0)
    {
      return -1;
    }

  fd = open(path, O_WRONLY | O_NONBLOCK | O_NOCTTY);
  if (fd < 0)
    {
      return -1;
    }

  do
    {
      written = write(fd, record, (size_t)length);
    }
  while (written < 0 && errno == EINTR);

  close(fd);
  return written == (ssize_t)length ? 0 : -1;
}
