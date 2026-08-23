/* SPDX-License-Identifier: Apache-2.0 */

#include "human_face_detect_roi.hpp"

#include <cassert>
#include <cstdio>

static dl::detect::result_t candidate(int left, int top, int right,
                                      int bottom)
{
  dl::detect::result_t result = {};
  result.box = {left, top, right, bottom};
  return result;
}

int main()
{
  auto centered = candidate(20, 10, 60, 50);
  assert(human_face_detect::prepare_candidate_roi(centered, 320, 240));
  assert((centered.box == std::vector<int>{20, 10, 60, 50}));

  auto crossing_left_edge = candidate(-20, 20, 40, 60);
  assert(human_face_detect::prepare_candidate_roi(crossing_left_edge,
                                                   320, 240));
  assert((crossing_left_edge.box == std::vector<int>{0, 10, 40, 70}));

  auto outside_right_edge = candidate(330, 10, 370, 50);
  assert(!human_face_detect::prepare_candidate_roi(outside_right_edge,
                                                    320, 240));
  assert((outside_right_edge.box == std::vector<int>{319, 10, 319, 50}));

  std::puts("AgentGuard ESP-DL ROI filter tests: PASS");
  return 0;
}
