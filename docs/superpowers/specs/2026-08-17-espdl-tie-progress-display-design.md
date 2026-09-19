# ESP-DL TIE Progress Display Design

**Date:** 2026-08-17

**Status:** Approved

## Problem

The current diagnostic firmware calls
`ag_espdl_tie_conv_selftest_run_once()` after opening the LCD but before
starting the display worker. On the physical ESP32-S3-EYE, the LCD remains
white. Because the controller has already initialized and the display worker
has not started, the strongest current explanation is that the first direct
`dl_tie728_s8_conv2d_11cn()` call does not return.

The existing final `K:xy` result cannot distinguish a blocked RAM-weight call
from a blocked Flash/DROM-weight call because it is published only after both
calls return.

## Goal

Keep the LCD refreshing while the synchronous TIE convolution self-test runs,
and publish enough progress to distinguish these boundaries:

- `D:40`: the RAM-weight TIE call is about to run or is still running.
- `D:41`: the RAM-weight call returned and the Flash/DROM-weight call is about
  to run or is still running.
- `D:42`: both TIE calls returned; the LCD also shows the final `K:xy` result.

This diagnostic change does not attempt to fix TIE convolution behavior.

## Chosen Approach

Start the existing LCD display worker before invoking the self-test, then run
the self-test synchronously in the AgentGuard main thread as it does today.
Publish progress through a C-compatible query backed by C++ atomic state.

This preserves the TIE call's current task and coprocessor context. Moving the
self-test to another thread was rejected because that would add thread stack,
priority, and CP3 context-switch variables to the diagnosis. Forced timeout or
cancellation was rejected because the assembly kernel cannot be interrupted
safely.

## Components and Interfaces

### Self-test progress publication

`agentguard/espdl_tie_selftest.h` defines stable diagnostic stage constants
for idle and stages 40, 41, and 42, plus a read-only C API:

```c
uint8_t ag_espdl_tie_conv_selftest_get_stage(void);
```

`espdl_tie_selftest.cpp` stores the stage in `std::atomic<uint8_t>`. The writer
uses release stores immediately before each direct kernel call and after the
second call returns. The display worker uses the query's acquire load. No C
code reads the C++ atomic object directly.

The final result remains unavailable until both calls and comparisons finish.
Its publication must also be race-free: the result bits are written before a
release publication flag, and the query performs an acquire load before
copying them. If a compact atomic result encoding is clearer in implementation,
it may replace the separate fields as long as the public result API and this
ordering remain unchanged.

### Startup ordering

`ag_run()` performs these operations in order:

1. Open `/dev/lcd0`.
2. Start the independent display worker.
3. Wait for two configured display refresh intervals so the controller's white
   default is replaced by the diagnostic UI.
4. Run `ag_espdl_tie_conv_selftest_run_once()` synchronously.
5. Continue with I2C and camera initialization only if the self-test returns.

If either TIE call blocks, the display worker remains scheduled and continues
to submit LCD frames.

### LCD presentation

`struct ag_ui_status` gains a TIE diagnostic stage field. The display worker
refreshes the field from the atomic query on every iteration.

Before a final result exists, the diagnostic detail line renders exactly
`D:40` or `D:41`. When both calls return it renders `D:42 K:xy`, where the two
bits retain the existing RAM-then-Flash order. An idle stage may render the
existing `K:--` state before the first publication. The primary camera status
continues to use the stable `CAM:<phase>` numeric format; this change does not
restore changing text labels.

## Error and Blocking Behavior

- A blocked RAM call leaves the LCD refreshing at `D:40` indefinitely.
- A returned RAM call followed by a blocked Flash call leaves it at `D:41`.
- Two returned calls produce `D:42 K:xy` and permit normal startup to continue.
- A missing LCD worker still follows the existing retry loop; the self-test is
  not started until the worker starts successfully.
- The diagnostic adds no timeout, signal, cancellation, heap allocation, or
  recovery path around a TIE call.

## Testing and Acceptance

Host tests first define the exact stage values and LCD strings, including
`D:40`, `D:41`, and `D:42 K:10`. The implementation then adds atomic progress
publication and changes startup ordering.

Software verification requires:

- all eight AgentGuard host test executables pass with warnings as errors;
- the official NuttX target build succeeds;
- symbol/disassembly inspection confirms the display worker starts before the
  self-test call and that the display worker queries the stage;
- RAM and Flash fixtures remain mapped to internal RAM and DROM respectively;
- the generated image stays below the LittleFS boundary;
- flashing `/dev/ttyACM0` completes with write-hash verification.

Physical acceptance requires one LCD observation. Report `D:40`, `D:41`, or
`D:42 K:xy`, plus the visible `CAM:<phase>` value if present. A white screen is
still a failure; a visible stage is diagnostic evidence, not proof that TIE is
correct or that face detection is usable.

## Scope Boundaries

This change does not alter model files, face thresholds, inference scheduling,
camera configuration, the private internal-memory pool, TIE assembly, PC
protocol, or product behavior outside the diagnostic display and startup
ordering.
