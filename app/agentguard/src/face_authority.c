/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/face_authority.h"

#ifndef CONFIG_AGENTGUARD_ESP_DL
#  include "agentguard/face_fallback.h"
#endif

void ag_face_authority_apply_rgb565(
  const uint16_t *pixels, uint16_t width, uint16_t height,
  uint64_t now_ms, uint32_t hold_ms,
  struct ag_face_presence_state *presence,
  struct ag_vision_result *result)
{
#ifdef CONFIG_AGENTGUARD_ESP_DL
  (void)pixels;
  (void)width;
  (void)height;
  ag_face_presence_filter(presence, now_ms, hold_ms, result);
#else
  ag_face_fallback_apply_rgb565(pixels, width, height, result);
  ag_face_presence_filter(presence, now_ms, hold_ms, result);
#endif
}
