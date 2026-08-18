/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_LCD_BOUNCE_H
#define AGENTGUARD_LCD_BOUNCE_H

#include "agentguard/lcd_timing.h"

#include <stdbool.h>
#include <stdint.h>

struct ag_lcd_bounce_ops
{
  bool (*read_ms)(void *context, uint64_t *value);
  int (*submit)(void *context, const uint16_t *pixels,
                uint16_t first_row, uint16_t row_count,
                uint16_t width);
  void *context;
};

int ag_lcd_bounce_frame(const struct ag_lcd_bounce_ops *ops,
                        const uint16_t *source,
                        uint16_t source_stride,
                        uint16_t source_height,
                        uint16_t source_x,
                        uint16_t source_y,
                        uint16_t visible_width,
                        uint16_t visible_height,
                        uint16_t *bounce,
                        uint16_t bounce_rows,
                        struct ag_lcd_timing_state *timing);

#endif
