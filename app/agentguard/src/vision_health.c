/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/vision_health.h"

#include <string.h>

void ag_vision_health_init(struct ag_vision_health_state *state)
{
  memset(state, 0, sizeof(*state));
}

bool ag_vision_health_gate(const struct ag_vision_health_state *state,
                           bool inference_ok, struct ag_vision_result *result)
{
  if (!inference_ok || state->error_active)
    {
      memset(result, 0, sizeof(*result));
      return false;
    }

  return true;
}

enum ag_vision_health_transition ag_vision_health_update(
  struct ag_vision_health_state *state, bool inference_ok)
{
  if (!state->error_active)
    {
      state->consecutive_successes = 0;
      if (inference_ok)
        {
          state->consecutive_failures = 0;
        }
      else if (state->consecutive_failures < AG_VISION_FAILURE_LIMIT)
        {
          state->consecutive_failures++;
          if (state->consecutive_failures == AG_VISION_FAILURE_LIMIT)
            {
              state->error_active = true;
              return AG_VISION_HEALTH_ENTERED_ERROR;
            }
        }

      return AG_VISION_HEALTH_NO_CHANGE;
    }

  state->consecutive_failures = 0;
  if (!inference_ok)
    {
      state->consecutive_successes = 0;
    }
  else if (state->consecutive_successes < AG_VISION_RECOVERY_LIMIT)
    {
      state->consecutive_successes++;
      if (state->consecutive_successes == AG_VISION_RECOVERY_LIMIT)
        {
          state->consecutive_successes = 0;
          state->error_active = false;
          return AG_VISION_HEALTH_RECOVERED;
        }
    }

  return AG_VISION_HEALTH_NO_CHANGE;
}
