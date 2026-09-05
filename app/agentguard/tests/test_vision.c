/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/vision.h"

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

static void rectangle(int x0, int y0, int width, int height, uint16_t color)
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
  struct ag_vision_context context;
  struct ag_vision_result result;
  uint16_t skin = rgb565(210, 145, 105);

  memset(frame, 0, sizeof(frame));
  ag_vision_init(&context);
  assert(ag_vision_process_rgb565(&context, frame, WIDTH, HEIGHT,
                                  &result) == 0);
  assert(result.face_count == 0);

  rectangle(60, 40, 56, 68, skin);
  assert(ag_vision_process_rgb565(&context, frame, WIDTH, HEIGHT,
                                  &result) == 0);
  assert(result.face_count == 1);

  /* Small isolated skin-colored objects (hands, highlights and noise) must
   * not be counted as additional people. */

  rectangle(150, 150, 20, 20, skin);
  assert(ag_vision_process_rgb565(&context, frame, WIDTH, HEIGHT,
                                  &result) == 0);
  assert(result.face_count == 1);

  rectangle(210, 48, 48, 60, skin);
  assert(ag_vision_process_rgb565(&context, frame, WIDTH, HEIGHT,
                                  &result) == 0);
  assert(result.face_count == 2);

  puts("AgentGuard vision tests: PASS");
  return 0;
}
