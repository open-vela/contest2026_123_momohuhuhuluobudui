/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/lcd_bounce.h"

#include <stddef.h>

int ag_lcd_bounce_area(const struct ag_lcd_bounce_ops *ops,
                       const uint16_t *source,
                       uint16_t source_stride,
                       uint16_t source_height,
                       uint16_t source_x,
                       uint16_t source_y,
                       uint16_t destination_x,
                       uint16_t destination_y,
                       uint16_t visible_width,
                       uint16_t visible_height,
                       uint16_t *bounce,
                       uint16_t bounce_rows,
                       struct ag_lcd_timing_state *timing)
{
  uint64_t started_ms = 0;
  uint64_t finished_ms = 0;
  uint16_t first_row;
  bool have_started;
  bool have_finished;

  ag_lcd_timing_invalidate(timing);
  if (ops == NULL || ops->read_ms == NULL || ops->submit == NULL ||
      source == NULL || bounce == NULL || timing == NULL ||
      source_stride == 0 || source_height == 0 ||
      visible_width == 0 || visible_height == 0 || bounce_rows == 0 ||
      source_x > source_stride ||
      visible_width > source_stride - source_x ||
      source_y > source_height ||
      visible_height > source_height - source_y ||
      visible_width - 1 > UINT16_MAX - destination_x ||
      visible_height - 1 > UINT16_MAX - destination_y)
    {
      return -1;
    }

  have_started = ops->read_ms(ops->context, &started_ms);
  for (first_row = 0; first_row < visible_height;)
    {
      uint16_t remaining = visible_height - first_row;
      uint16_t row_count = remaining < bounce_rows ? remaining : bounce_rows;
      uint16_t row;
      int result;

      for (row = 0; row < row_count; row++)
        {
          size_t source_offset =
            ((size_t)source_y + first_row + row) * source_stride + source_x;
          uint16_t column;

          for (column = 0; column < visible_width; column++)
            {
              uint16_t pixel = source[source_offset + column];

              bounce[(size_t)row * visible_width + column] =
                (uint16_t)((pixel << 8) | (pixel >> 8));
            }
        }

      result = ops->submit(ops->context, bounce, destination_x,
                           destination_y + first_row, row_count,
                           visible_width);
      if (result < 0)
        {
          ag_lcd_timing_invalidate(timing);
          return result;
        }

      first_row += row_count;
    }

  have_finished = ops->read_ms(ops->context, &finished_ms);
  if (!have_started || !have_finished ||
      !ag_lcd_timing_update(timing, started_ms, finished_ms))
    {
      ag_lcd_timing_invalidate(timing);
    }

  return 0;
}
