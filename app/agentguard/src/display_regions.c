/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/display_regions.h"
#include "agentguard/rgb565_region.h"

#include <stddef.h>

void ag_display_regions_note_generation(
  struct ag_display_regions_state *state, bool valid, uint32_t generation)
{
  if (state == NULL)
    {
      return;
    }

  if (!valid)
    {
      state->diagnostic_generation_valid = false;
      return;
    }

  if (!state->diagnostic_generation_valid ||
      state->diagnostic_generation != generation)
    {
      state->header_valid = false;
      state->footer_valid = false;
      state->diagnostic_generation = generation;
      state->diagnostic_generation_valid = true;
    }
}

int ag_display_regions_update(const struct ag_display_regions_ops *ops,
                              const uint16_t *pixels,
                              uint16_t stride,
                              uint16_t source_height,
                              const struct ag_preview_area *preview,
                              uint16_t header_height,
                              uint16_t footer_height,
                              struct ag_display_regions_state *state)
{
  uint16_t footer_y;
  struct ag_rgb565_rect header_difference;
  struct ag_rgb565_rect footer_difference;
  bool header_changed;
  bool footer_changed;
  int result;

  if (ops == NULL || ops->submit == NULL || pixels == NULL ||
      preview == NULL || state == NULL || state->header_snapshot == NULL ||
      state->footer_snapshot == NULL || stride == 0 || source_height == 0 ||
      preview->width == 0 || preview->height == 0 ||
      (uint32_t)preview->x + preview->width > stride ||
      (uint32_t)preview->y + preview->height > source_height ||
      header_height == 0 || footer_height == 0 ||
      header_height != preview->y ||
      (uint32_t)preview->y + preview->height + footer_height !=
        source_height)
    {
      return -1;
    }

  footer_y = source_height - footer_height;
  if (state->header_valid)
    {
      header_changed =
        ag_rgb565_region_difference(pixels, stride, source_height,
                                    0, 0, stride, header_height,
                                    state->header_snapshot,
                                    &header_difference);
    }
  else
    {
      header_changed = true;
      header_difference.x = 0;
      header_difference.y = 0;
      header_difference.width = stride;
      header_difference.height = header_height;
    }

  if (state->footer_valid)
    {
      footer_changed =
        ag_rgb565_region_difference(pixels, stride, source_height,
                                    0, footer_y, stride, footer_height,
                                    state->footer_snapshot,
                                    &footer_difference);
    }
  else
    {
      footer_changed = true;
      footer_difference.x = 0;
      footer_difference.y = footer_y;
      footer_difference.width = stride;
      footer_difference.height = footer_height;
    }

  result = ops->submit(ops->context, pixels, stride, source_height,
                       preview->x, preview->y,
                       preview->width, preview->height);
  if (result < 0)
    {
      return result;
    }

  if (header_changed)
    {
      result = ops->submit(ops->context, pixels, stride, source_height,
                           header_difference.x, header_difference.y,
                           header_difference.width,
                           header_difference.height);
      if (result < 0)
        {
          return result;
        }

      state->header_valid =
        ag_rgb565_region_copy(state->header_snapshot, pixels,
                              stride, source_height,
                              0, 0, stride, header_height);
      if (!state->header_valid)
        {
          return -1;
        }
    }

  if (footer_changed)
    {
      result = ops->submit(ops->context, pixels, stride, source_height,
                           footer_difference.x, footer_difference.y,
                           footer_difference.width,
                           footer_difference.height);
      if (result < 0)
        {
          return result;
        }

      state->footer_valid =
        ag_rgb565_region_copy(state->footer_snapshot, pixels,
                              stride, source_height,
                              0, footer_y, stride, footer_height);
      if (!state->footer_valid)
        {
          return -1;
        }
    }

  return 0;
}
