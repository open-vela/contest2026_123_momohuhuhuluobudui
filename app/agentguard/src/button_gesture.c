/* SPDX-License-Identifier: Apache-2.0 */
#include "agentguard/button_gesture.h"
#include <string.h>
#define AG_BUTTON_LONG_PRESS_MS 3000

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
      state->long_sent = false;
      state->pressed_since_ms = now_ms;
    }
  if (pressed && !state->long_sent && now_ms >= state->pressed_since_ms &&
      now_ms - state->pressed_since_ms >= AG_BUTTON_LONG_PRESS_MS)
    {
      state->long_sent = true;
      return AG_BUTTON_GESTURE_TOGGLE_DEMO;
    }
  if (!pressed && state->pressed)
    {
      bool long_sent = state->long_sent;
      bool long_elapsed = now_ms >= state->pressed_since_ms &&
        now_ms - state->pressed_since_ms >= AG_BUTTON_LONG_PRESS_MS;
      state->pressed = false;
      state->long_sent = false;
      if (long_sent) return AG_BUTTON_GESTURE_NONE;
      return long_elapsed ? AG_BUTTON_GESTURE_TOGGLE_DEMO :
        AG_BUTTON_GESTURE_ACK;
    }
  return AG_BUTTON_GESTURE_NONE;
}
