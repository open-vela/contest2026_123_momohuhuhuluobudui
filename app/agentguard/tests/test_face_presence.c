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
