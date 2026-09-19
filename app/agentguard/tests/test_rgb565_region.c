/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/rgb565_region.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
  uint16_t source[12] =
  {
     0,  1,  2,  3,
     4,  5,  6,  7,
     8,  9, 10, 11
  };
  uint16_t snapshot[4] = {0};
  static const uint16_t expected[4] = {5, 6, 9, 10};
  struct ag_rgb565_rect difference;

  assert(ag_rgb565_region_copy(snapshot, source, 4, 3,
                               1, 1, 2, 2));
  assert(memcmp(snapshot, expected, sizeof(expected)) == 0);
  assert(!ag_rgb565_region_difference(source, 4, 3, 1, 1, 2, 2,
                                      snapshot, &difference));
  assert(difference.width == 0 && difference.height == 0);

  source[0] = 100;
  assert(!ag_rgb565_region_difference(source, 4, 3, 1, 1, 2, 2,
                                      snapshot, &difference));
  source[6] = 100;
  assert(ag_rgb565_region_difference(source, 4, 3, 1, 1, 2, 2,
                                     snapshot, &difference));
  assert(difference.x == 2 && difference.y == 1);
  assert(difference.width == 1 && difference.height == 1);

  source[9] = 101;
  assert(ag_rgb565_region_difference(source, 4, 3, 1, 1, 2, 2,
                                     snapshot, &difference));
  assert(difference.x == 1 && difference.y == 1);
  assert(difference.width == 2 && difference.height == 2);

  assert(!ag_rgb565_region_copy(snapshot, source, 4, 3,
                                3, 2, 2, 1));
  assert(!ag_rgb565_region_difference(source, 4, 3, 1, 1, 0, 2,
                                      snapshot, &difference));

  puts("AgentGuard RGB565 region tests: PASS");
  return 0;
}
