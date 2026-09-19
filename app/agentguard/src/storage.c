/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/storage.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define AG_LOG_LINE_MAX 512
#define AG_LOG_PATH_MAX 256
#define AG_LOG_READ_BUFFER 256

struct ag_log_reader
{
  int fd;
  char data[AG_LOG_READ_BUFFER];
  size_t offset;
  size_t available;
};

static int ag_write_all(int fd, const char *data, size_t length)
{
  while (length > 0)
    {
      ssize_t written = write(fd, data, length);

      if (written < 0)
        {
          if (errno == EINTR)
            {
              continue;
            }

          return -1;
        }

      if (written == 0)
        {
          errno = EIO;
          return -1;
        }

      data += written;
      length -= (size_t)written;
    }

  return 0;
}

static int ag_read_byte(struct ag_log_reader *reader, char *byte)
{
  while (reader->offset == reader->available)
    {
      ssize_t result = read(reader->fd, reader->data, sizeof(reader->data));

      if (result < 0)
        {
          if (errno == EINTR)
            {
              continue;
            }

          return -1;
        }

      if (result == 0)
        {
          return 0;
        }

      reader->offset = 0;
      reader->available = (size_t)result;
    }

  *byte = reader->data[reader->offset++];
  return 1;
}

static int ag_read_line(struct ag_log_reader *reader, char *line, size_t size,
                        size_t *stored,
                        size_t *consumed, bool *truncated)
{
  char byte;

  *stored = 0;
  *consumed = 0;
  *truncated = false;
  for (;;)
    {
      int result = ag_read_byte(reader, &byte);

      if (result < 0)
        {
          if (errno == EINTR)
            {
              continue;
            }

          return -1;
        }

      if (result == 0)
        {
          break;
        }

      (*consumed)++;
      if (*stored + 1 < size)
        {
          line[(*stored)++] = byte;
        }
      else
        {
          *truncated = true;
        }

      if (byte == '\n')
        {
          break;
        }
    }

  line[*stored] = '\0';
  return *consumed == 0 ? 0 : 1;
}

static bool ag_parse_unix_ms(const char *line, uint64_t *unix_ms)
{
  const char *field = strstr(line, "\"unix_ms\":");
  char *end;
  unsigned long long value;

  if (field == NULL)
    {
      return false;
    }

  field += strlen("\"unix_ms\":");
  errno = 0;
  value = strtoull(field, &end, 10);
  if (errno != 0 || end == field)
    {
      return false;
    }

  *unix_ms = (uint64_t)value;
  return true;
}

static int ag_event_index(enum ag_event event)
{
  unsigned int index;

  for (index = 0; index < AG_STORAGE_EVENT_SLOTS; index++)
    {
      if (event == (enum ag_event)(1u << index))
        {
          return (int)index;
        }
    }

  return -1;
}

static enum ag_event ag_parse_event(const char *line)
{
  unsigned int index;

  for (index = 0; index < AG_STORAGE_EVENT_SLOTS; index++)
    {
      enum ag_event event = (enum ag_event)(1u << index);
      char needle[64];
      int length;

      length = snprintf(needle, sizeof(needle), "\"event\":\"%s\"",
                        ag_event_name(event));
      if (length > 0 && length < (int)sizeof(needle) &&
          strstr(line, needle) != NULL)
        {
          return event;
        }
    }

  return AG_EVENT_NONE;
}

static int ag_copy_retained(int source_fd, int target_fd,
                            uint64_t cutoff_unix_ms, bool use_cutoff,
                            size_t skip_bytes, size_t *written_bytes)
{
  char line[AG_LOG_LINE_MAX];
  struct ag_log_reader reader = {.fd = source_fd};
  size_t source_offset = 0;
  bool skip_partial = skip_bytes > 0;

  *written_bytes = 0;
  for (;;)
    {
      bool truncated;
      uint64_t unix_ms;
      size_t stored;
      size_t consumed;
      size_t line_start = source_offset;
      int result = ag_read_line(&reader, line, sizeof(line), &stored,
                                &consumed, &truncated);

      if (result < 0)
        {
          return -1;
        }

      if (result == 0)
        {
          return 0;
        }

      source_offset += consumed;
      if (truncated)
        {
          continue;
        }

      if (skip_partial)
        {
          if (line_start < skip_bytes)
            {
              continue;
            }

          skip_partial = false;
        }

      if (use_cutoff && ag_parse_unix_ms(line, &unix_ms) &&
          unix_ms >= AG_STORAGE_VALID_UNIX_MS && unix_ms < cutoff_unix_ms)
        {
          continue;
        }

      if (ag_write_all(target_fd, line, stored) < 0)
        {
          return -1;
        }

      *written_bytes += stored;
    }
}

static int ag_replace_log(const char *path, const char *temporary)
{
  if (rename(temporary, path) < 0)
    {
      int saved = errno;
      unlink(temporary);
      errno = saved;
      return -1;
    }

  return 0;
}

int ag_log_compact(const char *path, uint64_t now_unix_ms,
                   const struct ag_log_policy *policy)
{
  char first_path[AG_LOG_PATH_MAX];
  char second_path[AG_LOG_PATH_MAX];
  uint64_t cutoff = 0;
  size_t written = 0;
  size_t ignored = 0;
  bool use_cutoff;
  int source_fd;
  int target_fd;
  int length;

  if (path == NULL || policy == NULL || policy->max_bytes == 0)
    {
      errno = EINVAL;
      return -1;
    }

  source_fd = open(path, O_RDONLY);
  if (source_fd < 0)
    {
      return errno == ENOENT ? 0 : -1;
    }

  length = snprintf(first_path, sizeof(first_path), "%s.tmp", path);
  if (length <= 0 || length >= (int)sizeof(first_path))
    {
      close(source_fd);
      errno = ENAMETOOLONG;
      return -1;
    }

  target_fd = open(first_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (target_fd < 0)
    {
      close(source_fd);
      return -1;
    }

  use_cutoff = now_unix_ms >= AG_STORAGE_VALID_UNIX_MS &&
               policy->retention_ms > 0 &&
               now_unix_ms >= policy->retention_ms;
  if (use_cutoff)
    {
      cutoff = now_unix_ms - policy->retention_ms;
    }

  if (ag_copy_retained(source_fd, target_fd, cutoff, use_cutoff, 0,
                       &written) < 0)
    {
      int saved = errno;
      close(source_fd);
      close(target_fd);
      unlink(first_path);
      errno = saved;
      return -1;
    }

  close(source_fd);
  if (close(target_fd) < 0)
    {
      unlink(first_path);
      return -1;
    }

  if (written <= policy->max_bytes)
    {
      return ag_replace_log(path, first_path);
    }

  length = snprintf(second_path, sizeof(second_path), "%s.trim", path);
  if (length <= 0 || length >= (int)sizeof(second_path))
    {
      unlink(first_path);
      errno = ENAMETOOLONG;
      return -1;
    }

  source_fd = open(first_path, O_RDONLY);
  target_fd = open(second_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (source_fd < 0 || target_fd < 0 ||
      ag_copy_retained(source_fd, target_fd, 0, false,
                       written - policy->max_bytes, &ignored) < 0)
    {
      int saved = errno;
      if (source_fd >= 0) close(source_fd);
      if (target_fd >= 0) close(target_fd);
      unlink(first_path);
      unlink(second_path);
      errno = saved;
      return -1;
    }

  close(source_fd);
  if (close(target_fd) < 0)
    {
      unlink(first_path);
      unlink(second_path);
      return -1;
    }

  unlink(first_path);
  return ag_replace_log(path, second_path);
}

int ag_log_append(const char *path, const char *event_json,
                  uint64_t unix_ms, const struct ag_log_policy *policy)
{
  char record[AG_LOG_LINE_MAX];
  struct stat status;
  bool compact = false;
  int fd;
  int length;

  if (path == NULL || event_json == NULL || policy == NULL)
    {
      errno = EINVAL;
      return -1;
    }

  length = snprintf(record, sizeof(record),
                    "{\"unix_ms\":%llu,\"record\":%s}\n",
                    (unsigned long long)unix_ms, event_json);
  if (length <= 0 || length >= (int)sizeof(record) ||
      (size_t)length > policy->max_bytes)
    {
      errno = EOVERFLOW;
      return -1;
    }

  fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd < 0)
    {
      return -1;
    }

  if (ag_write_all(fd, record, (size_t)length) < 0)
    {
      int saved = errno;
      close(fd);
      errno = saved;
      return -1;
    }

  if (fstat(fd, &status) == 0 &&
      (size_t)status.st_size > policy->max_bytes)
    {
      compact = true;
    }

  if (close(fd) < 0)
    {
      return -1;
    }

  return compact ? ag_log_compact(path, unix_ms, policy) : 0;
}

int ag_log_read_stats(const char *path, struct ag_log_stats *stats)
{
  char line[AG_LOG_LINE_MAX];
  struct ag_log_reader reader;
  int fd;

  if (path == NULL || stats == NULL)
    {
      errno = EINVAL;
      return -1;
    }

  memset(stats, 0, sizeof(*stats));
  fd = open(path, O_RDONLY);
  if (fd < 0)
    {
      return -1;
    }

  memset(&reader, 0, sizeof(reader));
  reader.fd = fd;

  for (;;)
    {
      bool truncated;
      uint64_t unix_ms;
      enum ag_event event;
      int index;
      size_t stored;
      size_t consumed;
      int result = ag_read_line(&reader, line, sizeof(line), &stored,
                                &consumed,
                                &truncated);

      (void)stored;
      (void)consumed;

      if (result < 0)
        {
          int saved = errno;
          close(fd);
          errno = saved;
          return -1;
        }

      if (result == 0)
        {
          break;
        }

      event = truncated ? AG_EVENT_NONE : ag_parse_event(line);
      index = ag_event_index(event);
      if (index < 0)
        {
          stats->malformed_lines++;
          continue;
        }

      stats->event_counts[index]++;
      stats->total_events++;
      if (ag_parse_unix_ms(line, &unix_ms) &&
          unix_ms >= AG_STORAGE_VALID_UNIX_MS)
        {
          if (stats->first_unix_ms == 0 || unix_ms < stats->first_unix_ms)
            {
              stats->first_unix_ms = unix_ms;
            }

          if (unix_ms > stats->last_unix_ms)
            {
              stats->last_unix_ms = unix_ms;
            }
        }
    }

  return close(fd);
}

uint32_t ag_log_event_count(const struct ag_log_stats *stats,
                            enum ag_event event)
{
  int index = ag_event_index(event);

  return stats != NULL && index >= 0 ? stats->event_counts[index] : 0;
}
