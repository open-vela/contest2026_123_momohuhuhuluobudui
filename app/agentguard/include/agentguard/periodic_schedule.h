/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_PERIODIC_SCHEDULE_H
#define AGENTGUARD_PERIODIC_SCHEDULE_H

#include <stdbool.h>
#include <stdint.h>

struct ag_periodic_schedule
{
  uint64_t next_ms;
  uint64_t last_now_ms;
  bool initialized;
};

void ag_periodic_schedule_reset(struct ag_periodic_schedule *schedule,
                                uint64_t now_ms);
uint32_t ag_periodic_schedule_delay_ms(
  struct ag_periodic_schedule *schedule,
  uint64_t now_ms,
  uint32_t period_ms);

#endif
