/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/lcd_dma_buffer.h"

#include <stdint.h>

bool ag_lcd_dma_buffer_init(struct ag_lcd_dma_buffer *buffer,
                            const struct ag_lcd_dma_buffer_ops *ops,
                            size_t alignment, size_t allocation_bytes,
                            size_t usable_bytes)
{
  void *memory;

  if (buffer == NULL)
    {
      return false;
    }

  buffer->pixels = NULL;
  buffer->pixel_capacity = 0;
  if (ops == NULL || ops->allocate == NULL || ops->release == NULL ||
      ops->is_external == NULL || alignment == 0 ||
      (alignment & (alignment - 1)) != 0 || allocation_bytes == 0 ||
      usable_bytes == 0 || (usable_bytes % sizeof(uint16_t)) != 0 ||
      usable_bytes > allocation_bytes)
    {
      return false;
    }

  memory = ops->allocate(ops->context, alignment, allocation_bytes);
  if (memory == NULL)
    {
      return false;
    }

  if (((uintptr_t)memory & (alignment - 1)) != 0 ||
      !ops->is_external(ops->context, memory))
    {
      ops->release(ops->context, memory);
      return false;
    }

  buffer->pixels = memory;
  buffer->pixel_capacity = usable_bytes / sizeof(uint16_t);
  return true;
}

void ag_lcd_dma_buffer_release(struct ag_lcd_dma_buffer *buffer,
                               const struct ag_lcd_dma_buffer_ops *ops)
{
  if (buffer == NULL)
    {
      return;
    }

  if (buffer->pixels != NULL && ops != NULL && ops->release != NULL)
    {
      ops->release(ops->context, buffer->pixels);
    }

  buffer->pixels = NULL;
  buffer->pixel_capacity = 0;
}
