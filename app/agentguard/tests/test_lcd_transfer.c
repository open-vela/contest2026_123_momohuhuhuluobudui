/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/lcd_transfer.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum test_event
{
  EVENT_READ = 1,
  EVENT_CLEAN,
  EVENT_SUBMIT
};

struct test_context
{
  enum test_event events[8];
  size_t event_count;
  uint64_t clock_values[2];
  bool clock_success[2];
  size_t clock_index;
  uintptr_t clean_start;
  uintptr_t clean_end;
  int submit_result;
};

static void test_context_reset(struct test_context *context)
{
  memset(context, 0, sizeof(*context));
  context->clock_values[0] = 100;
  context->clock_values[1] = 131;
  context->clock_success[0] = true;
  context->clock_success[1] = true;
}

static bool test_read_ms(void *argument, uint64_t *value)
{
  struct test_context *context = argument;
  size_t index = context->clock_index++;

  context->events[context->event_count++] = EVENT_READ;
  if (index >= 2 || !context->clock_success[index])
    {
      return false;
    }

  *value = context->clock_values[index];
  return true;
}

static void test_clean(void *argument, uintptr_t start, uintptr_t end)
{
  struct test_context *context = argument;

  context->events[context->event_count++] = EVENT_CLEAN;
  context->clean_start = start;
  context->clean_end = end;
}

static int test_submit(void *argument)
{
  struct test_context *context = argument;

  context->events[context->event_count++] = EVENT_SUBMIT;
  return context->submit_result;
}

int main(void)
{
  struct ag_lcd_timing_state timing;
  struct test_context context;
  struct ag_lcd_transfer_ops ops =
  {
    .read_ms = test_read_ms,
    .clean = test_clean,
    .submit = test_submit,
    .context = &context
  };

  test_context_reset(&context);
  ag_lcd_timing_reset(&timing);
  assert(ag_lcd_transfer_run(&ops, 0x1000, 153600, &timing) == 0);
  assert(context.event_count == 4);
  assert(context.events[0] == EVENT_CLEAN);
  assert(context.events[1] == EVENT_READ);
  assert(context.events[2] == EVENT_SUBMIT);
  assert(context.events[3] == EVENT_READ);
  assert(context.clean_start == 0x1000);
  assert(context.clean_end == 0x26800);
  assert(timing.valid);
  assert(timing.write_ms == 31);

  test_context_reset(&context);
  context.submit_result = -1;
  assert(ag_lcd_transfer_run(&ops, 0x1000, 153600, &timing) == -1);
  assert(context.event_count == 3);
  assert(context.events[0] == EVENT_CLEAN);
  assert(context.events[1] == EVENT_READ);
  assert(context.events[2] == EVENT_SUBMIT);
  assert(!timing.valid);

  test_context_reset(&context);
  context.clock_success[0] = false;
  assert(ag_lcd_transfer_run(&ops, 0x1000, 153600, &timing) == 0);
  assert(context.event_count == 4);
  assert(!timing.valid);

  test_context_reset(&context);
  context.clock_values[0] = 200;
  context.clock_values[1] = 199;
  assert(ag_lcd_transfer_run(&ops, 0x1000, 153600, &timing) == 0);
  assert(!timing.valid);

  test_context_reset(&context);
  ops.clean = NULL;
  assert(ag_lcd_timing_update(&timing, 1, 2));
  assert(ag_lcd_transfer_run(&ops, 0x1000, 153600, &timing) == -1);
  assert(context.event_count == 0);
  assert(!timing.valid);

  puts("AgentGuard LCD transfer tests: PASS");
  return 0;
}
