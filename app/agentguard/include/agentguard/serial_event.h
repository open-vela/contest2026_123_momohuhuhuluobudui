/* SPDX-License-Identifier: Apache-2.0 */
#ifndef AGENTGUARD_SERIAL_EVENT_H
#define AGENTGUARD_SERIAL_EVENT_H
#include <stddef.h>
int ag_serial_event_format(char *buffer, size_t size, const char *json);
int ag_serial_event_try_write(const char *path, const char *json);
#endif
