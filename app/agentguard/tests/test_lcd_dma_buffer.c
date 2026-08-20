/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/lcd_dma_buffer.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct test_context
{
  void *allocation;
  bool external;
  unsigned int allocate_count;
  unsigned int release_count;
  size_t last_alignment;
  size_t last_bytes;
  void *released;
};

static void *test_allocate(void *argument, size_t alignment, size_t bytes)
{
  struct test_context *context = argument;

  context->allocate_count++;
  context->last_alignment = alignment;
  context->last_bytes = bytes;
  return context->allocation;
}

static void test_release(void *argument, void *memory)
{
  struct test_context *context = argument;

  context->release_count++;
  context->released = memory;
}

static bool test_is_external(void *argument, const void *memory)
{
  struct test_context *context = argument;

  return memory == context->allocation && context->external;
}

static void assert_empty(const struct ag_lcd_dma_buffer *buffer)
{
  assert(buffer->pixels == NULL);
  assert(buffer->pixel_capacity == 0);
}

int main(void)
{
  struct test_context context;
  struct ag_lcd_dma_buffer buffer;
  struct ag_lcd_dma_buffer_ops ops;

  memset(&context, 0, sizeof(context));
  ops.allocate = test_allocate;
  ops.release = test_release;
  ops.is_external = test_is_external;
  ops.context = &context;

  buffer.pixels = (uint16_t *)(uintptr_t)1;
  buffer.pixel_capacity = 1;
  assert(!ag_lcd_dma_buffer_init(&buffer, NULL, 64, 65536, 60000));
  assert_empty(&buffer);

  assert(!ag_lcd_dma_buffer_init(&buffer, &ops, 0, 65536, 60000));
  assert(!ag_lcd_dma_buffer_init(&buffer, &ops, 3, 65536, 60000));
  assert(!ag_lcd_dma_buffer_init(&buffer, &ops, 64, 65536, 60001));
  assert(!ag_lcd_dma_buffer_init(&buffer, &ops, 64, 59998, 60000));
  assert(context.allocate_count == 0);
  assert_empty(&buffer);

  context.allocation = NULL;
  assert(!ag_lcd_dma_buffer_init(&buffer, &ops, 64, 65536, 60000));
  assert(context.allocate_count == 1);
  assert(context.release_count == 0);
  assert_empty(&buffer);

  context.allocation = (void *)(uintptr_t)0x3c001001u;
  context.external = true;
  assert(!ag_lcd_dma_buffer_init(&buffer, &ops, 64, 65536, 60000));
  assert(context.allocate_count == 2);
  assert(context.release_count == 1);
  assert(context.released == context.allocation);
  assert_empty(&buffer);

  context.allocation = (void *)(uintptr_t)0x3fcca000u;
  context.external = false;
  assert(!ag_lcd_dma_buffer_init(&buffer, &ops, 64, 65536, 60000));
  assert(context.allocate_count == 3);
  assert(context.release_count == 2);
  assert(context.released == context.allocation);
  assert_empty(&buffer);

  context.allocation = (void *)(uintptr_t)0x3c001000u;
  context.external = true;
  assert(ag_lcd_dma_buffer_init(&buffer, &ops, 64, 65536, 60000));
  assert(context.allocate_count == 4);
  assert(context.last_alignment == 64);
  assert(context.last_bytes == 65536);
  assert(buffer.pixels == context.allocation);
  assert(buffer.pixel_capacity == 30000);

  ag_lcd_dma_buffer_release(&buffer, &ops);
  assert(context.release_count == 3);
  assert(context.released == context.allocation);
  assert_empty(&buffer);

  ag_lcd_dma_buffer_release(&buffer, &ops);
  assert(context.release_count == 3);

  puts("AgentGuard LCD DMA buffer tests: PASS");
  return 0;
}
