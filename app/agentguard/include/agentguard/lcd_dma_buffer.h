/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_LCD_DMA_BUFFER_H
#define AGENTGUARD_LCD_DMA_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct ag_lcd_dma_buffer
{
  uint16_t *pixels;
  size_t pixel_capacity;
};

struct ag_lcd_dma_buffer_ops
{
  void *(*allocate)(void *context, size_t alignment, size_t bytes);
  void (*release)(void *context, void *memory);
  bool (*is_external)(void *context, const void *memory);
  void *context;
};

bool ag_lcd_dma_buffer_init(struct ag_lcd_dma_buffer *buffer,
                            const struct ag_lcd_dma_buffer_ops *ops,
                            size_t alignment, size_t allocation_bytes,
                            size_t usable_bytes);
void ag_lcd_dma_buffer_release(struct ag_lcd_dma_buffer *buffer,
                               const struct ag_lcd_dma_buffer_ops *ops);

#endif
