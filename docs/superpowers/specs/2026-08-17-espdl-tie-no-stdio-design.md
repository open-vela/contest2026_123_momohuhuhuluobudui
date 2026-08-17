# ESP-DL TIE Startup No-stdio Design

**Date:** 2026-08-17

**Status:** Approved

## Evidence and Root Cause

The physical LCD reports `D:42 K:11 CAM:0`.

- `D:42` proves both direct TIE calls returned.
- `K:11` proves the RAM-weight and Flash/DROM-weight minimal convolutions both
  matched the scalar reference.
- `CAM:0` proves the AgentGuard main thread did not reach camera setup after
  publishing the complete self-test state.

Source and disassembly show that the only substantive work between publishing
stage 42 and returning from `ag_espdl_tie_conv_selftest_run_once()` is a chain
of `stderr` operations through `lib_get_stream`, `fprintf`, `fputc`, and
`fwrite`. Earlier physical debugging established that output to the unready
USB console can block AgentGuard startup. The root cause of this `CAM:0`
stall is therefore the synchronous diagnostic log after a successful
self-test, not either TIE call.

## Goal

Make the startup self-test return immediately after atomically publishing its
result and stage 42, allowing camera initialization to begin while preserving
`D:42 K:xy` on the LCD.

## Chosen Approach

Remove all standard-I/O activity from the TIE startup self-test. The LCD's
atomic stage and result display becomes the sole startup diagnostic channel.

Delaying the log until later startup was rejected because the same unavailable
USB sink could then block another product path. Adding stages 43 and 44 around
the log was rejected because the existing control-flow and historical evidence
already isolate that boundary; another flash/read cycle would not improve the
fix.

## Code Changes

`app/agentguard/src/espdl_tie_selftest.cpp` will no longer include `<cstdio>`,
define `print_vector()`, classify a result solely for printing, or call any
stdio function. After computing the two pass bits, it will publish the compact
atomic result, publish `AG_TIE_SELFTEST_STAGE_COMPLETE`, and return.

The direct TIE calls, their order, fixture contents, ABI assertions, atomic
ordering, result query, and LCD formatting remain unchanged.

## Regression Protection

A host-side source contract test will fail while the current stdio code exists
and pass only when the startup self-test source contains none of the prohibited
stdio dependencies or calls. The contract covers `<cstdio>`, `stderr`,
`stdout`, `fprintf`, `printf`, `fputc`, `fwrite`, and `print_vector`.

Target verification will additionally inspect the rebuilt `run_once`
disassembly and confirm it no longer reaches `lib_get_stream`, `fprintf`,
`fputc`, or `fwrite`.

## Verification and Acceptance

Software acceptance requires:

- the new no-stdio contract test fails before the production change;
- the contract and all existing host tests pass afterward;
- the NuttX target build succeeds;
- `run_once` disassembly contains no stdio calls;
- RAM and Flash fixtures remain in `0x3fc...` and `0x3c...` respectively;
- the image remains below the LittleFS boundary;
- flashing completes with write-hash verification.

Physical acceptance requires a reset and one LCD observation. `D:42 K:11`
must remain visible and `CAM:` must advance beyond zero. If camera progress
advances, this startup-log bug is resolved; the result does not by itself prove
that full-model TIE convolution or face-detection accuracy is correct.

## Scope Boundaries

This change does not modify TIE assembly, model data, thresholds, convolution
arguments, inference behavior, camera configuration, LCD layout, or PC/logging
features outside this startup self-test.
