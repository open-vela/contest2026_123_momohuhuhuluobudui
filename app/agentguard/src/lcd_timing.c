/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/lcd_timing.h"

#include <stddef.h>

void ag_lcd_timing_reset(struct ag_lcd_timing_state *state)
{
  if (state != NULL)
    {
      state->write_ms = 0;
      state->valid = false;
    }
}

void ag_lcd_timing_invalidate(struct ag_lcd_timing_state *state)
{
  if (state != NULL)
    {
      state->valid = false;
    }
}

bool ag_lcd_timing_update(struct ag_lcd_timing_state *state,
                          uint64_t started_ms, uint64_t finished_ms)
{
  uint64_t elapsed_ms;

  if (state == NULL)
    {
      return false;
    }

  state->valid = false;
  if (finished_ms < started_ms)
    {
      return false;
    }

  elapsed_ms = finished_ms - started_ms;
  state->write_ms = elapsed_ms > UINT32_MAX ? UINT32_MAX :
                    (uint32_t)elapsed_ms;
  state->valid = true;
  return true;
}
