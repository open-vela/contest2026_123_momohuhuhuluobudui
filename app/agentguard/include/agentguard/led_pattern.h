/* SPDX-License-Identifier: Apache-2.0 */
#ifndef AGENTGUARD_LED_PATTERN_H
#define AGENTGUARD_LED_PATTERN_H

#include <stdbool.h>
#include <stdint.h>

enum ag_led_pattern
{
  AG_LED_PATTERN_OFF = 0,
  AG_LED_PATTERN_SEDENTARY,
  AG_LED_PATTERN_PRIVACY,
  AG_LED_PATTERN_ATTENTION,
};

bool ag_led_pattern_enabled(enum ag_led_pattern pattern, uint64_t now_ms);

#endif
