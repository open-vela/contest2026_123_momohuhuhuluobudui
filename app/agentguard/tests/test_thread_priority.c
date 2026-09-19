/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/thread_priority.h"

#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>

int main(void)
{
  pthread_attr_t attr;
  struct sched_param param;
  int inherit;
  int policy;

  assert(ag_thread_attr_init_priority(&attr, 50) == 0);
  assert(pthread_attr_getinheritsched(&attr, &inherit) == 0);
  assert(inherit == PTHREAD_EXPLICIT_SCHED);
  assert(pthread_attr_getschedpolicy(&attr, &policy) == 0);
  assert(policy == SCHED_FIFO);
  assert(pthread_attr_getschedparam(&attr, &param) == 0);
  assert(param.sched_priority == 50);
  assert(pthread_attr_destroy(&attr) == 0);

  puts("AgentGuard thread priority tests: PASS");
  return 0;
}
