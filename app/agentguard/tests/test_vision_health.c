/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/vision_health.h"

#include <assert.h>
#include <stdio.h>

static void test_entry_requires_three_consecutive_failures(void)
{
  struct ag_vision_health_state state;

  ag_vision_health_init(&state);
  assert(!state.error_active);
  assert(ag_vision_health_update(&state, false) ==
         AG_VISION_HEALTH_NO_CHANGE);
  assert(ag_vision_health_update(&state, true) ==
         AG_VISION_HEALTH_NO_CHANGE);
  assert(state.consecutive_failures == 0);
  assert(ag_vision_health_update(&state, false) ==
         AG_VISION_HEALTH_NO_CHANGE);
  assert(ag_vision_health_update(&state, false) ==
         AG_VISION_HEALTH_NO_CHANGE);
  assert(ag_vision_health_update(&state, false) ==
         AG_VISION_HEALTH_ENTERED_ERROR);
  assert(state.error_active);
  assert(ag_vision_health_update(&state, false) ==
         AG_VISION_HEALTH_NO_CHANGE);
}

static void test_recovery_requires_two_consecutive_successes(void)
{
  struct ag_vision_health_state state;

  ag_vision_health_init(&state);
  ag_vision_health_update(&state, false);
  ag_vision_health_update(&state, false);
  assert(ag_vision_health_update(&state, false) ==
         AG_VISION_HEALTH_ENTERED_ERROR);

  assert(ag_vision_health_update(&state, true) ==
         AG_VISION_HEALTH_NO_CHANGE);
  assert(ag_vision_health_update(&state, false) ==
         AG_VISION_HEALTH_NO_CHANGE);
  assert(state.consecutive_successes == 0);
  assert(ag_vision_health_update(&state, true) ==
         AG_VISION_HEALTH_NO_CHANGE);
  assert(ag_vision_health_update(&state, true) ==
         AG_VISION_HEALTH_RECOVERED);
  assert(!state.error_active);
  assert(ag_vision_health_update(&state, true) ==
         AG_VISION_HEALTH_NO_CHANGE);
}

int main(void)
{
  test_entry_requires_three_consecutive_failures();
  test_recovery_requires_two_consecutive_successes();
  puts("AgentGuard vision health tests: PASS");
  return 0;
}
