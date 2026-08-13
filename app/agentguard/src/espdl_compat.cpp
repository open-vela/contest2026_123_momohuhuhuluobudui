/* SPDX-License-Identifier: Apache-2.0 */

#include <malloc.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <arch/arch.h>

#include "esp_heap_caps.h"
#include "espdl_alloc_policy.h"

extern "C"
{

extern char _image_drom_vma[];
extern char _image_drom_size[];

static bool agentguard_ptr_in_flash_drom(const void *pointer)
{
  uintptr_t address = reinterpret_cast<uintptr_t>(pointer);
  uintptr_t start = reinterpret_cast<uintptr_t>(_image_drom_vma);
  uintptr_t size = reinterpret_cast<uintptr_t>(_image_drom_size);
  return address >= start && address - start < size;
}

void *heap_caps_malloc(size_t size, uint32_t caps)
{
  if (ag_espdl_caps_require_internal(caps))
    {
      return xtensa_imm_malloc(size);
    }

  return malloc(size);
}

void *heap_caps_calloc(size_t count, size_t size, uint32_t caps)
{
  size_t bytes;
  void *memory;

  if (size != 0 && count > SIZE_MAX / size)
    {
      return nullptr;
    }

  bytes = count * size;
  memory = heap_caps_malloc(bytes, caps);
  if (memory != nullptr)
    {
      memset(memory, 0, bytes);
    }

  return memory;
}

void *heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps)
{
  if (ag_espdl_caps_require_internal(caps))
    {
      return xtensa_imm_memalign(alignment, size);
    }

  return memalign(alignment, size);
}

void *heap_caps_aligned_calloc(size_t alignment, size_t count, size_t size,
                               uint32_t caps)
{
  size_t bytes;
  void *memory;

  if (size != 0 && count > SIZE_MAX / size)
    {
      return nullptr;
    }

  bytes = count * size;
  memory = heap_caps_aligned_alloc(alignment, bytes, caps);

  if (memory != nullptr)
    {
      memset(memory, 0, bytes);
    }

  return memory;
}

void heap_caps_free(void *memory)
{
  if (memory == nullptr)
    {
      return;
    }

  if (xtensa_imm_heapmember(memory))
    {
      xtensa_imm_free(memory);
    }
  else
    {
      free(memory);
    }
}

size_t heap_caps_get_free_size(uint32_t caps)
{
  if (ag_espdl_caps_require_internal(caps))
    {
      return xtensa_imm_mallinfo().fordblks;
    }

  struct mallinfo info = mallinfo();
  return info.fordblks;
}

size_t heap_caps_get_largest_free_block(uint32_t caps)
{
  if (ag_espdl_caps_require_internal(caps))
    {
      return xtensa_imm_mallinfo().mxordblk;
    }

  struct mallinfo info = mallinfo();
  return info.mxordblk;
}

uint32_t esp_cpu_get_cycle_count(void)
{
  uint32_t cycles;
  __asm__ volatile("rsr.ccount %0" : "=a"(cycles));
  return cycles;
}

int64_t esp_timer_get_time(void)
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return static_cast<int64_t>(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
}

bool esp_ptr_external_ram(const void *pointer)
{
  uintptr_t address = reinterpret_cast<uintptr_t>(pointer);
  return !agentguard_ptr_in_flash_drom(pointer) &&
         address >= 0x3c000000u && address < 0x3e000000u;
}

bool esp_ptr_in_drom(const void *pointer)
{
  return agentguard_ptr_in_flash_drom(pointer);
}

bool esp_ptr_internal(const void *pointer)
{
  uintptr_t address = reinterpret_cast<uintptr_t>(pointer);
  return address >= 0x3fc88000u && address < 0x403e0000u;
}

int dsps_dotprod_f32(const float *input_a, const float *input_b,
                     float *output, int length)
{
  float sum = 0.0f;
  for (int i = 0; i < length; i++)
    {
      sum += input_a[i] * input_b[i];
    }

  *output = sum;
  return 0;
}

/* ESP-DL v3.2.0 ships its FlatBuffers parser as an object built with GCC 13.
 * GCC 13 moved the rare overlapping std::string replacement path into this
 * helper.  GCC 12 has the same string representation but keeps the code
 * inline, so provide the equivalent operation without importing a second
 * C++ standard library. */
void ag_gcc13_string_replace_cold(void *string_object, char *destination,
                                  unsigned int old_length,
                                  const char *source,
                                  unsigned int new_length,
                                  unsigned int tail_length)
  asm("_ZNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEE15_M_replace_coldEPcjPKcjj");

void ag_gcc13_string_replace_cold(void *string_object, char *destination,
                                  unsigned int old_length,
                                  const char *source,
                                  unsigned int new_length,
                                  unsigned int tail_length)
{
  (void)string_object;

  if (new_length != 0 && new_length <= old_length)
    {
      memmove(destination, source, new_length);
    }

  if (tail_length != 0 && old_length != new_length)
    {
      memmove(destination + new_length, destination + old_length,
              tail_length);
    }

  if (new_length > old_length)
    {
      if (source + new_length <= destination + old_length)
        {
          memmove(destination, source, new_length);
        }
      else if (source >= destination + old_length)
        {
          unsigned int offset = (unsigned int)(source - destination) +
                                new_length - old_length;
          memcpy(destination, destination + offset, new_length);
        }
      else
        {
          unsigned int left = (unsigned int)((destination + old_length) -
                                             source);
          memmove(destination, source, left);
          memcpy(destination + left, destination + new_length,
                 new_length - left);
        }
    }
}

} // extern "C"
