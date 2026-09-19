/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_FACE_FALLBACK_H
#define AGENTGUARD_FACE_FALLBACK_H

#include "agentguard/vision.h"

#include <stdbool.h>
#include <stdint.h>

bool ag_face_fallback_apply_rgb565(const uint16_t *pixels,
                                   uint16_t width, uint16_t height,
                                   struct ag_vision_result *result);

#endif
