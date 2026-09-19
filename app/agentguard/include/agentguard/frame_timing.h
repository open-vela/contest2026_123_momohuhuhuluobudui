/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_FRAME_TIMING_H
#define AGENTGUARD_FRAME_TIMING_H

#include <stdbool.h>
#include <stdint.h>

struct ag_frame_timing_state
{
  uint64_t previous_capture_ms;
  uint64_t previous_acquisition_ms;
  uint32_t capture_interval_ms;
  uint32_t dequeue_wait_ms;
  uint32_t loop_interval_ms;
  bool have_capture_timestamp;
  bool have_acquisition_timestamp;
  bool valid;
};

void ag_frame_timing_reset(struct ag_frame_timing_state *state);
bool ag_frame_timing_update(struct ag_frame_timing_state *state,
                            uint64_t dequeue_started_ms,
                            uint64_t dequeue_finished_ms,
                            int64_t capture_seconds,
                            int32_t capture_microseconds);

#endif
