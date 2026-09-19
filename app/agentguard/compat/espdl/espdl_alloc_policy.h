#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_heap_caps.h"

static inline bool ag_espdl_caps_require_internal(uint32_t caps)
{
  const uint32_t required = MALLOC_CAP_INTERNAL |
                            MALLOC_CAP_DMA |
                            MALLOC_CAP_SIMD;
  return (caps & required) != 0;
}
