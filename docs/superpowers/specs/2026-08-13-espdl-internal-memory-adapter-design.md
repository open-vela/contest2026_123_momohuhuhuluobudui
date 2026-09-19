# ESP-DL Internal Memory Adapter Design

## Context

AgentGuard runs ESP-DL 3.2.0 on ESP32-S3 under NuttX. The current ESP-IDF
compatibility layer ignores all `MALLOC_CAP_*` flags and sends every ESP-DL
allocation to the NuttX common heap. That heap includes PSRAM. As a result,
buffers explicitly requested as internal, SIMD-capable, or DMA-capable can be
placed in PSRAM and then consumed by TIE728 vector assembly.

Backend isolation on the device produced deterministic but incorrect output:

- all-C convolution: about 600 ms, reference score 57 and no reference face;
- TIE728 ordinary convolution: about 200 ms, score fixed near 2;
- TIE728 depthwise convolution: about 630 ms, score fixed near 95 and a false
  live face even without a person.

This establishes that the accelerated code executes, but its operands or
results are not reliable with the current memory adapter.

## Selected Approach

Enable NuttX's dedicated Xtensa internal-memory heap and map ESP-DL capability
requests onto it. Keep the existing common heap for ordinary/default
allocations. This changes only the ESP-DL compatibility boundary and does not
alter camera, LCD, networking, or general application allocations.

An allocation requires the internal heap when its flags contain
`MALLOC_CAP_INTERNAL`, `MALLOC_CAP_SIMD`, or `MALLOC_CAP_DMA`. Internal requests
must return `NULL` if the dedicated heap cannot satisfy them; they must not
silently fall back to PSRAM. Default and explicit SPIRAM requests continue to
use the common heap.

`heap_caps_free` determines which heap owns a pointer before releasing it.
Internal-heap pointers use `xtensa_imm_free`; other pointers use `free`. Freeing
`NULL` remains harmless. Internal free-space and largest-block queries report
the dedicated heap's `mallinfo`; common/SPIRAM queries retain current behavior.

## Components and Data Flow

1. A small, host-testable allocation policy classifies capability flags.
2. `espdl_compat.cpp` uses that policy for malloc, calloc, and aligned
   allocation.
3. NuttX configuration enables `CONFIG_XTENSA_IMEM_USE_SEPARATE_HEAP` with the
   existing 0x18000-byte region.
4. ESP-DL's memory planner sees the real internal free space and assigns only
   suitable tensors and scratch buffers there.
5. TIE728 convolution reads internal-capable buffers while large/default data
   remains in PSRAM.

## Failure Handling

Allocation failure is propagated through ESP-DL's existing `NULL` checks and
diagnostics. Arithmetic overflow in calloc size calculation is rejected.
Aligned allocations preserve the requested alignment. No pointer may be freed
through a heap that does not own it.

If 96 KB is insufficient for ESP-DL's planned internal arena, the experiment
fails explicitly rather than booting with corrupted ownership. In that case,
the next design step is targeted per-operator staging, not an implicit fallback
to PSRAM.

## Verification

- Unit tests cover capability classification, including combined flags.
- Existing AgentGuard host tests continue to pass.
- A clean firmware build verifies that accelerated convolution symbols remain
  linked and the separate internal heap is enabled.
- Flashing must complete with image hash verification and no boot/LCD
  regression.
- The embedded reference image must produce a stable reference face.
- Live `S`, `M`, and `FACE` values must differ appropriately between an empty
  scene and a visible face; a fixed face in both scenes is a failure.
- Inference latency is recorded but correctness takes precedence over speed.

LCD tearing and low preview cadence are a separate issue. After inference is
correct, the display worker will be changed to submit only newly published
frames instead of repeatedly writing the same full frame every 80 ms.
