/* SPDX-License-Identifier: Apache-2.0 */
#include "agentguard/serial_event.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
  char output[96];
  const char *event = "sedentary_alert";
  int length = ag_serial_event_format(output, sizeof(output), event);
  assert(length > 0);
  assert(length < 64);
  assert(strcmp(output,
    "AGENTGUARD_EVENT {\"event\":\"sedentary_alert\"}\n") == 0);
  assert(ag_serial_event_format(output, 8, event) < 0);
  assert(ag_serial_event_try_write("/dev/null", event) == 0);
  assert(ag_serial_event_try_write("/path/that/does/not/exist", event) < 0);
  puts("AgentGuard serial event tests: PASS");
  return 0;
}
