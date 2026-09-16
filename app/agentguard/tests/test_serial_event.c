/* SPDX-License-Identifier: Apache-2.0 */
#include "agentguard/serial_event.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

static char g_written[96];
static size_t g_written_size;

static ssize_t partial_writer(int fd, const void *buffer, size_t size)
{
  size_t chunk = size > 7 ? 7 : size;
  (void)fd;
  memcpy(g_written + g_written_size, buffer, chunk);
  g_written_size += chunk;
  return (ssize_t)chunk;
}

int main(void)
{
  struct ag_serial_event_sender sender;
  struct ag_serial_event_queue queue;
  char queued[64];
  size_t queued_size;
  char output[96];
  const char *event = "sedentary_alert";
  int length = ag_serial_event_format(output, sizeof(output), event);
  assert(length > 0);
  assert(length < 64);
  assert(strcmp(output,
    "AGENTGUARD_EVENT {\"event\":\"sedentary_alert\"}\n") == 0);
  assert(ag_serial_event_format(output, 8, event) < 0);
  assert(ag_serial_event_write_all(1, output, (size_t)length,
                                   partial_writer) == 0);
  assert(g_written_size == (size_t)length);
  assert(memcmp(g_written, output, (size_t)length) == 0);
  ag_serial_event_queue_init(&queue);
  assert(ag_serial_event_queue_push(&queue, output, (size_t)length) == 0);
  assert(ag_serial_event_queue_push(&queue, output, (size_t)length) == 0);
  assert(ag_serial_event_queue_push(&queue, output, (size_t)length) == 0);
  assert(ag_serial_event_queue_push(&queue, output, (size_t)length) == 0);
  assert(ag_serial_event_queue_push(&queue, "partial", 7) < 0);
  assert(ag_serial_event_queue_pop(&queue, queued, sizeof(queued),
                                   &queued_size) == 0);
  assert(queued_size == (size_t)length);
  assert(memcmp(queued, output, queued_size) == 0);
  assert(ag_serial_event_sender_start(&sender, "/dev/null") == 0);
  assert(ag_serial_event_sender_submit(&sender, event) == 0);
  ag_serial_event_sender_stop(&sender);
  puts("AgentGuard serial event tests: PASS");
  return 0;
}
