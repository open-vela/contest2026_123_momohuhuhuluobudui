/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/frame_timing.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
  struct ag_frame_timing_state state;

  ag_frame_timing_reset(&state);
  assert(!state.valid);

  assert(!ag_frame_timing_update(&state, 100, 130, 1, 0));
  assert(!state.valid);
  assert(ag_frame_timing_update(&state, 560, 600, 1, 500000));
  assert(state.capture_interval_ms == 500);
  assert(state.dequeue_wait_ms == 40);
  assert(state.loop_interval_ms == 470);

  assert(!ag_frame_timing_update(&state, 650, 640, 1, 600000));
  assert(!state.valid);
  assert(!ag_frame_timing_update(&state, 700, 710, 0, 0));
  assert(!state.valid);
  assert(!ag_frame_timing_update(&state, 800, 820, 1, 600000));
  assert(!state.valid);
  assert(!ag_frame_timing_update(&state, 900, 930, 1, 400000));
  assert(!state.valid);
  assert(ag_frame_timing_update(&state, 1000, 1040, 1, 900000));
  assert(state.capture_interval_ms == 500);
  assert(state.loop_interval_ms == 110);

  ag_frame_timing_reset(&state);
  assert(!state.valid);
  assert(!ag_frame_timing_update(&state, 1000, 1010, -1, 0));
  assert(!state.have_capture_timestamp);
  assert(!ag_frame_timing_update(&state, 1020, 1030, 1, -1));
  assert(!state.have_capture_timestamp);
  assert(!ag_frame_timing_update(&state, 1040, 1050, 1, 1000000));
  assert(!state.have_capture_timestamp);
  assert(!ag_frame_timing_update(&state, 1060, 1070, INT64_MAX, 999999));
  assert(!state.have_capture_timestamp);

  puts("AgentGuard frame timing tests: PASS");
  return 0;
}
