/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/face_presence.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
  struct ag_face_presence_state state;
  struct ag_vision_result result;

  ag_face_presence_reset(&state);
  memset(&result, 0, sizeof(result));
  result.face_count = 1;
  result.posture_score = 27;
  result.primary_face.x = 60;
  result.primary_face.y = 40;
  result.primary_face.width = 56;
  result.primary_face.height = 68;
  ag_face_presence_filter(&state, 1000, 1500, &result);
  assert(result.face_count == 1);
  assert(result.primary_face.x == 60 && result.primary_face.y == 40);

  memset(&result, 0, sizeof(result));
  ag_face_presence_filter(&state, 2499, 1500, &result);
  assert(result.face_count == 1);
  assert(result.posture_score == 0);
  assert(result.primary_face.x == 60 && result.primary_face.y == 40);
  assert(result.primary_face.width == 56 && result.primary_face.height == 68);

  memset(&result, 0, sizeof(result));
  ag_face_presence_filter(&state, 2500, 1500, &result);
  assert(result.face_count == 1);

  memset(&result, 0, sizeof(result));
  ag_face_presence_filter(&state, 2501, 1500, &result);
  assert(result.face_count == 0);
  assert(result.primary_face.width == 0 && result.primary_face.height == 0);

  ag_face_presence_reset(&state);
  memset(&result, 0, sizeof(result));
  result.face_count = 2;
  result.primary_face.x = 20;
  result.primary_face.y = 30;
  result.primary_face.width = 80;
  result.primary_face.height = 90;
  result.face_box_count = 2;
  result.face_boxes[0] = result.primary_face;
  result.face_boxes[1].x = 180;
  result.face_boxes[1].y = 45;
  result.face_boxes[1].width = 55;
  result.face_boxes[1].height = 65;
  ag_face_presence_filter(&state, 10000, 1500, &result);
  assert(result.face_count == 2);

  memset(&result, 0, sizeof(result));
  result.face_count = 1;
  result.primary_face.x = 110;
  result.primary_face.y = 70;
  result.primary_face.width = 50;
  result.primary_face.height = 60;
  result.face_box_count = 1;
  result.face_boxes[0] = result.primary_face;
  ag_face_presence_filter(&state, 11000, 1500, &result);
  assert(result.face_count == 2);
  assert(result.face_box_count == 2);
  assert(result.primary_face.x == 20 && result.primary_face.y == 30);
  assert(result.primary_face.width == 80 && result.primary_face.height == 90);
  assert(result.face_boxes[1].x == 180 && result.face_boxes[1].y == 45);

  memset(&result, 0, sizeof(result));
  result.face_count = 2;
  result.primary_face.x = 25;
  result.primary_face.y = 35;
  result.primary_face.width = 75;
  result.primary_face.height = 85;
  result.face_box_count = 2;
  result.face_boxes[0] = result.primary_face;
  result.face_boxes[1].x = 175;
  result.face_boxes[1].y = 40;
  result.face_boxes[1].width = 60;
  result.face_boxes[1].height = 70;
  ag_face_presence_filter(&state, 12000, 1500, &result);
  assert(result.face_count == 2);

  memset(&result, 0, sizeof(result));
  result.face_count = 1;
  result.primary_face.x = 120;
  result.primary_face.y = 72;
  result.primary_face.width = 64;
  result.primary_face.height = 72;
  result.face_box_count = 1;
  result.face_boxes[0] = result.primary_face;
  ag_face_presence_filter(&state, 13500, 1500, &result);
  assert(result.face_count == 2);
  assert(result.face_box_count == 2);
  assert(result.primary_face.x == 25 && result.primary_face.y == 35);
  assert(result.face_boxes[1].x == 175 && result.face_boxes[1].y == 40);

  memset(&result, 0, sizeof(result));
  result.face_count = 1;
  result.primary_face.x = 120;
  result.primary_face.y = 72;
  result.primary_face.width = 64;
  result.primary_face.height = 72;
  result.face_box_count = 1;
  result.face_boxes[0] = result.primary_face;
  ag_face_presence_filter(&state, 13501, 1500, &result);
  assert(result.face_count == 1);
  assert(result.face_box_count == 1);
  assert(result.primary_face.x == 120 && result.primary_face.y == 72);

  ag_face_presence_reset(&state);
  memset(&result, 0, sizeof(result));
  result.face_count = 1;
  result.primary_face.x = 120;
  result.primary_face.y = 72;
  result.primary_face.width = 64;
  result.primary_face.height = 72;
  ag_face_presence_filter(&state, 3000, 1500, &result);
  assert(result.face_count == 1);
  assert(result.primary_face.x == 120 && result.primary_face.y == 72);

  memset(&result, 0, sizeof(result));
  ag_face_presence_filter(&state, 2999, 1500, &result);
  assert(result.face_count == 0);

  puts("AgentGuard face presence tests: PASS");
  return 0;
}
