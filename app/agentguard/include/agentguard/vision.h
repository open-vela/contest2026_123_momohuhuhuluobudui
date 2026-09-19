/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_VISION_H
#define AGENTGUARD_VISION_H

#include <stddef.h>
#include <stdint.h>

#define AG_MAX_FACE_BOXES 8

struct ag_face_box
{
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
};

struct ag_vision_result
{
  uint8_t face_count;
  uint8_t face_box_count;
  uint8_t posture_score;
  struct ag_face_box primary_face;
  struct ag_face_box face_boxes[AG_MAX_FACE_BOXES];
};

struct ag_vision_context
{
  uint16_t reference_center_y;
  uint16_t reference_height;
  uint8_t stable_frames;
};

void ag_vision_init(struct ag_vision_context *context);
int ag_vision_process_rgb565(struct ag_vision_context *context,
                             const uint16_t *pixels, uint16_t width,
                             uint16_t height,
                             struct ag_vision_result *result);

#endif
