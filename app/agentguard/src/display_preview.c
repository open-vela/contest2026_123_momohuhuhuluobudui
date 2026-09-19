/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/display_preview.h"

#include <stddef.h>
#include <string.h>

static bool ag_preview_area_valid(const struct ag_preview_area *area)
{
  return area != NULL && area->width != 0 && area->height != 0 &&
         (uint32_t)area->x + area->width <= (uint32_t)UINT16_MAX + 1u &&
         (uint32_t)area->y + area->height <= (uint32_t)UINT16_MAX + 1u;
}

bool ag_preview_scale_rgb565(uint16_t *destination,
                             uint16_t destination_stride,
                             uint16_t destination_height,
                             const uint16_t *source,
                             uint16_t source_width,
                             uint16_t source_height,
                             const struct ag_preview_area *area)
{
  uint16_t row;

  if (destination == NULL || source == NULL || source_width == 0 ||
      source_height == 0 || !ag_preview_area_valid(area) ||
      (uint32_t)area->x + area->width > destination_stride ||
      (uint32_t)area->y + area->height > destination_height)
    {
      return false;
    }

  for (row = 0; row < area->height; row++)
    {
      uint16_t source_row =
        (uint16_t)(((uint32_t)row * source_height) / area->height);
      uint16_t column;

      for (column = 0; column < area->width; column++)
        {
          uint16_t source_column =
            (uint16_t)(((uint32_t)column * source_width) / area->width);

          destination[((size_t)area->y + row) * destination_stride +
                      area->x + column] =
            source[(size_t)source_row * source_width + source_column];
        }
    }

  return true;
}

bool ag_preview_map_face(const struct ag_face_box *source,
                         uint16_t source_width,
                         uint16_t source_height,
                         const struct ag_preview_area *area,
                         struct ag_face_box *destination)
{
  uint32_t source_right;
  uint32_t source_bottom;
  uint32_t left;
  uint32_t top;
  uint32_t right;
  uint32_t bottom;

  if (destination == NULL)
    {
      return false;
    }

  memset(destination, 0, sizeof(*destination));
  if (source == NULL || source_width == 0 || source_height == 0 ||
      !ag_preview_area_valid(area) || source->width == 0 ||
      source->height == 0 || source->x >= source_width ||
      source->y >= source_height)
    {
      return false;
    }

  source_right = (uint32_t)source->x + source->width;
  source_bottom = (uint32_t)source->y + source->height;
  if (source_right > source_width)
    {
      source_right = source_width;
    }

  if (source_bottom > source_height)
    {
      source_bottom = source_height;
    }

  left = (uint32_t)source->x * area->width / source_width;
  top = (uint32_t)source->y * area->height / source_height;
  right = (source_right * area->width + source_width - 1) / source_width;
  bottom =
    (source_bottom * area->height + source_height - 1) / source_height;

  destination->x = (uint16_t)(area->x + left);
  destination->y = (uint16_t)(area->y + top);
  destination->width = (uint16_t)(right - left);
  destination->height = (uint16_t)(bottom - top);
  return destination->width != 0 && destination->height != 0;
}

size_t ag_preview_map_faces(const struct ag_face_box *source,
                            size_t source_count,
                            uint16_t source_width,
                            uint16_t source_height,
                            const struct ag_preview_area *area,
                            struct ag_face_box *destination,
                            size_t destination_capacity)
{
  size_t source_index;
  size_t destination_count = 0;

  if (source == NULL || destination == NULL)
    {
      return 0;
    }

  for (source_index = 0;
       source_index < source_count &&
       destination_count < destination_capacity;
       source_index++)
    {
      if (ag_preview_map_face(&source[source_index],
                              source_width, source_height, area,
                              &destination[destination_count]))
        {
          destination_count++;
        }
    }

  return destination_count;
}
