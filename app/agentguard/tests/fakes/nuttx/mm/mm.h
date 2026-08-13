#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

struct mm_heap_s
{
  unsigned char *start;
  size_t size;
  size_t used;
};

struct mm_heap_config_s
{
  const char *name;
  void *start;
  size_t size;
};

struct mallinfo
{
  unsigned int arena;
  unsigned int ordblks;
  unsigned int aordblks;
  unsigned int mxordblk;
  unsigned int uordblks;
  unsigned int fordblks;
  unsigned int usmblks;
};

void mm_initialize_heap(const struct mm_heap_config_s *config,
                        struct mm_heap_s **heap);

static inline struct mm_heap_s *
mm_initialize(const char *name, void *heapstart, size_t heapsize)
{
  struct mm_heap_config_s config;
  struct mm_heap_s *heap;

  memset(&config, 0, sizeof(config));
  config.name = name;
  config.start = heapstart;
  config.size = heapsize;
  mm_initialize_heap(&config, &heap);
  return heap;
}

void *mm_malloc(struct mm_heap_s *heap, size_t size);
void *mm_memalign(struct mm_heap_s *heap, size_t alignment, size_t size);
void mm_free(struct mm_heap_s *heap, void *memory);
bool mm_heapmember(struct mm_heap_s *heap, void *memory);
struct mallinfo mm_mallinfo(struct mm_heap_s *heap);
