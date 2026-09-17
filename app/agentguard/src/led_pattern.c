/* SPDX-License-Identifier: Apache-2.0 */
#include "agentguard/led_pattern.h"

bool ag_led_pattern_enabled(enum ag_led_pattern pattern, uint64_t now_ms)
{
  uint64_t phase;

  switch (pattern)
    {
      case AG_LED_PATTERN_SEDENTARY:
        /* One long, calm pulse every 1.6 seconds. */
        return now_ms % 1600u < 500u;

      case AG_LED_PATTERN_PRIVACY:
        /* A distinctive double flash every 0.8 seconds. */
        phase = now_ms % 800u;
        return phase < 120u || (phase >= 240u && phase < 360u);

      case AG_LED_PATTERN_ATTENTION:
        return now_ms % 400u < 200u;

      case AG_LED_PATTERN_OFF:
      default:
        return false;
    }
}
