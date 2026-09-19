# Camera Frame Timing Diagnostics Design

**Date:** 2026-08-17

**Status:** Approved

## Context

The ESP32-S3-EYE now reaches the live AgentGuard screen and reports
`M:0 AI:70 K:11`. The LCD worker heartbeat changes quickly and uniformly, but
the camera image changes only about once every 0.5 seconds.

ESP-DL inference runs once every three captured frames. A measured 70 ms
inference therefore contributes about 23 ms per input frame on average and
cannot by itself explain the observed roughly 500 ms update interval. Static
inspection also shows that both the V4L2 default and the board OV2640
descriptor request 15 frames per second. The remaining delay must be localized
between sensor capture, `VIDIOC_DQBUF`, and the rest of the AgentGuard frame
loop before changing behavior.

The ESP32-S3 CAM lower half is a specific suspect: it stops DMA at each
completed frame, schedules a low-priority worker to copy the 320x240 RGB565
frame, invokes the V4L2 completion callback, and then rearms capture. This
design measures that path without assuming it is the cause.

## Goal

Expose enough timing data on the physical LCD to identify which frame-loop
boundary accounts for the approximately 500 ms camera update interval, while
leaving camera, inference, policy, and display behavior unchanged.

## Chosen Approach

Add four live values to the diagnostic footer:

```text
C:500 Q:430 L:500 AI:70
```

- `C` is the interval in milliseconds between consecutive V4L2 buffer
  timestamps. It represents the completed-frame cadence reported by the
  camera stack.
- `Q` is the duration in milliseconds of the current blocking
  `VIDIOC_DQBUF` call.
- `L` is the interval in milliseconds between consecutive successful frame
  acquisitions in the AgentGuard main loop.
- `AI` remains the most recently measured ESP-DL inference duration.

The footer temporarily omits `M` and `K`. Their latest values are already
known (`M:0`, `K:11`), while the four timing values are all needed for this
diagnostic decision. Startup continues to display the existing TIE progress
until live model diagnostics and frame timing are valid.

Two alternatives were rejected for this diagnostic step:

1. Disabling inference, rotation, and preview work in separate firmware builds
   would require several flashes and would provide only coarse attribution.
2. Converting the CAM lower half directly to continuous DMA is premature and
   risks buffer tearing, cache coherency errors, or ring-buffer regressions
   before the slow boundary is identified.

## Components and Data Flow

### Timing state

A small frame-timing state owned by `ag_run()` records:

- the previous valid V4L2 buffer timestamp;
- the previous successful frame-acquisition monotonic timestamp;
- the latest `C`, `Q`, and `L` values;
- whether enough samples exist to publish intervals.

The state has no thread sharing. It is updated only by the camera/main-loop
thread, then copied into `struct ag_ui_status`, which already crosses the
display-worker mutex by value.

### Measurement boundaries

For each frame attempt:

1. Read the monotonic clock immediately before `ag_next_frame()`.
2. Read it again immediately after a successful return. Their nonnegative
   difference becomes `Q`.
3. Convert `frame.timestamp` to milliseconds and subtract the previous valid
   buffer timestamp to obtain `C`.
4. Subtract the previous successful post-`DQBUF` monotonic timestamp to obtain
   `L`.
5. Run the existing vision, policy, rotation, and display-publication path
   unchanged.
6. Copy the timing values into the status published with that frame.

Using the V4L2 timestamp for `C` is important: the ESP32-S3 CAM interrupt
records it at frame completion, before the lower-half worker copies the frame
and before the application wakes from `DQBUF`.

### UI formatting

The diagnostic-detail formatter selects the timing form only after all three
frame metrics have a valid sample and model diagnostics are valid. Otherwise
it retains the existing model/TIE/fallback selection behavior.

Values greater than 9999 ms render as `9999+`. This keeps the line bounded and
avoids ambiguous truncation. A zero value is valid for sub-millisecond or
immediately available operations; unavailable first-sample intervals are not
shown as valid timing diagnostics.

## Error Handling

- A failed `DQBUF` does not update successful-frame interval state and follows
  the existing camera restart path.
- A missing, zero, or backward V4L2 timestamp invalidates `C` for that sample;
  subtraction never wraps.
- A monotonic clock failure or backward value invalidates the affected metric.
- Camera restart resets the timing state so intervals are never calculated
  across different streams.
- Diagnostic failures never prevent a frame from being queued back to V4L2.

## Testing

Implementation follows test-driven development.

Host tests cover a pure timing helper with synthetic inputs:

- the first successful frame does not claim complete interval data;
- a second frame calculates `C`, `Q`, and `L` at their exact boundaries;
- backward or missing timestamps invalidate the affected sample without
  unsigned wraparound;
- reset removes previous-stream history;
- values above the UI limit format with `9999+`;
- valid timing diagnostics take precedence over the existing `M/AI/K` footer;
- incomplete timing retains the existing footer.

After focused tests pass, all AgentGuard host tests and source-contract tests
must pass. The NuttX target build must complete, `git diff --check` must be
clean for the changed files, and the flashed image must pass esptool's write
hash verification.

## Physical Interpretation

The physical LCD readings determine the next fix:

- `C` and `L` both within 100 ms of the observed 500 ms interval: investigate
  the sensor/CAM completion and rearm path.
- `C` is both at least 100 ms and at least 25 percent below `L`: investigate
  application work or buffer starvation after frame completion.
- `Q` close to `C`: the main loop spends most of its time waiting for the
  camera.
- `Q` small while `L` is near 500 ms: frames are ready when requested and time
  is spent elsewhere in the loop.
- `C` and `L` are both at least 100 ms and at least 25 percent below the
  visually observed interval: investigate display publication/copying despite
  the responsive heartbeat.

No performance fix is included in this change. The next change will be based
on the measured boundary rather than on the current CAM-driver hypothesis.

## Scope Boundaries

This change does not alter OV2640 registers, requested frame rate, V4L2 buffer
count or mode, CAM DMA operation, inference cadence, model backend, thresholds,
frame rotation, LCD refresh period, policy logic, or persistent logging.
