/* SPDX-License-Identifier: Apache-2.0 */
#ifndef AGENTGUARD_BUTTON_GESTURE_H
#define AGENTGUARD_BUTTON_GESTURE_H
#include <stdbool.h>
#include <stdint.h>

enum ag_button_gesture_event
{
  AG_BUTTON_GESTURE_NONE = 0,
  AG_BUTTON_GESTURE_ACK,
  AG_BUTTON_GESTURE_TOGGLE_PRIVACY,
  AG_BUTTON_GESTURE_TOGGLE_DEMO,
};

struct ag_button_gesture
{
  uint64_t pressed_since_ms;
  bool pressed;
};

void ag_button_gesture_init(struct ag_button_gesture *state);
enum ag_button_gesture_event ag_button_gesture_update(
  struct ag_button_gesture *state, bool pressed, uint64_t now_ms);
#endif
