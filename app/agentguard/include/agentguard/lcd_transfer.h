/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_LCD_TRANSFER_H
#define AGENTGUARD_LCD_TRANSFER_H

#include "agentguard/lcd_timing.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct ag_lcd_transfer_ops
{
  bool (*read_ms)(void *context, uint64_t *value);
  void (*clean)(void *context, uintptr_t start, uintptr_t end);
  int (*submit)(void *context);
  void *context;
};

int ag_lcd_transfer_run(const struct ag_lcd_transfer_ops *ops,
                        uintptr_t buffer_start, size_t buffer_size,
                        struct ag_lcd_timing_state *timing);

#endif
