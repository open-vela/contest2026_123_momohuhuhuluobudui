/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_THREAD_PRIORITY_H
#define AGENTGUARD_THREAD_PRIORITY_H

#include <pthread.h>

int ag_thread_attr_init_priority(pthread_attr_t *attr, int priority);

#endif
