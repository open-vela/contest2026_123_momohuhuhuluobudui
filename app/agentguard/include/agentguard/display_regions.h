/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_DISPLAY_REGIONS_H
#define AGENTGUARD_DISPLAY_REGIONS_H

#include "agentguard/display_preview.h"

#include <stdbool.h>
#include <stdint.h>

struct ag_display_regions_ops
{
  int (*submit)(void *context, const uint16_t *pixels,
                uint16_t stride, uint16_t source_height,
                uint16_t x, uint16_t y,
                uint16_t width, uint16_t height);
  void *context;
};

struct ag_display_regions_state
{
  uint16_t *header_snapshot;
  uint16_t *footer_snapshot;
  uint32_t diagnostic_generation;
  bool header_valid;
  bool footer_valid;
  bool diagnostic_generation_valid;
};

void ag_display_regions_note_generation(
  struct ag_display_regions_state *state, bool valid, uint32_t generation);

int ag_display_regions_update(const struct ag_display_regions_ops *ops,
                              const uint16_t *pixels,
                              uint16_t stride,
                              uint16_t source_height,
                              const struct ag_preview_area *preview,
                              uint16_t header_height,
                              uint16_t footer_height,
                              struct ag_display_regions_state *state);

#endif
