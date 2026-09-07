/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/face_authority.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WIDTH 320
#define HEIGHT 240

static uint16_t frame[WIDTH * HEIGHT];

static uint16_t rgb565(unsigned int r, unsigned int g, unsigned int b)
{
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

int main(void)
{
  struct ag_face_presence_state state;
  struct ag_vision_result result;
  uint16_t skin = rgb565(210, 145, 105);

  memset(frame, 0, sizeof(frame));
  for (unsigned int y = 40; y < 108; y++)
    {
      for (unsigned int x = 60; x < 116; x++)
        {
          frame[y * WIDTH + x] = skin;
        }
    }

  ag_face_presence_reset(&state);
  memset(&result, 0, sizeof(result));
  ag_face_authority_apply_rgb565(frame, WIDTH, HEIGHT, 1000, 1500,
                                 &state, &result);

#ifdef CONFIG_AGENTGUARD_ESP_DL
  assert(result.face_count == 0);
  assert(!state.have_face);
#else
  assert(result.face_count == 1);
  assert(state.have_face);
#endif

  memset(&result, 0, sizeof(result));
  result.face_count = 1;
  result.primary_face.x = 70;
  result.primary_face.y = 40;
  result.primary_face.width = 90;
  result.primary_face.height = 120;
  ag_face_authority_apply_rgb565(frame, WIDTH, HEIGHT, 2000, 1500,
                                 &state, &result);
  assert(result.face_count == 1);
  assert(state.have_face);

  memset(frame, 0, sizeof(frame));
  memset(&result, 0, sizeof(result));
  ag_face_authority_apply_rgb565(frame, WIDTH, HEIGHT, 2500, 1500,
                                 &state, &result);
  assert(result.face_count == 1);
  assert(result.primary_face.x == 70);

  memset(&result, 0, sizeof(result));
  ag_face_authority_apply_rgb565(frame, WIDTH, HEIGHT, 3501, 1500,
                                 &state, &result);
  assert(result.face_count == 0);
  assert(!state.have_face);

  puts("AgentGuard face authority tests: PASS");
  return 0;
}
