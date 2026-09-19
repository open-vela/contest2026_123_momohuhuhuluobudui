/* SPDX-License-Identifier: Apache-2.0 */
#ifndef AGENTGUARD_BUTTON_DEVICE_H
#define AGENTGUARD_BUTTON_DEVICE_H

/* Returns an open nonblocking device FD, or -1 with errno set. */
int ag_button_open_device(const char *path,
                          int (*register_device)(const char *path));
#endif
