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

void ag_lcd_timing_reset(struct ag_lcd_timing_state *state);
void ag_lcd_timing_invalidate(struct ag_lcd_timing_state *state);
bool ag_lcd_timing_update(struct ag_lcd_timing_state *state,
                          uint64_t started_ms, uint64_t finished_ms);

#endif
