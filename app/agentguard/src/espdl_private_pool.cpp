/* SPDX-License-Identifier: Apache-2.0 */

#include "espdl_private_pool.h"

#include <pthread.h>
#include <stdint.h>
#include <string.h>

#include <nuttx/mm/mm.h>

namespace
{

constexpr size_t kPrivatePoolBytes = 96u * 1024u;
alignas(16) unsigned char g_private_pool[kPrivatePoolBytes];
pthread_once_t g_private_pool_once = PTHREAD_ONCE_INIT;
mm_heap_s *g_private_heap;

void initialize_private_pool()
{
  g_private_heap = mm_initialize("espdl-private", g_private_pool,
                                 sizeof(g_private_pool));
}

bool private_pool_ready()
{
  return pthread_once(&g_private_pool_once, initialize_private_pool) == 0 &&
         g_private_heap != nullptr;
}

bool allocation_size(size_t count, size_t size, size_t *bytes)
{
  if (size != 0 && count > SIZE_MAX / size)
    {
      return false;
    }

  *bytes = count * size;
  return true;
}

} // namespace

extern "C" void *ag_espdl_private_malloc(size_t size)
{
  return private_pool_ready() ? mm_malloc(g_private_heap, size) : nullptr;
}

extern "C" void *ag_espdl_private_calloc(size_t count, size_t size)
{
  size_t bytes;

  if (!allocation_size(count, size, &bytes))
    {
      return nullptr;
    }

  void *memory = ag_espdl_private_malloc(bytes);
  if (memory != nullptr)
    {
      memset(memory, 0, bytes);
    }

  return memory;
}

extern "C" void *ag_espdl_private_aligned_alloc(size_t alignment,
                                                  size_t size)
{
  return private_pool_ready() ?
    mm_memalign(g_private_heap, alignment, size) : nullptr;
}

extern "C" void *ag_espdl_private_aligned_calloc(size_t alignment,
                                                   size_t count,
                                                   size_t size)
{
  size_t bytes;

  if (!allocation_size(count, size, &bytes))
    {
      return nullptr;
    }

  void *memory = ag_espdl_private_aligned_alloc(alignment, bytes);
  if (memory != nullptr)
    {
      memset(memory, 0, bytes);
    }

  return memory;
}

extern "C" bool ag_espdl_private_owns(const void *memory)
{
  return memory != nullptr && private_pool_ready() &&
         mm_heapmember(g_private_heap, const_cast<void *>(memory));
}

extern "C" void ag_espdl_private_free(void *memory)
{
  if (memory != nullptr && ag_espdl_private_owns(memory))
    {
      mm_free(g_private_heap, memory);
    }
}

extern "C" size_t ag_espdl_private_free_size(void)
{
  return private_pool_ready() ? mm_mallinfo(g_private_heap).fordblks : 0;
}

extern "C" size_t ag_espdl_private_largest_free_block(void)
{
  return private_pool_ready() ? mm_mallinfo(g_private_heap).mxordblk : 0;
}
