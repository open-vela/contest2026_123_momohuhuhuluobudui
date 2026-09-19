# ESP-DL Runtime Metrics Display Design

**Date:** 2026-08-17

**Status:** Approved

## Context

After removing blocking startup stdio, the physical ESP32-S3-EYE reports
`D:42 K:11`, reaches `NO PERSON`, and displays live camera frames. The LCD
worker heartbeat flashes quickly and uniformly, while the camera image changes
only about once every 0.5 seconds.

The independent LCD worker therefore remains responsive. Camera frames are
published only after the synchronous vision call returns, and the ESP-DL
adapter runs the model every third input frame. The remaining leading
hypothesis is that model inference dominates the camera publish interval, but
the existing `D:42` footer priority hides the already collected
`inference_ms` value.

## Goal

Expose the existing live model candidate count and inference duration on the
LCD without changing inference, camera, or display timing.

## Chosen Presentation

Before the first valid live-model diagnostic, keep the current startup detail:

```text
D:42 K:11
```

Once `model_diagnostics_valid` becomes true, give the existing model detail
priority:

```text
M:<candidates> AI:<milliseconds> K:<ram><flash>
```

The final `K:` bits remain visible. `D:42` no longer remains in the footer
after the model produces a valid live inference result.

Timed alternation was rejected because it adds state and makes a physical
reading easy to miss. Combining `D`, `M`, `AI`, and `K` in one line was
rejected because it adds no diagnostic value after stage 42 and reduces
small-screen readability.

## Testable Boundary

Add a UI helper that selects and formats a diagnostic footer independently of
pixel rendering. It accepts `struct ag_ui_status`, writes to caller-owned
storage, and returns whether it produced diagnostic text.

Selection order is exact:

1. If `model_diagnostics_valid`, format with
   `ag_ui_format_model_diagnostics()`.
2. Otherwise, if the TIE stage is nonzero or the TIE result is valid, format
   with `ag_ui_format_tie_progress()`.
3. Otherwise return false so the existing seated-time/frame fallback renders.

The renderer uses this helper and makes no other layout or color change.

## Testing and Verification

Host tests first assert the desired precedence while the new helper is absent:

- stage 42 plus valid model diagnostics produces `M:3 AI:847 K:11`;
- stage 42 without valid model diagnostics produces `D:42 K:11`;
- no model or TIE diagnostic returns false.

The focused test must fail before implementation and pass afterward. All host
tests, the no-stdio contract, and the NuttX target build must then pass.
The captured image is flashed with write-hash verification.

Physical acceptance requires reporting `M:`, `AI:`, and the approximate camera
update interval. This measurement identifies whether inference duration
explains the observed frame cadence; it does not itself optimize performance.

## Scope Boundaries

This change does not alter the model, thresholds, backend choice, inference
interval, camera format, frame buffers, LCD refresh period, TIE self-test, or
face-detection behavior.
