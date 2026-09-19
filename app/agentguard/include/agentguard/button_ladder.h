/* SPDX-License-Identifier: Apache-2.0 */
#ifndef AGENTGUARD_BUTTON_LADDER_H
#define AGENTGUARD_BUTTON_LADDER_H

#include <stdint.h>

enum ag_function_button
{
  AG_FUNCTION_BUTTON_NONE = 0,
  AG_FUNCTION_BUTTON_DOWN,
  AG_FUNCTION_BUTTON_UP,
  AG_FUNCTION_BUTTON_PLAY,
  AG_FUNCTION_BUTTON_MENU,
};

struct ag_button_ladder_state
{
  enum ag_function_button candidate;
  enum ag_function_button stable;
  uint8_t candidate_samples;
};

void ag_button_ladder_init(struct ag_button_ladder_state *state);
enum ag_function_button ag_button_ladder_decode(uint16_t raw);
enum ag_function_button ag_button_ladder_update(
  struct ag_button_ladder_state *state, uint16_t raw);

#endif
