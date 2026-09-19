/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_ESPDL_CAMERA_ADAPTER_H
#define AGENTGUARD_ESPDL_CAMERA_ADAPTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

bool ag_espdl_camera_rgb565be_copy(uint16_t *destination,
                                   const uint16_t *source,
                                   size_t pixel_count);

#ifdef __cplusplus
}
#endif

#endif
