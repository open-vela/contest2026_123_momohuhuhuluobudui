/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/face_fallback.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define WIDTH 320
#define HEIGHT 240

static uint16_t frame[WIDTH * HEIGHT];

static uint16_t rgb565(unsigned int r, unsigned int g, unsigned int b)
{
  return (uint16_t)(((r * 31 / 255) << 11) |
                    ((g * 63 / 255) << 5) | (b * 31 / 255));
}

static void rectangle(int x0, int y0, int width, int height,
                      uint16_t color)
{
  int x;
  int y;

  for (y = y0; y < y0 + height; y++)
    {
      for (x = x0; x < x0 + width; x++)
        {
          frame[y * WIDTH + x] = color;
        }
    }
}

int main(void)
{
  struct ag_vision_result result;
  uint16_t skin = rgb565(210, 145, 105);

  memset(frame, 0, sizeof(frame));
  memset(&result, 0, sizeof(result));
  rectangle(60, 40, 56, 68, skin);
  assert(ag_face_fallback_apply_rgb565(frame, WIDTH, HEIGHT, &result));
  assert(result.face_count == 1);
  assert(result.primary_face.x == 60 && result.primary_face.y == 40);
  assert(result.primary_face.width == 56 && result.primary_face.height == 68);

  result.face_count = 1;
  result.primary_face.x = 7;
  result.primary_face.y = 8;
  result.primary_face.width = 9;
  result.primary_face.height = 10;
  assert(!ag_face_fallback_apply_rgb565(frame, WIDTH, HEIGHT, &result));
  assert(result.face_count == 1 && result.primary_face.x == 7 &&
         result.primary_face.y == 8 && result.primary_face.width == 9 &&
         result.primary_face.height == 10);

  memset(frame, 0, sizeof(frame));
  memset(&result, 0, sizeof(result));
  rectangle(40, 40, 80, 20, skin);
  assert(!ag_face_fallback_apply_rgb565(frame, WIDTH, HEIGHT, &result));
  assert(result.face_count == 0);

  memset(frame, 0, sizeof(frame));
  rectangle(100, 80, 16, 16, skin);
  assert(!ag_face_fallback_apply_rgb565(frame, WIDTH, HEIGHT, &result));
  assert(result.face_count == 0);

  puts("AgentGuard face fallback tests: PASS");
  return 0;
}
