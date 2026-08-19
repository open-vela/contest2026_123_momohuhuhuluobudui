/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/periodic_schedule.h"

#include <limits.h>
#include <stddef.h>

void ag_periodic_schedule_reset(struct ag_periodic_schedule *schedule,
                                uint64_t now_ms)
{
  if (schedule != NULL)
    {
      schedule->next_ms = now_ms;
      schedule->last_now_ms = now_ms;
      schedule->initialized = true;
    }
}

uint32_t ag_periodic_schedule_delay_ms(
  struct ag_periodic_schedule *schedule,
  uint64_t now_ms,
  uint32_t period_ms)
{
  uint64_t steps;
  uint64_t delay;

  if (schedule == NULL || period_ms == 0)
    {
      return 0;
    }

  if (!schedule->initialized || now_ms < schedule->last_now_ms)
    {
      ag_periodic_schedule_reset(schedule, now_ms);
    }

  schedule->last_now_ms = now_ms;
  if (schedule->next_ms <= now_ms)
    {
      steps = (now_ms - schedule->next_ms) / period_ms + 1;
      if (steps > (UINT64_MAX - schedule->next_ms) / period_ms)
        {
          schedule->next_ms = UINT64_MAX;
        }
      else
        {
          schedule->next_ms += steps * period_ms;
        }
    }

  delay = schedule->next_ms - now_ms;
  return delay > UINT32_MAX ? UINT32_MAX : (uint32_t)delay;
}
