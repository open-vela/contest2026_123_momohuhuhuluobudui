/* SPDX-License-Identifier: Apache-2.0 */
#include "agentguard/button_gesture.h"
#include <string.h>
#define AG_BUTTON_PRIVACY_PRESS_MS 3000
#define AG_BUTTON_DEMO_PRESS_MS 8000

void ag_button_gesture_init(struct ag_button_gesture *state)
{
  memset(state, 0, sizeof(*state));
}

enum ag_button_gesture_event ag_button_gesture_update(
  struct ag_button_gesture *state, bool pressed, uint64_t now_ms)
{
  if (pressed && !state->pressed)
    {
      state->pressed = true;
      state->pressed_since_ms = now_ms;
    }
  if (!pressed && state->pressed)
    {
      uint64_t elapsed_ms = now_ms >= state->pressed_since_ms ?
        now_ms - state->pressed_since_ms : 0;
      state->pressed = false;
      if (elapsed_ms >= AG_BUTTON_DEMO_PRESS_MS)
        {
          return AG_BUTTON_GESTURE_TOGGLE_DEMO;
        }
      if (elapsed_ms >= AG_BUTTON_PRIVACY_PRESS_MS)
        {
          return AG_BUTTON_GESTURE_TOGGLE_PRIVACY;
        }
      return AG_BUTTON_GESTURE_ACK;
    }
  return AG_BUTTON_GESTURE_NONE;
}
