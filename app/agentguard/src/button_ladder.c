/* SPDX-License-Identifier: Apache-2.0 */
#include "agentguard/button_ladder.h"

#include <string.h>

/* ESP32-S3-EYE connects MENU, PLAY, UP+ and DN- to ADC1 channel 0 through
 * a resistor ladder.  NuttX's ESP32-S3 ADC upper half and Espressif's board
 * BSP both expose calibrated millivolts.  Keep the official board windows:
 * DN-=0.38 V, UP+=0.82 V, PLAY=1.98 V and MENU=2.41 V. */

enum ag_function_button ag_button_ladder_decode(uint16_t raw)
{
  if (raw >= 280 && raw <= 480)
    {
      return AG_FUNCTION_BUTTON_DOWN;
    }
  if (raw >= 720 && raw <= 920)
    {
      return AG_FUNCTION_BUTTON_UP;
    }
  if (raw >= 1880 && raw <= 2080)
    {
      return AG_FUNCTION_BUTTON_PLAY;
    }
  if (raw >= 2310 && raw <= 2510)
    {
      return AG_FUNCTION_BUTTON_MENU;
    }
  return AG_FUNCTION_BUTTON_NONE;
}

void ag_button_ladder_init(struct ag_button_ladder_state *state)
{
  memset(state, 0, sizeof(*state));
}

enum ag_function_button ag_button_ladder_update(
  struct ag_button_ladder_state *state, uint16_t raw)
{
  enum ag_function_button decoded = ag_button_ladder_decode(raw);
  enum ag_function_button released = AG_FUNCTION_BUTTON_NONE;

  if (decoded != state->candidate)
    {
      state->candidate = decoded;
      state->candidate_samples = 1;
      return AG_FUNCTION_BUTTON_NONE;
    }

  if (state->candidate_samples < 2)
    {
      state->candidate_samples++;
      if (state->candidate_samples < 2)
        {
          return AG_FUNCTION_BUTTON_NONE;
        }
    }

  if (decoded == state->stable)
    {
      return AG_FUNCTION_BUTTON_NONE;
    }

  if (decoded == AG_FUNCTION_BUTTON_NONE)
    {
      released = state->stable;
    }
  state->stable = decoded;
  return released;
}
