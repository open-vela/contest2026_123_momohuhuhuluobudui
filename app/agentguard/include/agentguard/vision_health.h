/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_VISION_HEALTH_H
#define AGENTGUARD_VISION_HEALTH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AG_VISION_FAILURE_LIMIT 3
#define AG_VISION_RECOVERY_LIMIT 2

enum ag_vision_health_transition
{
  AG_VISION_HEALTH_NO_CHANGE = 0,
  AG_VISION_HEALTH_ENTERED_ERROR,
  AG_VISION_HEALTH_RECOVERED,
};

struct ag_vision_health_state
{
  uint8_t consecutive_failures;
  uint8_t consecutive_successes;
  bool error_active;
};

void ag_vision_health_init(struct ag_vision_health_state *state);
enum ag_vision_health_transition ag_vision_health_update(
  struct ag_vision_health_state *state, bool inference_ok);

#ifdef __cplusplus
}
#endif

#endif
