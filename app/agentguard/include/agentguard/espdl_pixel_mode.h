/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_ESPDL_PIXEL_MODE_H
#define AGENTGUARD_ESPDL_PIXEL_MODE_H

#define AG_ESPDL_PIXEL_MODE_COUNT 4u
#define AG_ESPDL_PIXEL_MODE_HOLD_INFERENCES 4u

static inline unsigned int
ag_espdl_pixel_mode(unsigned int inference_index)
{
  return (inference_index / AG_ESPDL_PIXEL_MODE_HOLD_INFERENCES) %
         AG_ESPDL_PIXEL_MODE_COUNT;
}

#endif
