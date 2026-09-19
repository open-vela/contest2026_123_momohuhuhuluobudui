/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_FACE_AUTHORITY_H
#define AGENTGUARD_FACE_AUTHORITY_H

#include "agentguard/face_presence.h"
#include "agentguard/vision.h"

#include <stdint.h>

void ag_face_authority_apply_rgb565(
  const uint16_t *pixels, uint16_t width, uint16_t height,
  uint64_t now_ms, uint32_t hold_ms,
  struct ag_face_presence_state *presence,
  struct ag_vision_result *result);

#endif
