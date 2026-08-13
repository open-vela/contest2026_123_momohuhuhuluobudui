/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_VISION_MODEL_H
#define AGENTGUARD_VISION_MODEL_H

#include <stdint.h>

#include "agentguard/vision.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct ag_vision_model_diagnostics
{
  uint32_t inference_ms;
  uint8_t msr_candidates;
  int16_t input_min;
  int16_t input_max;
  uint8_t score_percent;
  uint8_t pixel_mode_scores[4];
  uint8_t reference_score_percent;
  uint8_t reference_face_count;
  uint8_t tie_selftest_valid;
  uint8_t tie_ram_pass;
  uint8_t tie_flash_pass;
  uint8_t valid;
};

int ag_vision_model_process_rgb565(const uint16_t *pixels, uint16_t width,
                                   uint16_t height,
                                   struct ag_vision_result *result);
void ag_vision_model_get_diagnostics(
  struct ag_vision_model_diagnostics *diagnostics);

#ifdef __cplusplus
}
#endif

#endif
