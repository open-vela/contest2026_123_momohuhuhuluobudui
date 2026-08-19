/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_RGB565_REGION_H
#define AGENTGUARD_RGB565_REGION_H

#include <stdbool.h>
#include <stdint.h>

struct ag_rgb565_rect
{
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
};

bool ag_rgb565_region_copy(uint16_t *snapshot,
                           const uint16_t *source,
                           uint16_t source_stride,
                           uint16_t source_height,
                           uint16_t source_x,
                           uint16_t source_y,
                           uint16_t width,
                           uint16_t height);
bool ag_rgb565_region_difference(const uint16_t *source,
                                 uint16_t source_stride,
                                 uint16_t source_height,
                                 uint16_t source_x,
                                 uint16_t source_y,
                                 uint16_t width,
                                 uint16_t height,
                                 const uint16_t *snapshot,
                                 struct ag_rgb565_rect *difference);

#endif
