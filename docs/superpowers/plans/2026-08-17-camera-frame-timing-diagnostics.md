# Camera Frame Timing Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Display camera completion interval, dequeue wait, main-loop frame interval, and ESP-DL inference duration on the ESP32-S3-EYE LCD so the approximately 500 ms preview cadence can be localized.

**Architecture:** A new pure C frame-timing module owns timestamp validation, interval calculation, clamping, and reset behavior. The AgentGuard main loop measures immediately around `VIDIOC_DQBUF`, feeds the V4L2 completion timestamp into that module, and copies a valid sample into the existing mutex-protected UI status. The display formatter gives complete timing diagnostics priority over the current model/TIE footer without changing frame processing.

**Tech Stack:** C11, POSIX/NuttX V4L2 `struct timeval`, host Make tests with warnings as errors, Python source-contract test, NuttX cross-build, esptool.

## Global Constraints

- Do not alter OV2640 registers, requested 15 FPS interval, V4L2 buffer count or ring mode, CAM DMA operation, inference cadence, model backend, thresholds, frame rotation, LCD refresh period, policy logic, or persistent logging.
- Render complete live metrics as `C:<ms> Q:<ms> L:<ms> AI:<ms>`.
- Clamp each displayed value above 9999 ms to `9999+`.
- Do not display timing diagnostics until `C`, `Q`, `L`, and model diagnostics are all valid.
- Reset timing history whenever the camera stream restarts.
- Never calculate an interval across a missing, zero, or backward timestamp.
- Preserve unrelated workspace changes and keep `.git.codex-hold` untouched.

---

### Task 1: Pure Frame-Timing State

**Files:**
- Create: `app/agentguard/include/agentguard/frame_timing.h`
- Create: `app/agentguard/src/frame_timing.c`
- Create: `app/agentguard/tests/test_frame_timing.c`
- Modify: `app/agentguard/tests/Makefile`

**Interfaces:**
- Produces: `struct ag_frame_timing_state` containing previous capture/acquisition timestamps, current `capture_interval_ms`, `dequeue_wait_ms`, `loop_interval_ms`, and `valid`/history flags.
- Produces: `void ag_frame_timing_reset(struct ag_frame_timing_state *state)`.
- Produces: `uint64_t ag_frame_timestamp_ms(int64_t seconds, int32_t microseconds)`; returns zero for negative seconds, microseconds outside `[0, 999999]`, or overflow.
- Produces: `bool ag_frame_timing_update(struct ag_frame_timing_state *state, uint64_t dequeue_started_ms, uint64_t dequeue_finished_ms, uint64_t capture_timestamp_ms)`.

- [ ] **Step 1: Add the failing host test and build target**

Create `test_frame_timing.c` with exact boundary tests:

```c
#include "agentguard/frame_timing.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
  struct ag_frame_timing_state state;

  ag_frame_timing_reset(&state);
  assert(!state.valid);
  assert(ag_frame_timestamp_ms(12, 345000) == 12345);
  assert(ag_frame_timestamp_ms(-1, 0) == 0);
  assert(ag_frame_timestamp_ms(1, -1) == 0);
  assert(ag_frame_timestamp_ms(1, 1000000) == 0);

  assert(!ag_frame_timing_update(&state, 100, 130, 1000));
  assert(!state.valid);
  assert(ag_frame_timing_update(&state, 560, 600, 1500));
  assert(state.capture_interval_ms == 500);
  assert(state.dequeue_wait_ms == 40);
  assert(state.loop_interval_ms == 470);

  assert(!ag_frame_timing_update(&state, 650, 640, 1600));
  assert(!state.valid);
  assert(!ag_frame_timing_update(&state, 700, 710, 0));
  assert(!state.valid);
  assert(!ag_frame_timing_update(&state, 800, 820, 1600));
  assert(!state.valid);
  assert(!ag_frame_timing_update(&state, 900, 930, 1400));
  assert(!state.valid);
  assert(ag_frame_timing_update(&state, 1000, 1040, 1900));
  assert(state.capture_interval_ms == 500);
  assert(state.loop_interval_ms == 110);

  ag_frame_timing_reset(&state);
  assert(!state.valid);
  assert(!ag_frame_timing_update(&state, 1000, 1010, 2000));

  puts("AgentGuard frame timing tests: PASS");
  return 0;
}
```

Add `test_frame_timing` to `TARGETS`, compile it from the test and new source, and execute it in the `test` recipe.

- [ ] **Step 2: Run the focused test to verify RED**

Run: `make -C app/agentguard/tests test_frame_timing`

Expected: FAIL because `agentguard/frame_timing.h` and its functions do not exist.

- [ ] **Step 3: Implement the minimal timing state**

Define the public state and functions. `ag_frame_timing_update()` must:

```c
if (state == NULL)
  return false;

state->valid = false;
if (dequeue_finished_ms < dequeue_started_ms)
  {
    state->have_acquisition_timestamp = false;
    return false;
  }

state->dequeue_wait_ms = clamp_u64_to_u32(
  dequeue_finished_ms - dequeue_started_ms);

capture_valid = state->have_capture_timestamp &&
                capture_timestamp_ms > state->previous_capture_ms;
loop_valid = state->have_acquisition_timestamp &&
             dequeue_finished_ms >= state->previous_acquisition_ms;

if (capture_timestamp_ms == 0)
  {
    state->have_capture_timestamp = false;
  }
else
  {
    if (capture_valid)
      state->capture_interval_ms = clamp_u64_to_u32(
        capture_timestamp_ms - state->previous_capture_ms);
    state->previous_capture_ms = capture_timestamp_ms;
    state->have_capture_timestamp = true;
  }

if (loop_valid)
  state->loop_interval_ms = clamp_u64_to_u32(
    dequeue_finished_ms - state->previous_acquisition_ms);
state->previous_acquisition_ms = dequeue_finished_ms;
state->have_acquisition_timestamp = true;
state->valid = capture_valid && loop_valid;
return state->valid;
```

Perform the null check before dereferencing `state`. A missing capture
timestamp and an invalid dequeue clock pair clear their corresponding history
flag so the next sample establishes a new baseline instead of spanning the bad
sample. Use `UINT32_MAX` when a calculated interval exceeds the public field
width. `ag_frame_timestamp_ms()` must reject multiplication overflow before
calculating `seconds * 1000 + microseconds / 1000`.

- [ ] **Step 4: Verify GREEN and regression**

Run: `make -C app/agentguard/tests clean test_frame_timing && app/agentguard/tests/test_frame_timing`

Expected: `AgentGuard frame timing tests: PASS`.

Run: `make -C app/agentguard/tests test`

Expected: all existing tests, the new frame-timing test, and the no-stdio contract pass.

- [ ] **Step 5: Commit the timing module**

Stage only the four Task 1 files and commit:

```text
feat: add camera frame timing state
```

---

### Task 2: Timing Diagnostic Footer

**Files:**
- Modify: `app/agentguard/include/agentguard/display_ui.h`
- Modify: `app/agentguard/src/display_ui.c`
- Modify: `app/agentguard/tests/test_display_ui.c`

**Interfaces:**
- Consumes: `capture_interval_ms`, `dequeue_wait_ms`, and `loop_interval_ms` from Task 1.
- Extends: `struct ag_ui_status` with those three `uint32_t` fields and `bool frame_timing_valid`.
- Produces: `void ag_ui_format_frame_timing(char *buffer, size_t buffer_size, uint32_t capture_interval_ms, uint32_t dequeue_wait_ms, uint32_t loop_interval_ms, uint32_t inference_ms)`.
- Changes: `ag_ui_format_diagnostic_detail()` selects timing text first only when both `frame_timing_valid` and `model_diagnostics_valid` are true.

- [ ] **Step 1: Write failing formatting and precedence tests**

Increase the test-local `diagnostics` buffer to 40 bytes. Add:

```c
ag_ui_format_frame_timing(diagnostics, sizeof(diagnostics),
                          500, 430, 500, 70);
assert(strcmp(diagnostics, "C:500 Q:430 L:500 AI:70") == 0);

ag_ui_format_frame_timing(diagnostics, sizeof(diagnostics),
                          10000, UINT32_MAX, 9999, 12345);
assert(strcmp(diagnostics,
              "C:9999+ Q:9999+ L:9999 AI:9999+") == 0);
```

In the existing diagnostic-selection block, set the three timing fields and
`status.frame_timing_valid = true`, then assert the timing string wins. Clear
`frame_timing_valid` and assert the current `M:3 AI:847 K:11` string returns.

- [ ] **Step 2: Run the focused test to verify RED**

Run: `make -C app/agentguard/tests test_display_ui`

Expected: FAIL under `-Werror` because `ag_ui_format_frame_timing` and the new status fields are absent.

- [ ] **Step 3: Implement bounded value formatting and selection**

Add a private helper that writes either the decimal value or `9999+` into a
caller-provided small buffer. Build the exact timing string with `snprintf`:

```c
snprintf(buffer, buffer_size, "C:%s Q:%s L:%s AI:%s",
         capture_text, dequeue_text, loop_text, inference_text);
```

In `ag_ui_format_diagnostic_detail()`, place this condition before the current
model-diagnostics condition:

```c
if (status->frame_timing_valid && status->model_diagnostics_valid)
  {
    ag_ui_format_frame_timing(buffer, buffer_size,
                              status->capture_interval_ms,
                              status->dequeue_wait_ms,
                              status->loop_interval_ms,
                              status->inference_ms);
    return true;
  }
```

Do not change header, footer, colors, glyphs, or seated-time fallback.

- [ ] **Step 4: Verify GREEN and full host regression**

Run: `make -C app/agentguard/tests clean test`

Expected: all host tests and source-contract tests pass.

- [ ] **Step 5: Commit the UI boundary**

Stage only the three Task 2 files and commit:

```text
feat: display camera frame timing diagnostics
```

---

### Task 3: Main-Loop Measurement Integration

**Files:**
- Modify: `app/agentguard/Makefile`
- Modify: `app/agentguard/src/agentguard_main.c`
- Create: `app/agentguard/tests/test_frame_timing_integration.py`
- Modify: `app/agentguard/tests/Makefile`

**Interfaces:**
- Consumes: Task 1 `ag_frame_timing_reset()`, `ag_frame_timestamp_ms()`, and `ag_frame_timing_update()`.
- Consumes: Task 2 timing fields in `struct ag_ui_status`.
- Changes: `ag_video_restart()` accepts a `struct ag_frame_timing_state *` and resets it before closing/reopening the stream.

- [ ] **Step 1: Add a failing source-contract test**

Create `test_frame_timing_integration.py` that reads
`src/agentguard_main.c` and asserts all of these exact contracts:

```python
from pathlib import Path

source = (Path(__file__).parent.parent / "src" /
          "agentguard_main.c").read_text(encoding="utf-8")

assert '#include "agentguard/frame_timing.h"' in source
assert "dequeue_started_ms = ag_now_ms();" in source
assert "dequeue_finished_ms = ag_now_ms();" in source
assert source.index("dequeue_started_ms = ag_now_ms();") < source.index(
    "ag_next_frame(&video, &watchdog, &frame)")
assert source.index("ag_next_frame(&video, &watchdog, &frame)") < source.index(
    "dequeue_finished_ms = ag_now_ms();")
assert "ag_frame_timestamp_ms(frame.timestamp.tv_sec," in source
assert "ag_frame_timing_update(&frame_timing," in source
assert "ui_status.frame_timing_valid = frame_timing.valid;" in source
assert "ag_frame_timing_reset(frame_timing);" in source
```

Add `python3 test_frame_timing_integration.py` to the test recipe.

- [ ] **Step 2: Verify the integration contract is RED**

Run: `python3 app/agentguard/tests/test_frame_timing_integration.py`

Expected: FAIL at the missing `frame_timing.h` include assertion.

- [ ] **Step 3: Integrate measurement without changing frame behavior**

Add `src/frame_timing.c` to `CSRCS` and include the header from
`agentguard_main.c`. In `ag_run()`, declare the timing state plus
`dequeue_started_ms`, `dequeue_finished_ms`, and `capture_timestamp_ms`, then
reset the state once before the frame loop.

Measure around the existing dequeue call in this order:

```c
dequeue_started_ms = ag_now_ms();
if (ag_next_frame(&video, &watchdog, &frame) < 0)
  {
    if (ag_video_restart(&video, &display_worker, &frame_timing) < 0)
      {
        usleep(AG_CAMERA_RESTART_DELAY_US);
      }
    continue;
  }
dequeue_finished_ms = ag_now_ms();
capture_timestamp_ms = ag_frame_timestamp_ms(frame.timestamp.tv_sec,
                                              frame.timestamp.tv_usec);
ag_frame_timing_update(&frame_timing,
                       dequeue_started_ms,
                       dequeue_finished_ms,
                       capture_timestamp_ms);
```

When populating `ui_status`, copy the state before publishing:

```c
ui_status.frame_timing_valid = frame_timing.valid;
ui_status.capture_interval_ms = frame_timing.capture_interval_ms;
ui_status.dequeue_wait_ms = frame_timing.dequeue_wait_ms;
ui_status.loop_interval_ms = frame_timing.loop_interval_ms;
```

Change `ag_video_restart()` to reset the supplied timing state before
`ag_video_close()`, and pass `&frame_timing` at both the dequeue-error and
`VIDIOC_QBUF`-error call sites. Preserve the existing error messages and the
`camera stream recovered` message around the expanded call shown above.

- [ ] **Step 4: Verify integration and all host tests**

Run: `python3 app/agentguard/tests/test_frame_timing_integration.py`

Expected: exit zero.

Run: `make -C app/agentguard/tests clean test`

Expected: every compiled host test plus both Python source-contract tests pass.

- [ ] **Step 5: Commit the runtime integration**

Stage only the four Task 3 files and commit:

```text
feat: measure camera frame pipeline timing
```

---

### Task 4: Target Verification, Flash, and Physical Reading

**Files:**
- Inspect: `/home/yhx/Desktop/openvela/nuttx/nuttx.bin`
- Hardware: `/dev/ttyACM0`

**Interfaces:**
- Consumes: complete Tasks 1-3 firmware.
- Produces: verified physical `C`, `Q`, `L`, and `AI` readings that select the next root-cause branch.

- [ ] **Step 1: Run clean host verification and diff checks**

Run:

```text
make -C app/agentguard/tests clean test
git diff --check
```

Expected: all tests pass and no whitespace errors are reported.

- [ ] **Step 2: Cross-build the NuttX target**

Activate `/home/yhx/Desktop/openvela/myenv/bin/activate` and run:

```text
make -C /home/yhx/Desktop/openvela/nuttx -j8
```

Expected: successful `nuttx.bin` generation with no link failure.

- [ ] **Step 3: Freeze and validate the exact image**

Record `stat` size and `sha256sum` for `nuttx.bin`. Require size below
`0x300000`, then run:

```text
esptool --chip esp32s3 image-info /home/yhx/Desktop/openvela/nuttx/nuttx.bin
```

Expected: a valid ESP32-S3 image and checksum.

- [ ] **Step 4: Flash the captured image without rebuilding**

Write that exact image at offset `0x0` through `/dev/ttyACM0`. Require
esptool's `Hash of data verified`. If the device is not in download mode,
request only the physical BOOT/RESET action needed to continue with the same
captured image.

- [ ] **Step 5: Collect and interpret the physical metrics**

After physical RESET, request the exact footer in this form:

```text
C:<ms> Q:<ms> L:<ms> AI:<ms>
```

Interpret it using the committed design thresholds:

- `C` and `L` within 100 ms of 500 ms: CAM/sensor completion-rearm path.
- `C` at least 100 ms and 25 percent below `L`: application work or buffer starvation.
- `Q` differs from `C` by no more than 100 ms: dequeue is predominantly
  waiting on the camera.
- `Q` is at most 50 ms while `L` is within 100 ms of 500 ms: time is spent
  after dequeue.
- `C` and `L` both at least 100 ms and 25 percent below the visual interval: display publication/copy path.

Do not implement the performance fix until this physical reading selects one branch.
