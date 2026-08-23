/* SPDX-License-Identifier: Apache-2.0 */

#include "human_face_detect_roi.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <list>

static dl::detect::result_t candidate(int left, int top, int right,
                                      int bottom)
{
  dl::detect::result_t result = {};
  result.box = {left, top, right, bottom};
  return result;
}

int main()
{
  ag_face_candidate_diagnostic captured = {};
  ag_resize_scale_diagnostic resize = {};
  ag_face_detector_trace trace = {};

  assert(human_face_detect::capture_resize_scale(0.5f, 0.25f,
                                                  2.0f, 4.0f,
                                                  &resize));
  assert(resize.x_millionths == 500000);
  assert(resize.y_millionths == 250000);
  assert(resize.inv_x_millionths == 2000000);
  assert(resize.inv_y_millionths == 4000000);
  assert(resize.valid);

  std::memset(&resize, 0x55, sizeof(resize));
  assert(human_face_detect::capture_resize_scale(0.0f, 0.0f,
                                                  0.0f, 0.0f,
                                                  &resize));
  assert(resize.x_millionths == 0);
  assert(resize.y_millionths == 0);
  assert(resize.inv_x_millionths == 0);
  assert(resize.inv_y_millionths == 0);
  assert(resize.valid);

  auto centered = candidate(20, 10, 60, 50);
  centered.score = 0.625f;
  assert(human_face_detect::capture_candidate(centered, &captured));
  assert(captured.left == 20);
  assert(captured.top == 10);
  assert(captured.right == 60);
  assert(captured.bottom == 50);
  assert(captured.score_permille == 625);
  assert(captured.valid);
  assert(human_face_detect::prepare_candidate_roi(centered, 320, 240));
  assert((centered.box == std::vector<int>{20, 10, 60, 50}));

  auto crossing_left_edge = candidate(-20, 20, 40, 60);
  std::memset(&captured, 0, sizeof(captured));
  assert(human_face_detect::capture_candidate(crossing_left_edge,
                                               &captured));
  assert(captured.left == -20);
  assert(captured.top == 20);
  assert(captured.right == 40);
  assert(captured.bottom == 60);
  assert(human_face_detect::prepare_candidate_roi(crossing_left_edge,
                                                   320, 240));
  assert((crossing_left_edge.box == std::vector<int>{0, 10, 40, 70}));

  auto outside_right_edge = candidate(330, 10, 370, 50);
  outside_right_edge.score = 1.5f;
  std::memset(&captured, 0, sizeof(captured));
  assert(human_face_detect::capture_candidate(outside_right_edge,
                                               &captured));
  assert(captured.left == 330);
  assert(captured.right == 370);
  assert(captured.score_permille == 1000);
  trace.msr_candidate_before_clip = captured;
  assert(!human_face_detect::prepare_candidate_roi(outside_right_edge,
                                                    320, 240));
  assert((outside_right_edge.box == std::vector<int>{319, 10, 319, 50}));
  std::memset(&captured, 0, sizeof(captured));
  assert(human_face_detect::capture_candidate(outside_right_edge,
                                               &captured));
  assert(captured.left == 319);
  assert(captured.top == 10);
  assert(captured.right == 319);
  assert(captured.bottom == 50);
  assert(captured.valid);
  trace.msr_candidate_after_clip = captured;
  assert(trace.msr_candidate_before_clip.left == 330);
  assert(trace.msr_candidate_before_clip.right == 370);
  assert(trace.msr_candidate_after_clip.left == 319);
  assert(trace.msr_candidate_after_clip.right == 319);

  dl::detect::result_t missing_box = {};
  std::memset(&captured, 0x55, sizeof(captured));
  assert(!human_face_detect::capture_candidate(missing_box, &captured));
  assert(!captured.valid);

  std::list<dl::detect::result_t> ranked_candidates;
  auto highest = candidate(-42, -18, -2, 22);
  highest.score = 0.875f;
  auto lower = candidate(10, 20, 30, 40);
  lower.score = 0.75f;
  ranked_candidates.push_back(highest);
  ranked_candidates.push_back(lower);
  assert(human_face_detect::capture_top_candidate(ranked_candidates,
                                                   &captured));
  assert(captured.left == -42);
  assert(captured.top == -18);
  assert(captured.right == -2);
  assert(captured.bottom == 22);
  assert(captured.score_permille == 875);

  ranked_candidates.clear();
  assert(!human_face_detect::capture_top_candidate(ranked_candidates,
                                                    &captured));
  assert(!captured.valid);

  std::puts("AgentGuard ESP-DL ROI filter tests: PASS");
  return 0;
}
