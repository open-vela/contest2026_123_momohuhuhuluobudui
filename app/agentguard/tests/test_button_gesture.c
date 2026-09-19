/* SPDX-License-Identifier: Apache-2.0 */
#include "agentguard/button_gesture.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
  struct ag_button_gesture state;
  ag_button_gesture_init(&state);
  assert(ag_button_gesture_update(&state, false, 0) == AG_BUTTON_GESTURE_NONE);
  assert(ag_button_gesture_update(&state, true, 100) == AG_BUTTON_GESTURE_NONE);
  assert(ag_button_gesture_update(&state, false, 500) == AG_BUTTON_GESTURE_ACK);
  assert(ag_button_gesture_update(&state, true, 1000) == AG_BUTTON_GESTURE_NONE);
  assert(ag_button_gesture_update(&state, true, 4000) == AG_BUTTON_GESTURE_NONE);
  assert(ag_button_gesture_update(&state, false, 5000) ==
         AG_BUTTON_GESTURE_TOGGLE_PRIVACY);
  assert(ag_button_gesture_update(&state, true, 6000) == AG_BUTTON_GESTURE_NONE);
  assert(ag_button_gesture_update(&state, true, 13999) == AG_BUTTON_GESTURE_NONE);
  assert(ag_button_gesture_update(&state, false, 14000) ==
         AG_BUTTON_GESTURE_TOGGLE_DEMO);
  puts("AgentGuard button gesture tests: PASS");
  return 0;
}
