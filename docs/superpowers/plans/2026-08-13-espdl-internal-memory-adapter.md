# ESP-DL Internal Memory Adapter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make ESP-DL honor internal/SIMD/DMA memory requests under NuttX so ESP32-S3 TIE728 convolution receives compatible buffers.

**Architecture:** A small pure-C policy header classifies ESP-DL capability flags. The NuttX compatibility layer routes required allocations to the Xtensa internal heap, routes default allocations to the common heap, and frees through the owning heap. The project configuration enables the existing 0x18000-byte internal region and restores both convolution families to TIE728 for validation.

**Tech Stack:** NuttX, ESP32-S3 Xtensa internal heap API, ESP-DL 3.2.0, C11/C++20 host tests, GNU Make.

## Global Constraints

- Do not change the NuttX global/common heap behavior.
- Internal, SIMD, and DMA requests must never silently fall back to PSRAM.
- Preserve the existing `CONFIG_XTENSA_IMEM_REGION_SIZE=0x18000` region size.
- Free every allocation through the heap that owns its pointer.
- Correctness on the embedded reference and live camera takes precedence over inference latency.
- LCD refresh work remains outside this implementation until inference correctness is established.

---

### Task 1: Capability Allocation Policy

**Files:**
- Create: `app/agentguard/compat/espdl/espdl_alloc_policy.h`
- Create: `app/agentguard/tests/test_espdl_alloc_policy.c`
- Modify: `app/agentguard/tests/Makefile`

**Interfaces:**
- Consumes: `MALLOC_CAP_INTERNAL`, `MALLOC_CAP_DMA`, and `MALLOC_CAP_SIMD` from `esp_heap_caps.h`.
- Produces: `bool ag_espdl_caps_require_internal(uint32_t caps)`.

- [ ] **Step 1: Add the failing policy test and Makefile target**

```c
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
```

Add `test_espdl_alloc_policy` to `TARGETS`, build it with the compatibility
include directory, and execute it from the `test` target.

- [ ] **Step 2: Run the policy target and verify RED**

Run: `make -C app/agentguard/tests test_espdl_alloc_policy`

Expected: compilation fails because `espdl_alloc_policy.h` does not exist.

- [ ] **Step 3: Add the minimal policy header**

```c
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
```

- [ ] **Step 4: Run all host tests and verify GREEN**

Run: `make -C app/agentguard/tests test`

Expected: all six test programs pass.

- [ ] **Step 5: Commit the policy and test**

```bash
git add app/agentguard/compat/espdl/espdl_alloc_policy.h \
  app/agentguard/tests/test_espdl_alloc_policy.c \
  app/agentguard/tests/Makefile
git commit -m "test: define ESP-DL internal allocation policy"
```

### Task 2: Capability-Aware NuttX Allocator

**Files:**
- Modify: `app/agentguard/src/espdl_compat.cpp`
- Modify: `tools/apply_agentguard_config.sh`
- Modify: `app/agentguard/compat/espdl/sdkconfig.h`
- Modify: `app/agentguard/tests/test_espdl_backend_config.cpp`

**Interfaces:**
- Consumes: `ag_espdl_caps_require_internal`, `xtensa_imm_malloc`, `xtensa_imm_memalign`, `xtensa_imm_free`, `xtensa_imm_heapmember`, and `xtensa_imm_mallinfo`.
- Produces: capability-correct implementations of the existing `heap_caps_*` compatibility API and a fully accelerated diagnostic backend.

- [ ] **Step 1: Change backend assertions to require both accelerated convolution families**

```cpp
static_assert(CONFIG_AGENTGUARD_ESP_DL_FORCE_C_CONV == 0,
              "ordinary convolution must retain TIE728 acceleration");
static_assert(CONFIG_AGENTGUARD_ESP_DL_FORCE_C_DEPTHWISE == 0,
              "depthwise convolution must retain TIE728 acceleration");
```

- [ ] **Step 2: Run the backend target and verify RED**

Run: `make -C app/agentguard/tests test_espdl_backend_config`

Expected: the ordinary-convolution assertion fails while
`CONFIG_AGENTGUARD_ESP_DL_FORCE_C_CONV` remains 1.

- [ ] **Step 3: Restore the accelerated backend and enable the internal heap in the config script**

Set both force-C flags to zero in `sdkconfig.h`:

```c
#define CONFIG_AGENTGUARD_ESP_DL_FORCE_C_CONV 0
#define CONFIG_AGENTGUARD_ESP_DL_FORCE_C_DEPTHWISE 0
```

Replace the internal-heap disable operation in `apply_agentguard_config.sh`
with:

```bash
"$tweak" --file "$config_file" --enable XTENSA_IMEM_USE_SEPARATE_HEAP
"$tweak" --file "$config_file" --set-val XTENSA_IMEM_REGION_SIZE 0x18000
```

Update the adjacent comment to explain that ESP-DL capability allocations and
Xtensa stacks share this dedicated internal region.

- [ ] **Step 4: Implement capability-aware allocation and ownership-aware free**

Include `<arch/arch.h>`, `<limits.h>`, and `espdl_alloc_policy.h`. Route malloc
and aligned allocation as follows:

```cpp
if (ag_espdl_caps_require_internal(caps))
  {
    return xtensa_imm_malloc(size);
  }

return malloc(size);
```

Use `xtensa_imm_memalign` for aligned internal requests. Reject calloc when
`size != 0 && count > SIZE_MAX / size`; otherwise allocate through the same
capability path and zero the returned bytes. Free through the owner:

```cpp
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
```

For internal free-size queries return `xtensa_imm_mallinfo().fordblks`; for
internal largest-block queries return `.mxordblk`. Preserve `mallinfo()` for
other capability queries.

- [ ] **Step 5: Run all host tests and static checks**

Run: `make -C app/agentguard/tests clean`

Run: `make -C app/agentguard/tests test`

Run: `git diff --check`

Expected: all six tests pass and the diff check emits no errors.

- [ ] **Step 6: Commit allocator and configuration changes**

```bash
git add app/agentguard/src/espdl_compat.cpp \
  app/agentguard/compat/espdl/sdkconfig.h \
  app/agentguard/tests/test_espdl_backend_config.cpp \
  tools/apply_agentguard_config.sh
git commit -m "fix: honor ESP-DL internal memory capabilities"
```

### Task 3: Firmware Build and Device Validation

**Files:**
- Modify at build time: `/home/yhx/Desktop/openvela/nuttx/.config`
- Verify: `/home/yhx/Desktop/openvela/nuttx/nuttx`
- Verify: `/home/yhx/Desktop/openvela/nuttx/nuttx.bin`

**Interfaces:**
- Consumes: the capability-aware allocator and project configuration script.
- Produces: a flashed ESP32-S3 image with both convolution families accelerated and internal memory enabled.

- [ ] **Step 1: Apply the product configuration**

Run: `tools/apply_agentguard_config.sh /home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui /home/yhx/Desktop/openvela`

Expected: `.config` contains `CONFIG_XTENSA_IMEM_USE_SEPARATE_HEAP=y` and
`CONFIG_XTENSA_IMEM_REGION_SIZE=0x18000`.

- [ ] **Step 2: Perform a clean firmware build**

Run: `make -C /home/yhx/Desktop/openvela/nuttx clean`

Run after activating `/home/yhx/Desktop/openvela/myenv/bin/activate`:
`make -C /home/yhx/Desktop/openvela/nuttx -j8`

Expected: `nuttx.bin` is generated successfully.

- [ ] **Step 3: Verify configuration, symbols, image, and host tests**

Run the host test suite again. Verify both convolution objects contain
unresolved `dl_tie728_*conv2d*` symbols with `xtensa-esp32s3-elf-nm -u`.
Verify the ELF contains `xtensa_imm_malloc`, `xtensa_imm_free`, and
`xtensa_imm_heapmember`. Record `sha256sum nuttx.bin` and run
`esptool.py --chip esp32s3 image_info nuttx.bin`.

Expected: tests pass, all internal-heap and accelerated symbols are linked,
and esptool accepts the image.

- [ ] **Step 4: Flash with write verification**

Run after activating the same environment:
`make -C /home/yhx/Desktop/openvela/nuttx -j8 flash ESPTOOL_PORT=/dev/ttyACM0 ESPTOOL_BINDIR=./`

Expected: esptool reports that the written-data hash is verified and resets
the board.

- [ ] **Step 5: Validate on the physical device**

After startup, record `AI`, `T`, `F`, `S`, `FACE`, and `M` first with no face
visible and then with one face centered. Also confirm that the LCD remains on.

Pass criteria: the reference image has at least one reference face; the live
face count is zero in the empty scene and at least one with a centered face;
live diagnostics change between the two scenes. A boot failure, black LCD, or
fixed live face count fails validation.
