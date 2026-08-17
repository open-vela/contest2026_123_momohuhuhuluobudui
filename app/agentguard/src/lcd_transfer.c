/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/lcd_transfer.h"

#include <stddef.h>

int ag_lcd_transfer_run(const struct ag_lcd_transfer_ops *ops,
                        uintptr_t buffer_start, size_t buffer_size,
                        struct ag_lcd_timing_state *timing)
{
  uint64_t started_ms = 0;
  uint64_t finished_ms = 0;
  bool have_started;
  bool have_finished;
  int result;

  if (ops == NULL || ops->read_ms == NULL || ops->clean == NULL ||
      ops->submit == NULL || timing == NULL)
    {
      ag_lcd_timing_invalidate(timing);
      return -1;
    }

  have_started = ops->read_ms(ops->context, &started_ms);
  ops->clean(ops->context, buffer_start, buffer_start + buffer_size);
  result = ops->submit(ops->context);
  if (result < 0)
    {
      ag_lcd_timing_invalidate(timing);
      return result;
    }

  have_finished = ops->read_ms(ops->context, &finished_ms);
  if (!have_started || !have_finished ||
      !ag_lcd_timing_update(timing, started_ms, finished_ms))
    {
      ag_lcd_timing_invalidate(timing);
    }

  return result;
}
