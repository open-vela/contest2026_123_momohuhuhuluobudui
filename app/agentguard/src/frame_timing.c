/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/frame_timing.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static uint32_t ag_frame_timing_clamp(uint64_t value)
{
  return value > UINT32_MAX ? UINT32_MAX : (uint32_t)value;
}

static uint64_t ag_frame_timing_timestamp_ms(int64_t seconds,
                                             int32_t microseconds)
{
  uint64_t seconds_unsigned;
  uint64_t milliseconds;

  if (seconds < 0 || microseconds < 0 || microseconds >= 1000000)
    {
      return 0;
    }

  seconds_unsigned = (uint64_t)seconds;
  if (seconds_unsigned >
      (UINT64_MAX - (uint64_t)microseconds / 1000) / 1000)
    {
      return 0;
    }

  milliseconds = seconds_unsigned * 1000 +
                 (uint64_t)microseconds / 1000;
  return milliseconds;
}

void ag_frame_timing_reset(struct ag_frame_timing_state *state)
{
  if (state != NULL)
    {
      memset(state, 0, sizeof(*state));
    }
}

bool ag_frame_timing_update(struct ag_frame_timing_state *state,
                            uint64_t dequeue_started_ms,
                            uint64_t dequeue_finished_ms,
                            int64_t capture_seconds,
                            int32_t capture_microseconds)
{
  uint64_t capture_timestamp_ms;
  bool capture_valid;
  bool loop_valid;

  if (state == NULL)
    {
      return false;
    }

  state->valid = false;
  if (dequeue_finished_ms < dequeue_started_ms)
    {
      state->have_acquisition_timestamp = false;
      return false;
    }

  state->dequeue_wait_ms = ag_frame_timing_clamp(
    dequeue_finished_ms - dequeue_started_ms);
  capture_timestamp_ms = ag_frame_timing_timestamp_ms(capture_seconds,
                                                       capture_microseconds);
  capture_valid = state->have_capture_timestamp &&
                  capture_timestamp_ms > state->previous_capture_ms;
  loop_valid = state->have_acquisition_timestamp &&
               dequeue_finished_ms >= state->previous_acquisition_ms;

  if (capture_timestamp_ms == 0)
    {
      state->have_capture_timestamp = false;
    }
  else
    {
      if (capture_valid)
        {
          state->capture_interval_ms = ag_frame_timing_clamp(
            capture_timestamp_ms - state->previous_capture_ms);
        }

      state->previous_capture_ms = capture_timestamp_ms;
      state->have_capture_timestamp = true;
    }

  if (loop_valid)
    {
      state->loop_interval_ms = ag_frame_timing_clamp(
        dequeue_finished_ms - state->previous_acquisition_ms);
    }

  state->previous_acquisition_ms = dequeue_finished_ms;
  state->have_acquisition_timestamp = true;
  state->valid = capture_valid && loop_valid;
  return state->valid;
}
