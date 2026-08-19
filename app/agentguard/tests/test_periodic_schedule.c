/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/periodic_schedule.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
  struct ag_periodic_schedule schedule;

  ag_periodic_schedule_reset(&schedule, 1000);

  /* A 20 ms render in a 100 ms period leaves 80 ms until the absolute
   * deadline.  Returning 100 here would reintroduce render-time drift. */
  assert(ag_periodic_schedule_delay_ms(&schedule, 1020, 100) == 80);
  assert(ag_periodic_schedule_delay_ms(&schedule, 1120, 100) == 80);

  /* Missing two deadlines advances to the next future point instead of
   * issuing catch-up refreshes. */
  assert(ag_periodic_schedule_delay_ms(&schedule, 1350, 100) == 50);

  /* A defensive clock rollback starts a fresh phase rather than sleeping
   * until a stale deadline. */
  assert(ag_periodic_schedule_delay_ms(&schedule, 100, 100) == 100);

  assert(ag_periodic_schedule_delay_ms(NULL, 100, 100) == 0);
  assert(ag_periodic_schedule_delay_ms(&schedule, 100, 0) == 0);

  puts("AgentGuard periodic schedule tests: PASS");
  return 0;
}
