/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/display_regions.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_CALLS 4

struct submitted_area
{
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
};

struct submit_context
{
  struct submitted_area calls[MAX_CALLS];
  size_t call_count;
  size_t fail_call;
};

static int submit_area(void *argument, const uint16_t *pixels,
                       uint16_t stride, uint16_t source_height,
                       uint16_t x, uint16_t y,
                       uint16_t width, uint16_t height)
{
  struct submit_context *context = argument;
  size_t index = context->call_count++;

  assert(pixels != NULL);
  assert(stride == 6);
  assert(source_height == 6);
  assert(index < MAX_CALLS);
  context->calls[index].x = x;
  context->calls[index].y = y;
  context->calls[index].width = width;
  context->calls[index].height = height;
  return index == context->fail_call ? -7 : 0;
}

static void reset_context(struct submit_context *context)
{
  memset(context, 0, sizeof(*context));
  context->fail_call = SIZE_MAX;
}

int main(void)
{
  uint16_t pixels[36];
  uint16_t header_snapshot[6];
  uint16_t footer_snapshot[12];
  const struct ag_preview_area preview =
  {
    .x = 1,
    .y = 2,
    .width = 4,
    .height = 2
  };
  struct ag_display_regions_state state =
  {
    .header_snapshot = header_snapshot,
    .footer_snapshot = footer_snapshot
  };
  struct submit_context context;
  const struct ag_display_regions_ops ops =
  {
    .submit = submit_area,
    .context = &context
  };

  memset(pixels, 0, sizeof(pixels));
  reset_context(&context);
  assert(ag_display_regions_update(&ops, pixels, 6, 6, &preview,
                                   1, 2, &state) == 0);
  assert(context.call_count == 3);
  assert(context.calls[0].x == 1 && context.calls[0].y == 2 &&
         context.calls[0].width == 4 && context.calls[0].height == 2);
  assert(context.calls[1].x == 0 && context.calls[1].y == 0 &&
         context.calls[1].width == 6 && context.calls[1].height == 1);
  assert(context.calls[2].x == 0 && context.calls[2].y == 4 &&
         context.calls[2].width == 6 && context.calls[2].height == 2);
  assert(state.header_valid && state.footer_valid);

  reset_context(&context);
  assert(ag_display_regions_update(&ops, pixels, 6, 6, &preview,
                                   1, 2, &state) == 0);
  assert(context.call_count == 1);

  pixels[2] = 11;
  reset_context(&context);
  assert(ag_display_regions_update(&ops, pixels, 6, 6, &preview,
                                   1, 2, &state) == 0);
  assert(context.call_count == 2);
  assert(context.calls[1].x == 2 && context.calls[1].y == 0 &&
         context.calls[1].width == 1 && context.calls[1].height == 1);

  pixels[32] = 22;
  reset_context(&context);
  assert(ag_display_regions_update(&ops, pixels, 6, 6, &preview,
                                   1, 2, &state) == 0);
  assert(context.call_count == 2);
  assert(context.calls[1].x == 2 && context.calls[1].y == 5 &&
         context.calls[1].width == 1 && context.calls[1].height == 1);

  state.header_valid = false;
  reset_context(&context);
  context.fail_call = 1;
  assert(ag_display_regions_update(&ops, pixels, 6, 6, &preview,
                                   1, 2, &state) == -7);
  assert(context.call_count == 2);
  assert(!state.header_valid);

  puts("AgentGuard display region tests: PASS");
  return 0;
}
