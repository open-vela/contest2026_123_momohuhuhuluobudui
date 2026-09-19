/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/espdl_camera_adapter.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
  const uint16_t camera_le[] = {0xf800, 0x07e0, 0x001f, 0x1234};
  uint16_t model_be[4] = {0};

  assert(!ag_espdl_camera_rgb565be_copy(NULL, camera_le, 4));
  assert(!ag_espdl_camera_rgb565be_copy(model_be, NULL, 4));
  assert(ag_espdl_camera_rgb565be_copy(model_be, camera_le, 4));
  assert(model_be[0] == 0x00f8);
  assert(model_be[1] == 0xe007);
  assert(model_be[2] == 0x1f00);
  assert(model_be[3] == 0x3412);

  puts("AgentGuard ESP-DL camera adapter tests: PASS");
  return 0;
}
