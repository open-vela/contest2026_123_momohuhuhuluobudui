/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/espdl_pixel_mode.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
  static const unsigned int expected[17] =
  {
    0, 0, 0, 0,
    1, 1, 1, 1,
    2, 2, 2, 2,
    3, 3, 3, 3,
    0
  };
  unsigned int inference_index;

  for (inference_index = 0; inference_index < 17; inference_index++)
    {
      assert(ag_espdl_pixel_mode(inference_index) ==
             expected[inference_index]);
    }

  puts("AgentGuard ESP-DL pixel mode tests: PASS");
  return 0;
}
