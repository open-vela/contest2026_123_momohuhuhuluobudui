/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/thread_priority.h"

#include <errno.h>
#include <sched.h>
#include <stddef.h>

int ag_thread_attr_init_priority(pthread_attr_t *attr, int priority)
{
  struct sched_param param;
  int result;

  if (attr == NULL)
    {
      return EINVAL;
    }

  result = pthread_attr_init(attr);
  if (result != 0)
    {
      return result;
    }

  result = pthread_attr_setschedpolicy(attr, SCHED_FIFO);
  if (result == 0)
    {
      param.sched_priority = priority;
      result = pthread_attr_setschedparam(attr, &param);
    }

  if (result == 0)
    {
      result = pthread_attr_setinheritsched(attr, PTHREAD_EXPLICIT_SCHED);
    }

  if (result != 0)
    {
      pthread_attr_destroy(attr);
    }

  return result;
}
