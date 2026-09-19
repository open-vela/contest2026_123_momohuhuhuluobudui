#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *ag_espdl_private_malloc(size_t size);
void *ag_espdl_private_calloc(size_t count, size_t size);
void *ag_espdl_private_aligned_alloc(size_t alignment, size_t size);
void *ag_espdl_private_aligned_calloc(size_t alignment, size_t count,
                                      size_t size);
bool ag_espdl_private_owns(const void *memory);
void ag_espdl_private_free(void *memory);
size_t ag_espdl_private_free_size(void);
size_t ag_espdl_private_largest_free_block(void);

#ifdef __cplusplus
}
#endif
