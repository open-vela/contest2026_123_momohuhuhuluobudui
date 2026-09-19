/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_LCD_TIMING_H
#define AGENTGUARD_LCD_TIMING_H

#include <stdbool.h>
#include <stdint.h>

struct ag_lcd_timing_state
{
  uint32_t write_ms;
  bool valid;
};

struct ag_lcd_submit_timing_state
{
  uint32_t sum_ms;
  uint32_t max_ms;
  uint32_t sample_count;
  bool valid;
};

void ag_lcd_timing_reset(struct ag_lcd_timing_state *state);
void ag_lcd_timing_invalidate(struct ag_lcd_timing_state *state);
bool ag_lcd_timing_update(struct ag_lcd_timing_state *state,
                          uint64_t started_ms, uint64_t finished_ms);
void ag_lcd_submit_timing_reset(struct ag_lcd_submit_timing_state *state);
bool ag_lcd_submit_timing_record(
  struct ag_lcd_submit_timing_state *state,
  const struct ag_lcd_timing_state *sample);

#endif
