#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "espdl_private_pool.h"
#include <nuttx/mm/mm.h>

static struct mm_heap_s g_fake_heap;
static unsigned int g_fake_initialize_count;
static unsigned int g_fake_free_count;

void mm_initialize_heap(const struct mm_heap_config_s *config,
                        struct mm_heap_s **heap)
{
  g_fake_initialize_count++;
  g_fake_heap.start = static_cast<unsigned char *>(config->start);
  g_fake_heap.size = config->size;
  g_fake_heap.used = 0;
  *heap = &g_fake_heap;
}

static void *fake_allocate(struct mm_heap_s *heap, size_t alignment,
                           size_t size)
{
  uintptr_t address = reinterpret_cast<uintptr_t>(heap->start + heap->used);
  uintptr_t aligned = (address + alignment - 1) & ~(alignment - 1);
  size_t offset = aligned - reinterpret_cast<uintptr_t>(heap->start);

  if (offset > heap->size || size > heap->size - offset)
    {
      return nullptr;
    }

  heap->used = offset + size;
  void *memory = reinterpret_cast<void *>(aligned);
  memset(memory, 0xa5, size);
  return memory;
}

void *mm_malloc(struct mm_heap_s *heap, size_t size)
{
  return fake_allocate(heap, alignof(max_align_t), size);
}

void *mm_memalign(struct mm_heap_s *heap, size_t alignment, size_t size)
{
  return fake_allocate(heap, alignment, size);
}

void mm_free(struct mm_heap_s *heap, void *memory)
{
  (void)heap;
  (void)memory;
  g_fake_free_count++;
}

bool mm_heapmember(struct mm_heap_s *heap, void *memory)
{
  uintptr_t address = reinterpret_cast<uintptr_t>(memory);
  uintptr_t start = reinterpret_cast<uintptr_t>(heap->start);
  return address >= start && address - start < heap->size;
}

struct mallinfo mm_mallinfo(struct mm_heap_s *heap)
{
  struct mallinfo info = {};
  size_t free_bytes = heap->size - heap->used;
  info.arena = heap->size;
  info.uordblks = heap->used;
  info.fordblks = free_bytes;
  info.mxordblk = free_bytes;
  return info;
}

int main()
{
  assert(g_fake_initialize_count == 0);
  void *plain = ag_espdl_private_malloc(32);
  assert(plain != nullptr);
  assert(g_fake_initialize_count == 1);
  assert(ag_espdl_private_owns(plain));

  void *aligned = ag_espdl_private_aligned_alloc(32, 64);
  assert(aligned != nullptr);
  assert(reinterpret_cast<uintptr_t>(aligned) % 32 == 0);

  auto *zeroed = static_cast<unsigned char *>(
    ag_espdl_private_calloc(16, 4));
  for (size_t i = 0; i < 64; ++i)
    {
      assert(zeroed[i] == 0);
    }

  assert(ag_espdl_private_calloc(SIZE_MAX, 2) == nullptr);
  assert(ag_espdl_private_aligned_calloc(16, SIZE_MAX, 2) == nullptr);
  assert(!ag_espdl_private_owns(nullptr));
  int outside;
  assert(!ag_espdl_private_owns(&outside));
  assert(ag_espdl_private_malloc(96u * 1024u) == nullptr);

  ag_espdl_private_free(plain);
  assert(g_fake_free_count == 1);
  assert(ag_espdl_private_free_size() <= 96u * 1024u);
  assert(ag_espdl_private_largest_free_block() <= 96u * 1024u);
  return 0;
}
