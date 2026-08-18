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

void ag_lcd_submit_timing_reset(struct ag_lcd_submit_timing_state *state)
{
  if (state != NULL)
    {
      state->sum_ms = 0;
      state->max_ms = 0;
      state->sample_count = 0;
      state->valid = false;
    }
}

bool ag_lcd_submit_timing_record(
  struct ag_lcd_submit_timing_state *state,
  const struct ag_lcd_timing_state *sample)
{
  if (state == NULL || sample == NULL)
    {
      return false;
    }

  if (state->sample_count == 0)
    {
      state->valid = sample->valid;
    }
  else if (!sample->valid)
    {
      state->valid = false;
    }

  state->sample_count++;
  if (!sample->valid)
    {
      return false;
    }

  if (UINT32_MAX - state->sum_ms < sample->write_ms)
    {
      state->sum_ms = UINT32_MAX;
    }
  else
    {
      state->sum_ms += sample->write_ms;
    }

  if (sample->write_ms > state->max_ms)
    {
      state->max_ms = sample->write_ms;
    }

  return state->valid;
}
