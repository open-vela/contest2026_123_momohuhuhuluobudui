/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/lcd_timing.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
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

  puts("AgentGuard LCD timing tests: PASS");
  return 0;
}
