/* SPDX-License-Identifier: Apache-2.0 */
#ifndef AGENTGUARD_LED_DEVICE_H
#define AGENTGUARD_LED_DEVICE_H

/* Returns a writable LED device FD, registering the device on ENOENT. */
int ag_led_open_device(const char *path,
                       int (*register_device)(const char *path));

#endif
