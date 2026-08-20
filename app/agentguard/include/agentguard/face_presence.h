/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_FACE_PRESENCE_H
#define AGENTGUARD_FACE_PRESENCE_H

#include "agentguard/vision.h"

#include <stdbool.h>
#include <stdint.h>

struct ag_face_presence_state
{
  uint64_t last_seen_ms;
  uint8_t face_count;
  struct ag_face_box primary_face;
  bool have_face;
};

void ag_face_presence_reset(struct ag_face_presence_state *state);
void ag_face_presence_filter(struct ag_face_presence_state *state,
                             uint64_t now_ms, uint32_t hold_ms,
                             struct ag_vision_result *result);

#endif
