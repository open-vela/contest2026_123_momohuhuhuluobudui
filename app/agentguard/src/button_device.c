/* SPDX-License-Identifier: Apache-2.0 */
#include "agentguard/button_device.h"
#include <errno.h>
#include <fcntl.h>

int ag_button_open_device(const char *path,
                          int (*register_device)(const char *path))
{
  int fd = open(path, O_RDONLY | O_NONBLOCK);
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
  return open(path, O_RDONLY | O_NONBLOCK);
}
