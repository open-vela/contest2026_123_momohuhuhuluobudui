/* SPDX-License-Identifier: Apache-2.0 */
#include "agentguard/led_device.h"

#include <errno.h>
#include <fcntl.h>

int ag_led_open_device(const char *path,
                       int (*register_device)(const char *path))
{
  int fd = open(path, O_WRONLY);
  int result;

  if (fd >= 0 || errno != ENOENT)
    {
      return fd;
    }

  result = register_device(path);
  if (result < 0)
    {
      errno = -result;
      return -1;
    }

  return open(path, O_WRONLY);
}
