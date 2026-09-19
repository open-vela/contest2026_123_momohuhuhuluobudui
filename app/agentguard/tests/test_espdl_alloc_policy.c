#include <assert.h>

#include "esp_heap_caps.h"
#include "espdl_alloc_policy.h"

int main(void)
{
  assert(!ag_espdl_caps_require_internal(MALLOC_CAP_DEFAULT));
  assert(!ag_espdl_caps_require_internal(MALLOC_CAP_SPIRAM));
  assert(ag_espdl_caps_require_internal(MALLOC_CAP_INTERNAL));
  assert(ag_espdl_caps_require_internal(MALLOC_CAP_DMA));
  assert(ag_espdl_caps_require_internal(MALLOC_CAP_SIMD));
  assert(ag_espdl_caps_require_internal(MALLOC_CAP_DEFAULT |
                                        MALLOC_CAP_SIMD));
  return 0;
}
