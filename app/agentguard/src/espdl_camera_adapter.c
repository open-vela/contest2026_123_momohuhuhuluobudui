/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/espdl_camera_adapter.h"

bool ag_espdl_camera_rgb565be_copy(uint16_t *destination,
                                   const uint16_t *source,
                                   size_t pixel_count)
{
  size_t index;

  if (destination == NULL || source == NULL)
    {
      return false;
    }

  for (index = 0; index < pixel_count; index++)
    {
      uint16_t pixel = source[index];
      destination[index] = (uint16_t)((pixel << 8) | (pixel >> 8));
    }

  return true;
}
