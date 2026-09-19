/* SPDX-License-Identifier: Apache-2.0 */
#define _POSIX_C_SOURCE 200809L
#include "agentguard/led_device.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int register_fixture(const char *path)
{
  int fd = open(path, O_CREAT | O_EXCL | O_RDWR, 0600);
  assert(fd >= 0);
  close(fd);
  return 0;
}

static int registration_fails(const char *path)
{
  (void)path;
  return -EIO;
}

int main(void)
{
  char directory[] = "/tmp/agentguard-led-test.XXXXXX";
  char path[160];
  int fd;

  assert(mkdtemp(directory) != NULL);
  snprintf(path, sizeof(path), "%s/userleds", directory);

  fd = ag_led_open_device(path, registration_fails);
  assert(fd == -1 && errno == EIO);

  fd = ag_led_open_device(path, register_fixture);
  assert(fd >= 0);
  assert((fcntl(fd, F_GETFL) & O_ACCMODE) == O_WRONLY);
  close(fd);

  /* An existing device must be reused without registering again. */

  fd = ag_led_open_device(path, registration_fails);
  assert(fd >= 0);
  close(fd);

  assert(unlink(path) == 0);
  assert(rmdir(directory) == 0);
  puts("AgentGuard LED device tests: PASS");
  return 0;
}
