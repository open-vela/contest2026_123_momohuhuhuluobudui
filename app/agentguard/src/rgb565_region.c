/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/rgb565_region.h"

#include <stddef.h>
#include <string.h>

static bool ag_rgb565_region_valid(const uint16_t *source,
                                   uint16_t source_stride,
                                   uint16_t source_height,
                                   uint16_t source_x,
                                   uint16_t source_y,
                                   uint16_t width,
                                   uint16_t height,
                                   const uint16_t *snapshot)
{
  return source != NULL && snapshot != NULL && width != 0 && height != 0 &&
         source_x <= source_stride && width <= source_stride - source_x &&
         source_y <= source_height && height <= source_height - source_y;
}

bool ag_rgb565_region_copy(uint16_t *snapshot,
                           const uint16_t *source,
                           uint16_t source_stride,
                           uint16_t source_height,
                           uint16_t source_x,
                           uint16_t source_y,
                           uint16_t width,
                           uint16_t height)
{
  uint16_t row;

  if (!ag_rgb565_region_valid(source, source_stride, source_height,
                              source_x, source_y, width, height, snapshot))
    {
      return false;
    }

  for (row = 0; row < height; row++)
    {
      const uint16_t *source_row =
        source + ((size_t)source_y + row) * source_stride + source_x;
      uint16_t *snapshot_row = snapshot + (size_t)row * width;

      memcpy(snapshot_row, source_row,
             (size_t)width * sizeof(*source_row));
    }

  return true;
}

bool ag_rgb565_region_difference(const uint16_t *source,
                                 uint16_t source_stride,
                                 uint16_t source_height,
                                 uint16_t source_x,
                                 uint16_t source_y,
                                 uint16_t width,
                                 uint16_t height,
                                 const uint16_t *snapshot,
                                 struct ag_rgb565_rect *difference)
{
  uint16_t minimum_x;
  uint16_t minimum_y;
  uint16_t maximum_x = 0;
  uint16_t maximum_y = 0;
  uint16_t row;

  if (difference == NULL)
    {
      return false;
    }

  memset(difference, 0, sizeof(*difference));
  if (!ag_rgb565_region_valid(source, source_stride, source_height,
                              source_x, source_y, width, height, snapshot))
    {
      return false;
    }

  minimum_x = width;
  minimum_y = height;
  for (row = 0; row < height; row++)
    {
      const uint16_t *source_row =
        source + ((size_t)source_y + row) * source_stride + source_x;
      const uint16_t *snapshot_row = snapshot + (size_t)row * width;
      uint16_t column;

      for (column = 0; column < width; column++)
        {
          if (source_row[column] != snapshot_row[column])
            {
              if (column < minimum_x) minimum_x = column;
              if (column > maximum_x) maximum_x = column;
              if (row < minimum_y) minimum_y = row;
              if (row > maximum_y) maximum_y = row;
            }
        }
    }

  if (minimum_x == width)
    {
      return false;
    }

  difference->x = source_x + minimum_x;
  difference->y = source_y + minimum_y;
  difference->width = maximum_x - minimum_x + 1;
  difference->height = maximum_y - minimum_y + 1;
  return true;
}
