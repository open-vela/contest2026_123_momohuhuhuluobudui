/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/vision_health.h"

#include <assert.h>
#include <stdio.h>

static void test_untrusted_results_cannot_expose_faces(void)
{
  struct ag_vision_health_state state;
  struct ag_vision_result result =
  {
    .face_count = 2,
    .face_box_count = 2,
    .posture_score = 90,
    .primary_face = {10, 20, 30, 40},
    .face_boxes = {{10, 20, 30, 40}, {50, 60, 70, 80}},
  };

  ag_vision_health_init(&state);
  assert(!ag_vision_health_gate(&state, false, &result));
  assert(result.face_count == 0);
  assert(result.face_box_count == 0);
  assert(result.primary_face.width == 0);
  assert(result.posture_score == 0);
  assert(result.face_boxes[0].width == 0);
  assert(result.face_boxes[1].width == 0);

  ag_vision_health_update(&state, false);
  ag_vision_health_update(&state, false);
  ag_vision_health_update(&state, false);
  ag_vision_health_update(&state, true);
  result.face_count = 1;
  result.face_box_count = 1;
  assert(!ag_vision_health_gate(&state, true, &result));
  assert(result.face_count == 0);
  assert(result.face_box_count == 0);

  ag_vision_health_update(&state, true);
  result.face_count = 1;
  result.face_box_count = 1;
  assert(ag_vision_health_gate(&state, true, &result));
  assert(result.face_count == 1);
  assert(result.face_box_count == 1);
}

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
  test_untrusted_results_cannot_expose_faces();
  puts("AgentGuard vision health tests: PASS");
  return 0;
}
