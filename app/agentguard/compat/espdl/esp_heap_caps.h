#pragma once

#include <stddef.h>
#include <stdint.h>

#define MALLOC_CAP_DEFAULT  (1u << 0)
#define MALLOC_CAP_INTERNAL (1u << 1)
#define MALLOC_CAP_SPIRAM   (1u << 2)
#define MALLOC_CAP_8BIT     (1u << 3)
#define MALLOC_CAP_DMA      (1u << 4)
#define MALLOC_CAP_SIMD     (1u << 5)
#define MALLOC_CAP_TCM      (1u << 6)
#define HEAP_IRAM_ATTR

#ifdef __cplusplus
extern "C" {
#endif
void *heap_caps_malloc(size_t size, uint32_t caps);
void *heap_caps_calloc(size_t count, size_t size, uint32_t caps);
void *heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps);
void *heap_caps_aligned_calloc(size_t alignment, size_t count, size_t size,
                               uint32_t caps);
void heap_caps_free(void *memory);
size_t heap_caps_get_free_size(uint32_t caps);
size_t heap_caps_get_largest_free_block(uint32_t caps);
#ifdef __cplusplus
}
#endif
