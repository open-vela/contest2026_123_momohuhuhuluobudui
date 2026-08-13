# ESP-DL Private Internal Pool Design

## Context

ESP-DL requests internal, SIMD-capable, and DMA-capable memory for accelerated
ESP32-S3 operations. AgentGuard's compatibility layer currently routes those
requests to the NuttX common heap, which also contains PSRAM. TIE728 ordinary
and depthwise convolution then produce deterministic but incorrect inference
results.

The first remediation enabled `CONFIG_XTENSA_IMEM_USE_SEPARATE_HEAP`. Device
logs proved that this NuttX switch also redirects every Xtensa task stack into
the 96 KiB heap. During `nx_bringup`, task creation produced an invalid
scheduler context and `up_saveusercontext` raised `EXCCAUSE=0x1c` before the
LCD could start. That global switch is therefore incompatible with this
board's flat startup configuration and must remain disabled.

## Selected Approach

Reserve a 96 KiB, 16-byte-aligned byte array in AgentGuard's internal DRAM
BSS. Initialize a private NuttX `mm_heap_s` over this array on first use. This
pool belongs only to the ESP-DL compatibility layer and does not affect global
malloc, task stacks, camera buffers, LCD buffers, networking, or NuttX driver
allocations.

Capability requests containing `MALLOC_CAP_INTERNAL`, `MALLOC_CAP_SIMD`, or
`MALLOC_CAP_DMA` use `mm_malloc` or `mm_memalign` on the private heap. Default
and SPIRAM requests continue to use the common heap. Internal requests return
`NULL` when the private pool is exhausted and never fall back to PSRAM.

`heap_caps_free` checks `mm_heapmember` before selecting `mm_free` or `free`.
Internal free-space and largest-block queries use `mm_mallinfo` for the private
heap. Initialization is performed through `pthread_once`, so allocator calls
from multiple tasks cannot create two heaps over the same storage.

## Components and Data Flow

1. `espdl_alloc_policy.h` continues to classify capability flags.
2. A focused private-pool module owns the aligned storage, `pthread_once`
   state, and wrappers around NuttX `mm_*` functions.
3. `espdl_compat.cpp` delegates internal allocations, ownership checks, frees,
   and statistics to that module.
4. `tools/apply_agentguard_config.sh` explicitly keeps the Xtensa global
   separate heap disabled.
5. Both ordinary and depthwise convolution are restored to TIE728 only after
   the private pool is present.

The 96 KiB storage increases internal BSS and moves `_sheap` upward by the same
amount. A clean link must succeed without overflowing the ESP32-S3 internal
DRAM segment. PSRAM remains available as the second common-heap region.

## Failure Handling

- Reject multiplication overflow before calloc allocation.
- Preserve caller-requested alignment for aligned allocations.
- Treat `free(NULL)` as a no-op.
- Never call `mm_heapmember` before successful private-heap initialization.
- If `pthread_once` initialization cannot provide a valid heap, internal
  allocation and statistics calls fail safely without using PSRAM.
- If the firmware fails to link, boot, or display the LCD, restore the prior
  mixed backend and do not enlarge the pool without a new memory-map review.

## Verification

- Host tests cover capability classification and private-pool routing through
  a fake backend, including exhaustion, alignment, ownership, calloc zeroing,
  overflow rejection, and cross-heap free selection.
- All existing AgentGuard host tests pass.
- `git diff --check` passes.
- A clean firmware build confirms the global separate heap is disabled, the
  private pool is located in internal DRAM BSS, both TIE728 convolution
  families are linked, and the image is valid.
- Flash write verification succeeds and the LCD reaches the live UI.
- The embedded reference image reports at least one face with a stable score.
- With the camera unobstructed, `S`, `M`, and `FACE` differ between an empty
  scene and a centered face. Fixed live output in both scenes is a failure.

LCD tearing remains a separate follow-up. Once inference correctness is
established, the display worker will stop repeatedly submitting unchanged
full frames.
