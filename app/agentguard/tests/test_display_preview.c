/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/display_preview.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_scale_uses_nearest_source_pixels(void)
{
  static const uint16_t source[16] =
  {
     0,  1,  2,  3,
     4,  5,  6,  7,
     8,  9, 10, 11,
    12, 13, 14, 15
  };
  static const uint16_t expected[16] =
  {
    99, 99, 99, 99,
    99,  0,  2, 99,
    99,  8, 10, 99,
    99, 99, 99, 99
  };
  const struct ag_preview_area area =
  {
    .x = 1,
    .y = 1,
    .width = 2,
    .height = 2
  };
  uint16_t destination[16];
  size_t index;

  for (index = 0; index < 16; index++)
    {
      destination[index] = 99;
    }

  assert(ag_preview_scale_rgb565(destination, 4, 4,
                                 source, 4, 4, &area));
  assert(memcmp(destination, expected, sizeof(expected)) == 0);
}

static void test_face_box_maps_and_rounds_outward(void)
{
  const struct ag_preview_area area =
  {
    .x = 20,
    .y = 37,
    .width = 200,
    .height = 150
  };
  const struct ag_face_box source =
  {
    .x = 80,
    .y = 60,
    .width = 160,
    .height = 120
  };
  struct ag_face_box destination;

  assert(ag_preview_map_face(&source, 320, 240, &area, &destination));
  assert(destination.x == 70);
  assert(destination.y == 74);
  assert(destination.width == 100);
  assert(destination.height == 76);
}

static void test_face_box_clips_to_source_frame(void)
{
  const struct ag_preview_area area =
  {
    .x = 10,
    .y = 20,
    .width = 50,
    .height = 40
  };
  const struct ag_face_box source =
  {
    .x = 90,
    .y = 80,
    .width = 30,
    .height = 30
  };
  struct ag_face_box destination;

  assert(ag_preview_map_face(&source, 100, 100, &area, &destination));
  assert(destination.x == 55);
  assert(destination.y == 52);
  assert(destination.width == 5);
  assert(destination.height == 8);

  destination.width = 7;
  assert(!ag_preview_map_face(&source, 0, 100, &area, &destination));
  assert(destination.width == 0);
}

static void test_all_face_boxes_are_mapped(void)
{
  const struct ag_preview_area area =
  {
    .x = 20,
    .y = 37,
    .width = 200,
    .height = 150
  };
  const struct ag_face_box source[2] =
  {
    {.x = 32, .y = 24, .width = 64, .height = 72},
    {.x = 192, .y = 48, .width = 80, .height = 96}
  };
  struct ag_face_box destination[2];

  assert(ag_preview_map_faces(source, 2, 320, 240, &area,
                              destination, 2) == 2);
  assert(destination[0].x == 40 && destination[0].y == 52);
  assert(destination[0].width == 40 && destination[0].height == 45);
  assert(destination[1].x == 140 && destination[1].y == 67);
  assert(destination[1].width == 50 && destination[1].height == 60);
}

static void test_scale_rejects_out_of_bounds_area(void)
{
  const uint16_t source[4] = {1, 2, 3, 4};
  uint16_t destination[4] = {5, 6, 7, 8};
  const struct ag_preview_area area =
  {
    .x = 1,
    .y = 1,
    .width = 2,
    .height = 2
  };

  assert(!ag_preview_scale_rgb565(destination, 2, 2,
                                  source, 2, 2, &area));
  assert(destination[0] == 5 && destination[3] == 8);
}

int main(void)
{
  test_scale_uses_nearest_source_pixels();
  test_face_box_maps_and_rounds_outward();
  test_face_box_clips_to_source_frame();
  test_all_face_boxes_are_mapped();
  test_scale_rejects_out_of_bounds_area();
  puts("AgentGuard display preview tests: PASS");
  return 0;
}
