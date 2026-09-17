/* SPDX-License-Identifier: Apache-2.0 */
#include "agentguard/led_pattern.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
  assert(!ag_led_pattern_enabled(AG_LED_PATTERN_OFF, 0));
  assert(ag_led_pattern_enabled(AG_LED_PATTERN_SEDENTARY, 100));
  assert(!ag_led_pattern_enabled(AG_LED_PATTERN_SEDENTARY, 800));
  assert(ag_led_pattern_enabled(AG_LED_PATTERN_PRIVACY, 50));
  assert(!ag_led_pattern_enabled(AG_LED_PATTERN_PRIVACY, 180));
  assert(ag_led_pattern_enabled(AG_LED_PATTERN_PRIVACY, 300));
  assert(!ag_led_pattern_enabled(AG_LED_PATTERN_PRIVACY, 500));
  puts("AgentGuard LED pattern tests: PASS");
  return 0;
}
