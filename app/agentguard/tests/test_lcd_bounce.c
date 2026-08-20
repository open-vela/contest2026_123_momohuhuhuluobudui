/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/lcd_bounce.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_MAX_SUBMITS 4
#define TEST_MAX_PIXELS  12

struct test_context
{
  uint64_t clock_values[2];
  bool clock_success[2];
  size_t clock_index;
  int submit_results[TEST_MAX_SUBMITS];
  size_t submit_count;
  uint16_t first_rows[TEST_MAX_SUBMITS];
  uint16_t first_columns[TEST_MAX_SUBMITS];
  uint16_t row_counts[TEST_MAX_SUBMITS];
  uint16_t widths[TEST_MAX_SUBMITS];
  uint16_t snapshots[TEST_MAX_SUBMITS][TEST_MAX_PIXELS];
};

static void test_context_reset(struct test_context *context,
                               uint64_t started_ms,
                               uint64_t finished_ms)
{
  memset(context, 0, sizeof(*context));
  context->clock_values[0] = started_ms;
  context->clock_values[1] = finished_ms;
  context->clock_success[0] = true;
  context->clock_success[1] = true;
}

static bool test_read_ms(void *argument, uint64_t *value)
{
  struct test_context *context = argument;
  size_t index = context->clock_index++;

  if (index >= 2 || !context->clock_success[index])
    {
      return false;
    }

  *value = context->clock_values[index];
  return true;
}

static int test_submit(void *argument, const uint16_t *pixels,
                       uint16_t first_column, uint16_t first_row,
                       uint16_t row_count,
                       uint16_t width)
{
  struct test_context *context = argument;
  size_t index = context->submit_count++;
  size_t pixel_count = (size_t)row_count * width;

  assert(index < TEST_MAX_SUBMITS);
  context->first_rows[index] = first_row;
  context->first_columns[index] = first_column;
  context->row_counts[index] = row_count;
  context->widths[index] = width;
  if (pixel_count <= TEST_MAX_PIXELS)
    {
      memcpy(context->snapshots[index], pixels,
             pixel_count * sizeof(*pixels));
    }
  return context->submit_results[index];
}

static void assert_invalid(const struct ag_lcd_bounce_ops *ops,
                           const uint16_t *source,
                           uint16_t source_stride,
                           uint16_t source_height,
                           uint16_t source_x,
                           uint16_t source_y,
                           uint16_t visible_width,
                           uint16_t visible_height,
                           uint16_t *bounce,
                           uint16_t bounce_rows,
                           struct ag_lcd_timing_state *timing,
                           struct test_context *context)
{
  assert(ag_lcd_timing_update(timing, 1, 2));
  assert(ag_lcd_bounce_area(ops, source, source_stride, source_height,
                            source_x, source_y, 0, 0,
                            visible_width, visible_height,
                            bounce, bounce_rows, timing) == -1);
  assert(context->submit_count == 0);
  assert(!timing->valid);
}

int main(void)
{
  static const uint16_t primary_source[4] =
    {0xf800, 0x07e0, 0x001f, 0xffff};
  static const uint16_t primary_expected[4] =
    {0x00f8, 0xe007, 0x1f00, 0xffff};
  static const uint16_t source[30] =
  {
     0,  1,  2,  3,  4,  5,
    10, 11, 12, 13, 14, 15,
    20, 21, 22, 23, 24, 25,
    30, 31, 32, 33, 34, 35,
    40, 41, 42, 43, 44, 45
  };
  static const uint16_t first_expected[6] =
    {0x0b00, 0x0c00, 0x0d00, 0x1500, 0x1600, 0x1700};
  static const uint16_t second_expected[6] =
    {0x1f00, 0x2000, 0x2100, 0x2900, 0x2a00, 0x2b00};
  static const uint16_t final_expected[3] =
    {0x1f00, 0x2000, 0x2100};
  static uint16_t lcd_source[240 * 240];
  static uint16_t preview_bounce[200 * 150];
  struct ag_lcd_timing_state timing;
  struct test_context context;
  struct ag_lcd_bounce_ops ops =
  {
    .read_ms = test_read_ms,
    .submit = test_submit,
    .context = &context
  };
  uint16_t bounce[6];

  test_context_reset(&context, 50, 51);
  ag_lcd_timing_reset(&timing);
  assert(ag_lcd_bounce_area(&ops, primary_source, 4, 1, 0, 0, 7, 9,
                            4, 1, bounce, 4, &timing) == 0);
  assert(context.submit_count == 1);
  assert(context.first_columns[0] == 7 && context.first_rows[0] == 9);
  assert(memcmp(context.snapshots[0], primary_expected,
                sizeof(primary_expected)) == 0);

  test_context_reset(&context, 100, 135);
  ag_lcd_timing_reset(&timing);
  assert(ag_lcd_bounce_area(&ops, source, 6, 5, 1, 1, 7, 9,
                            3, 4, bounce, 6, &timing) == 0);
  assert(context.submit_count == 2);
  assert(context.first_columns[0] == 7 && context.first_rows[0] == 9 &&
         context.row_counts[0] == 2);
  assert(context.first_columns[1] == 7 && context.first_rows[1] == 11 &&
         context.row_counts[1] == 2);
  assert(context.widths[0] == 3 && context.widths[1] == 3);
  assert(memcmp(context.snapshots[0], first_expected,
                sizeof(first_expected)) == 0);
  assert(memcmp(context.snapshots[1], second_expected,
                sizeof(second_expected)) == 0);
  assert(timing.valid && timing.write_ms == 35);

  test_context_reset(&context, 200, 220);
  assert(ag_lcd_bounce_area(&ops, source, 6, 5, 1, 1, 7, 9,
                            3, 3, bounce, 6, &timing) == 0);
  assert(context.submit_count == 2);
  assert(context.row_counts[0] == 2);
  assert(context.first_rows[1] == 11 && context.row_counts[1] == 1);
  assert(memcmp(context.snapshots[1], final_expected,
                sizeof(final_expected)) == 0);
  assert(timing.valid && timing.write_ms == 20);

  test_context_reset(&context, 300, 340);
  context.submit_results[1] = -7;
  assert(ag_lcd_bounce_area(&ops, source, 6, 5, 1, 1, 7, 9,
                            3, 4, bounce, 6, &timing) == -7);
  assert(context.submit_count == 2);
  assert(!timing.valid);

  test_context_reset(&context, 400, 440);
  context.clock_success[0] = false;
  assert(ag_lcd_bounce_area(&ops, source, 6, 5, 1, 1, 7, 9,
                            3, 4, bounce, 6, &timing) == 0);
  assert(context.submit_count == 2);
  assert(!timing.valid);

  test_context_reset(&context, 500, 499);
  assert(ag_lcd_bounce_area(&ops, source, 6, 5, 1, 1, 7, 9,
                            3, 4, bounce, 6, &timing) == 0);
  assert(!timing.valid);

  test_context_reset(&context, 700, 708);
  assert(ag_lcd_bounce_area(&ops, lcd_source, 240, 240,
                            20, 45, 20, 45, 200, 150,
                            preview_bounce, 200 * 150, &timing) == 0);
  assert(context.submit_count == 1);
  assert(context.first_columns[0] == 20 && context.first_rows[0] == 45);
  assert(context.widths[0] == 200 && context.row_counts[0] == 150);

  test_context_reset(&context, 800, 810);
  assert(ag_lcd_bounce_area(&ops, lcd_source, 240, 240,
                            0, 0, 0, 0, 240, 126,
                            preview_bounce, 200 * 150, &timing) == 0);
  assert(context.submit_count == 2);
  assert(context.widths[0] == 240 && context.widths[1] == 240);
  assert(context.row_counts[0] == 125 && context.row_counts[1] == 1);
  assert(context.first_rows[0] == 0 && context.first_rows[1] == 125);

  test_context_reset(&context, 600, 640);
  assert_invalid(&ops, NULL, 6, 5, 1, 1, 3, 4,
                 bounce, 2, &timing, &context);
  test_context_reset(&context, 600, 640);
  assert_invalid(&ops, source, 6, 5, 1, 1, 0, 4,
                 bounce, 2, &timing, &context);
  test_context_reset(&context, 600, 640);
  assert_invalid(&ops, source, 6, 5, 1, 1, 3, 4,
                 bounce, 2, &timing, &context);
  test_context_reset(&context, 600, 640);
  assert_invalid(&ops, source, 6, 5, 4, 1, 3, 4,
                 bounce, 2, &timing, &context);
  test_context_reset(&context, 600, 640);
  assert_invalid(&ops, source, 6, 5, 1, 3, 3, 4,
                 bounce, 2, &timing, &context);

  puts("AgentGuard LCD bounce tests: PASS");
  return 0;
}
