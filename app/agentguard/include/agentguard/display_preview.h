/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_DISPLAY_PREVIEW_H
#define AGENTGUARD_DISPLAY_PREVIEW_H

#include "agentguard/vision.h"

#include <stdbool.h>
#include <stdint.h>

struct ag_preview_area
{
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
};

bool ag_preview_scale_rgb565(uint16_t *destination,
                             uint16_t destination_stride,
                             uint16_t destination_height,
                             const uint16_t *source,
                             uint16_t source_width,
                             uint16_t source_height,
                             const struct ag_preview_area *area);
bool ag_preview_map_face(const struct ag_face_box *source,
                         uint16_t source_width,
                         uint16_t source_height,
                         const struct ag_preview_area *area,
                         struct ag_face_box *destination);
size_t ag_preview_map_faces(const struct ag_face_box *source,
                            size_t source_count,
                            uint16_t source_width,
                            uint16_t source_height,
                            const struct ag_preview_area *area,
                            struct ag_face_box *destination,
                            size_t destination_capacity);

#endif
