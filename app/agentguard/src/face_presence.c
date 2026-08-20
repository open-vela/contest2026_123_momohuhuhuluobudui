/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/face_presence.h"

#include <stddef.h>
#include <string.h>

void ag_face_presence_reset(struct ag_face_presence_state *state)
{
  if (state != NULL)
    {
      memset(state, 0, sizeof(*state));
    }
}

void ag_face_presence_filter(struct ag_face_presence_state *state,
                             uint64_t now_ms, uint32_t hold_ms,
                             struct ag_vision_result *result)
{
  if (state == NULL || result == NULL)
    {
      return;
    }

  if (result->face_count > 0)
    {
      state->last_seen_ms = now_ms;
      state->face_count = result->face_count;
      state->primary_face = result->primary_face;
      state->have_face = true;
      return;
    }

  if (!state->have_face)
    {
      return;
    }

  if (now_ms < state->last_seen_ms ||
      now_ms - state->last_seen_ms > hold_ms)
    {
      ag_face_presence_reset(state);
      return;
    }

  result->face_count = state->face_count;
  result->primary_face = state->primary_face;
}
