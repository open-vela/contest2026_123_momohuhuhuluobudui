/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/lcd_timing.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
  struct ag_lcd_submit_timing_state submit_timing;
  struct ag_lcd_timing_state state;

  ag_lcd_timing_reset(&state);
  assert(!state.valid);

  assert(ag_lcd_timing_update(&state, 100, 131));
  assert(state.valid);
  assert(state.write_ms == 31);

  assert(!ag_lcd_timing_update(&state, 200, 199));
  assert(!state.valid);

  assert(ag_lcd_timing_update(&state, 300, 300));
  assert(state.valid);
  assert(state.write_ms == 0);

  assert(ag_lcd_timing_update(&state, 0, UINT64_MAX));
  assert(state.write_ms == UINT32_MAX);

  ag_lcd_timing_invalidate(&state);
  assert(!state.valid);
  assert(!ag_lcd_timing_update(NULL, 0, 1));

  ag_lcd_submit_timing_reset(&submit_timing);
  assert(!submit_timing.valid);
  assert(submit_timing.sample_count == 0);
  assert(submit_timing.sum_ms == 0);
  assert(submit_timing.max_ms == 0);

  assert(ag_lcd_timing_update(&state, 10, 12));
  assert(ag_lcd_submit_timing_record(&submit_timing, &state));
  assert(ag_lcd_timing_update(&state, 20, 25));
  assert(ag_lcd_submit_timing_record(&submit_timing, &state));
  assert(ag_lcd_timing_update(&state, 30, 33));
  assert(ag_lcd_submit_timing_record(&submit_timing, &state));
  assert(submit_timing.valid);
  assert(submit_timing.sample_count == 3);
  assert(submit_timing.sum_ms == 10);
  assert(submit_timing.max_ms == 5);

  ag_lcd_timing_invalidate(&state);
  assert(!ag_lcd_submit_timing_record(&submit_timing, &state));
  assert(!submit_timing.valid);
  assert(submit_timing.sample_count == 4);
  assert(submit_timing.sum_ms == 10);
  assert(submit_timing.max_ms == 5);

  assert(ag_lcd_timing_update(&state, 40, 44));
  assert(!ag_lcd_submit_timing_record(&submit_timing, &state));
  assert(!submit_timing.valid);
  assert(submit_timing.sample_count == 5);
  assert(submit_timing.sum_ms == 14);
  assert(submit_timing.max_ms == 5);

  ag_lcd_submit_timing_reset(&submit_timing);
  assert(ag_lcd_timing_update(&state, 0, UINT64_MAX));
  assert(ag_lcd_submit_timing_record(&submit_timing, &state));
  assert(ag_lcd_submit_timing_record(&submit_timing, &state));
  assert(submit_timing.sum_ms == UINT32_MAX);
  assert(submit_timing.max_ms == UINT32_MAX);
  assert(!ag_lcd_submit_timing_record(NULL, &state));
  assert(!ag_lcd_submit_timing_record(&submit_timing, NULL));

  puts("AgentGuard LCD timing tests: PASS");
  return 0;
}
