/* SPDX-License-Identifier: Apache-2.0 */
#include "agentguard/serial_event.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
  char output[96];
  const char *json = "{\"event\":\"sedentary_alert\"}";
  int length = ag_serial_event_format(output, sizeof(output), json);
  assert(length > 0);
  assert(strcmp(output,
    "AGENTGUARD_EVENT {\"event\":\"sedentary_alert\"}\n") == 0);
  assert(ag_serial_event_format(output, 8, json) < 0);
  assert(ag_serial_event_try_write("/dev/null", json) == 0);
  assert(ag_serial_event_try_write("/path/that/does/not/exist", json) < 0);
  puts("AgentGuard serial event tests: PASS");
  return 0;
}
