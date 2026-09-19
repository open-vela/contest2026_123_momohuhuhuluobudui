# LCD SPI DMA Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the ESP32-S3-EYE LCD's visible half-second edge-to-edge repaint by enabling coherent SPI DMA and expose the blocking LCD write duration as `D` on the diagnostic footer.

**Architecture:** A small pure C timing state validates and stores the display worker's most recent `LCDDEVIO_PUTAREA` duration. The existing UI status gains an optional LCD duration and formats complete diagnostics as `C/Q/L/D`; the display worker renders the previous valid duration, cleans the complete draw buffer cache range, submits the existing strided area, and measures the blocking ioctl. AgentGuard's reproducible configuration script enables the existing ESP32-S3 SPI DMA path without modifying NuttX driver sources.

**Tech Stack:** C11, POSIX monotonic clock, NuttX LCD character API, NuttX data-cache API, ESP32-S3 SPI DMA Kconfig, Python source-contract tests, host Make tests with warnings as errors, NuttX cross-build, esptool.

## Global Constraints

- Do not modify the NuttX ST7789 or ESP32-S3 SPI driver sources.
- Keep the LCD clock at 40 MHz and the visible area at 240x240 RGB565.
- Keep the current 320-pixel-stride draw buffer, 180-degree rotation, camera mode, V4L2 buffering, inference cadence, model thresholds, policy, network, and storage behavior.
- Enable exactly `CONFIG_ESP32S3_SPI_DMA=y`, `CONFIG_ESP32S3_SPI_DMA_BUFSIZE=2048`, and `CONFIG_ESP32S3_SPI_DMATHRESHOLD=64` through `tools/apply_agentguard_config.sh`.
- Clean the complete `AG_FRAME_BYTES` draw-buffer range after all CPU rendering and before every submitted LCD DMA transfer.
- Display the previous successful LCD write duration as `D`; never claim a valid `D` for the first frame, a failed clock read, a backward clock pair, or a failed LCD ioctl.
- Clamp displayed durations above 9999 ms to `9999+`.
- Accept the physical result only if the display remains coherent, the edge-to-edge repaint is no longer obvious, and normal `D` values are below 100 ms.
- Preserve unrelated workspace changes and keep `.git.codex-hold` untouched.

---

### Task 1: Pure LCD Write-Timing State

**Files:**
- Create: `app/agentguard/include/agentguard/lcd_timing.h`
- Create: `app/agentguard/src/lcd_timing.c`
- Create: `app/agentguard/tests/test_lcd_timing.c`
- Modify: `app/agentguard/tests/Makefile`
- Modify: `app/agentguard/Makefile`

**Interfaces:**
- Produces: `struct ag_lcd_timing_state { uint32_t write_ms; bool valid; }`.
- Produces: `void ag_lcd_timing_reset(struct ag_lcd_timing_state *state)`.
- Produces: `void ag_lcd_timing_invalidate(struct ag_lcd_timing_state *state)`.
- Produces: `bool ag_lcd_timing_update(struct ag_lcd_timing_state *state, uint64_t started_ms, uint64_t finished_ms)`; a nonnegative interval is valid and values beyond `UINT32_MAX` clamp to `UINT32_MAX`.

- [ ] **Step 1: Add the failing host test and build target**

Create `test_lcd_timing.c`:

```c
/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/lcd_timing.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
  struct ag_lcd_timing_state state;

  ag_lcd_timing_reset(&state);
  assert(!state.valid);

  assert(ag_lcd_timing_update(&state, 100, 131));
  assert(state.valid);
  assert(state.write_ms == 31);

  assert(!ag_lcd_timing_update(&state, 200, 199));
  assert(!state.valid);

  assert(ag_lcd_timing_update(&state, 300, 300));
  assert(state.valid);
  assert(state.write_ms == 0);

  assert(ag_lcd_timing_update(&state, 0, UINT64_MAX));
  assert(state.write_ms == UINT32_MAX);

  ag_lcd_timing_invalidate(&state);
  assert(!state.valid);
  assert(!ag_lcd_timing_update(NULL, 0, 1));

  puts("AgentGuard LCD timing tests: PASS");
  return 0;
}
```

Add `test_lcd_timing` to `TARGETS`, compile it from the new test and source,
and execute it after `test_frame_timing` in the `test` recipe. Add
`src/lcd_timing.c` to the product `CSRCS` in `app/agentguard/Makefile`.

- [ ] **Step 2: Run the focused test to verify RED**

Run: `make -C app/agentguard/tests test_lcd_timing`

Expected: FAIL because `agentguard/lcd_timing.h` does not exist.

- [ ] **Step 3: Implement the minimal timing state**

Create the header with the exact public struct and declarations from the
Interfaces block:

```c
/* SPDX-License-Identifier: Apache-2.0 */

#ifndef AGENTGUARD_LCD_TIMING_H
#define AGENTGUARD_LCD_TIMING_H

#include <stdbool.h>
#include <stdint.h>

struct ag_lcd_timing_state
{
  uint32_t write_ms;
  bool valid;
};

void ag_lcd_timing_reset(struct ag_lcd_timing_state *state);
void ag_lcd_timing_invalidate(struct ag_lcd_timing_state *state);
bool ag_lcd_timing_update(struct ag_lcd_timing_state *state,
                          uint64_t started_ms, uint64_t finished_ms);

#endif
```

Implement the source as:

```c
/* SPDX-License-Identifier: Apache-2.0 */

#include "agentguard/lcd_timing.h"

#include <limits.h>
#include <stddef.h>

void ag_lcd_timing_reset(struct ag_lcd_timing_state *state)
{
  if (state != NULL)
    {
      state->write_ms = 0;
      state->valid = false;
    }
}

void ag_lcd_timing_invalidate(struct ag_lcd_timing_state *state)
{
  if (state != NULL)
    {
      state->valid = false;
    }
}

bool ag_lcd_timing_update(struct ag_lcd_timing_state *state,
                          uint64_t started_ms, uint64_t finished_ms)
{
  uint64_t elapsed_ms;

  if (state == NULL)
    {
      return false;
    }

  state->valid = false;
  if (finished_ms < started_ms)
    {
      return false;
    }

  elapsed_ms = finished_ms - started_ms;
  state->write_ms = elapsed_ms > UINT32_MAX ? UINT32_MAX :
                    (uint32_t)elapsed_ms;
  state->valid = true;
  return true;
}
```

- [ ] **Step 4: Verify GREEN and host regression**

Run: `make -C app/agentguard/tests clean test_lcd_timing && app/agentguard/tests/test_lcd_timing`

Expected: `AgentGuard LCD timing tests: PASS`.

Run: `make -C app/agentguard/tests test`

Expected: all ten compiled host tests and the TIE no-stdio contract pass.

- [ ] **Step 5: Commit the timing boundary**

Stage only the five Task 1 files and commit:

```text
feat: add LCD write timing state
```

---

### Task 2: LCD Duration Diagnostic Footer

**Files:**
- Modify: `app/agentguard/include/agentguard/display_ui.h`
- Modify: `app/agentguard/src/display_ui.c`
- Modify: `app/agentguard/tests/test_display_ui.c`

**Interfaces:**
- Extends: `struct ag_ui_status` with `uint32_t lcd_write_ms` and `bool lcd_write_valid`.
- Produces: `void ag_ui_format_lcd_timing(char *buffer, size_t buffer_size, uint32_t capture_interval_ms, uint32_t dequeue_wait_ms, uint32_t loop_interval_ms, uint32_t lcd_write_ms)`.
- Changes: `ag_ui_format_diagnostic_detail()` selects `C/Q/L/D` when frame timing, model diagnostics, and LCD timing are all valid; otherwise its existing `C/Q/L/AI`, model, TIE-progress, and unavailable fallbacks remain intact.

- [ ] **Step 1: Write failing formatting, clamping, and first-sample tests**

After the existing `ag_ui_format_frame_timing()` assertions in
`test_display_ui.c`, add:

```c
ag_ui_format_lcd_timing(diagnostics, sizeof(diagnostics),
                        92, 40, 94, 31);
assert(strcmp(diagnostics, "C:92 Q:40 L:94 D:31") == 0);

ag_ui_format_lcd_timing(diagnostics, sizeof(diagnostics),
                        10000, UINT32_MAX, 9999, 12345);
assert(strcmp(diagnostics,
              "C:9999+ Q:9999+ L:9999 D:9999+") == 0);
```

In the diagnostic-selection block, keep the existing first timing assertion,
then add:

```c
status.lcd_write_valid = true;
status.lcd_write_ms = 31;
assert(ag_ui_format_diagnostic_detail(diagnostics, sizeof(diagnostics),
                                      &status));
assert(strcmp(diagnostics, "C:500 Q:430 L:500 D:31") == 0);

status.lcd_write_valid = false;
assert(ag_ui_format_diagnostic_detail(diagnostics, sizeof(diagnostics),
                                      &status));
assert(strcmp(diagnostics, "C:500 Q:430 L:500 AI:847") == 0);
```

This last assertion is the explicit first-frame behavior: no valid previous
LCD measurement means no `D` claim.

- [ ] **Step 2: Run the focused test to verify RED**

Run: `make -C app/agentguard/tests clean test_display_ui`

Expected: FAIL under `-Werror` because `ag_ui_format_lcd_timing()` and the two
status fields do not exist.

- [ ] **Step 3: Implement LCD timing formatting and precedence**

Declare the new formatter in `display_ui.h`. Reuse the existing private
`ag_ui_format_bounded_ms()` helper for all four values and add this function:

```c
void ag_ui_format_lcd_timing(char *buffer, size_t buffer_size,
                             uint32_t capture_interval_ms,
                             uint32_t dequeue_wait_ms,
                             uint32_t loop_interval_ms,
                             uint32_t lcd_write_ms)
{
  char capture_text[6];
  char dequeue_text[6];
  char loop_text[6];
  char display_text[6];

  if (buffer == NULL || buffer_size == 0)
    {
      return;
    }

  ag_ui_format_bounded_ms(capture_text, sizeof(capture_text),
                          capture_interval_ms);
  ag_ui_format_bounded_ms(dequeue_text, sizeof(dequeue_text),
                          dequeue_wait_ms);
  ag_ui_format_bounded_ms(loop_text, sizeof(loop_text), loop_interval_ms);
  ag_ui_format_bounded_ms(display_text, sizeof(display_text), lcd_write_ms);
snprintf(buffer, buffer_size, "C:%s Q:%s L:%s D:%s",
         capture_text, dequeue_text, loop_text, display_text);
}
```

Add `lcd_write_ms` next to the other duration fields and `lcd_write_valid`
next to `frame_timing_valid`. At the top of the timing selection in
`ag_ui_format_diagnostic_detail()`, add:

```c
if (status->frame_timing_valid && status->model_diagnostics_valid &&
    status->lcd_write_valid)
  {
    ag_ui_format_lcd_timing(buffer, buffer_size,
                            status->capture_interval_ms,
                            status->dequeue_wait_ms,
                            status->loop_interval_ms,
                            status->lcd_write_ms);
    return true;
  }
```

Leave the existing `C/Q/L/AI` branch immediately after it so startup and an
invalid LCD sample retain their current behavior.

- [ ] **Step 4: Verify GREEN and full host regression**

Run: `make -C app/agentguard/tests clean test`

Expected: all ten compiled host tests and the no-stdio contract pass.

- [ ] **Step 5: Commit the UI boundary**

Stage only the three Task 2 files and commit:

```text
feat: display LCD write timing diagnostics
```

---

### Task 3: Testable LCD Transfer Boundary

**Files:**
- Create: `app/agentguard/include/agentguard/lcd_transfer.h`
- Create: `app/agentguard/src/lcd_transfer.c`
- Create: `app/agentguard/tests/test_lcd_transfer.c`
- Modify: `app/agentguard/tests/Makefile`
- Modify: `app/agentguard/Makefile`

**Interfaces:**
- Consumes: Task 1 LCD timing state.
- Produces: `struct ag_lcd_transfer_ops` callbacks for monotonic time, cache clean, and hardware submission plus an opaque context.
- Produces: `int ag_lcd_transfer_run(const struct ag_lcd_transfer_ops *ops, uintptr_t buffer_start, size_t buffer_size, struct ag_lcd_timing_state *timing)`.
- Guarantees: cleans `[buffer_start, buffer_start + buffer_size)`, reads the start clock, submits once, reads the finish clock after successful submission, and publishes timing only when all required operations succeed with a non-backward clock.

- [ ] **Step 1: Write the failing real transfer-sequencing test**

Create a fake hardware context whose callbacks append `READ`, `CLEAN`, and
`SUBMIT` to an event array. Test these literal behaviors:

```c
assert(ag_lcd_transfer_run(&ops, 0x1000, 153600, &timing) == 0);
assert(context.event_count == 4);
assert(context.events[0] == EVENT_CLEAN);
assert(context.events[1] == EVENT_READ);
assert(context.events[2] == EVENT_SUBMIT);
assert(context.events[3] == EVENT_READ);
assert(context.clean_start == 0x1000);
assert(context.clean_end == 0x26800);
assert(timing.valid);
assert(timing.write_ms == 31);
```

Then set the fake submit result to `-1`; assert the event sequence stops after
`CLEAN/READ/SUBMIT` and timing becomes invalid. Separately make the first clock
read fail and use a backward `200/199` clock pair; each case must still submit
the frame but leave timing invalid.

Add `test_lcd_transfer` to `TARGETS`, link it with `lcd_transfer.c` and
`lcd_timing.c`, and run it after `test_lcd_timing`. Add
`src/lcd_transfer.c` to the product `CSRCS`.

- [ ] **Step 2: Run the focused test to verify RED**

Run: `make -C app/agentguard/tests test_lcd_transfer`

Expected: FAIL because `agentguard/lcd_transfer.h` does not exist.

- [ ] **Step 3: Implement the minimal transfer boundary**

Define callbacks with these exact signatures:

```c
bool (*read_ms)(void *context, uint64_t *value);
void (*clean)(void *context, uintptr_t start, uintptr_t end);
int (*submit)(void *context);
```

`ag_lcd_transfer_run()` validates all pointers, always cleans, reads the start
clock, and submits once, then returns immediately with invalid timing on a failed
submission, then reads the finish clock and calls `ag_lcd_timing_update()` only
when both clock reads succeeded. Missing operations return `-1` and invalidate
timing. The hardware submit return value is returned unchanged.

- [ ] **Step 4: Verify GREEN and regression**

Run: `make -C app/agentguard/tests clean test_lcd_transfer && app/agentguard/tests/test_lcd_transfer`

Expected: `AgentGuard LCD transfer tests: PASS`.

Run: `make -C app/agentguard/tests test`

Expected: all eleven compiled tests and the no-stdio contract pass.

- [ ] **Step 5: Commit the transfer boundary**

Stage only the five Task 3 files and commit:

```text
feat: add coherent LCD transfer boundary
```

---

### Task 4: SPI DMA Configuration and Display Integration

**Files:**
- Create: `app/agentguard/tests/test_lcd_dma_config.py`
- Modify: `app/agentguard/tests/Makefile`
- Modify: `tools/apply_agentguard_config.sh`
- Modify: `app/agentguard/src/agentguard_main.c`

**Interfaces:**
- Consumes: Task 2 LCD UI fields and Task 3 transfer boundary.
- Produces: reproducible SPI DMA values by executing the real product configuration script.
- Adapts: NuttX `clock_gettime`, `up_clean_dcache`, and `LCDDEVIO_PUTAREA` to `struct ag_lcd_transfer_ops`.

- [ ] **Step 1: Add the failing configuration behavior test**

Create `test_lcd_dma_config.py`. In a `tempfile.TemporaryDirectory()`, build
the layout `openvela/contest/tools`, copy the real configuration script there,
create a minimal `openvela/nuttx/.config`, and install a fake executable
`openvela/prebuilts/build-tools/linux-x86_64/bin/kconfig-tweak` that implements
`--enable`, `--disable`, `--set-val`, and `--set-str` by updating that temporary
`.config`. Add a minimal NuttX Makefile whose `include/nuttx/config.h` target
succeeds, then run the copied script with `subprocess.run(..., check=True)`.

Parse the resulting `.config` and assert these exact effects:

```python
assert values["CONFIG_ESP32S3_SPI_DMA"] == "y"
assert values["CONFIG_ESP32S3_SPI_DMA_BUFSIZE"] == "2048"
assert values["CONFIG_ESP32S3_SPI_DMATHRESHOLD"] == "64"
```

This test exercises the real shell script in an isolated filesystem; it does
not search the script's source text. Add it after the no-stdio test in the
`test` recipe.

- [ ] **Step 2: Run the configuration test to verify RED**

Run: `python3 app/agentguard/tests/test_lcd_dma_config.py`

Expected: FAIL because the resulting temporary `.config` does not contain
`CONFIG_ESP32S3_SPI_DMA`.

- [ ] **Step 3: Add the minimal reproducible DMA configuration**

After the camera XCLK setting in `apply_agentguard_config.sh`, add:

```bash
# The ST7789 frame is 115,200 bytes.  Polling SPI splits it into roughly 1,800
# 64-byte transactions and visibly scans the LCD.  Use the existing SPI2 DMA
# path; the display worker cleans its cached transmit buffer before submission.
"$tweak" --file "$config_file" --enable ESP32S3_SPI_DMA
"$tweak" --file "$config_file" --set-val ESP32S3_SPI_DMA_BUFSIZE 2048
"$tweak" --file "$config_file" --set-val ESP32S3_SPI_DMATHRESHOLD 64
```

Run: `python3 app/agentguard/tests/test_lcd_dma_config.py`

Expected: `AgentGuard LCD DMA config test: PASS`.

- [ ] **Step 4: Adapt the tested transfer boundary to NuttX**

Include `agentguard/lcd_timing.h`, `agentguard/lcd_transfer.h`, and
`<nuttx/cache.h>`. Add a small submit context containing the display and
`lcddev_area_s` pointers. Its three adapters must:

```c
read_ms: call clock_gettime(CLOCK_MONOTONIC), reject failure/negative seconds,
         and return milliseconds through the output pointer
clean:   call up_clean_dcache(start, end)
submit:  call ioctl(display->fd, LCDDEVIO_PUTAREA, (uintptr_t)area)
```

Extend `ag_display_frame()` with `struct ag_lcd_timing_state *lcd_timing`,
construct the operations/context after the area is complete, and call:

```c
result = ag_lcd_transfer_run(&ops, (uintptr_t)pixels, AG_FRAME_BYTES,
                             lcd_timing);
```

On a negative result, preserve the existing error message and close behavior.
Declare/reset the timing state once in `ag_display_worker_main()`, copy its
previous `valid/write_ms` into the local UI status before rendering, and pass
it to `ag_display_frame()`. Frame N therefore renders frame N-1's duration.

- [ ] **Step 5: Verify integration and full host regression**

Run: `make -C app/agentguard/tests clean test`

Expected: all eleven compiled tests plus the no-stdio and real configuration
behavior tests pass.

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  diff --check -- tools/apply_agentguard_config.sh \
  app/agentguard/src/agentguard_main.c app/agentguard/tests/Makefile \
  app/agentguard/tests/test_lcd_dma_config.py
```

Expected: no output and exit status 0.

- [ ] **Step 6: Commit coherent DMA integration**

Stage only the four Task 4 files and commit:

```text
feat: accelerate LCD writes with coherent SPI DMA
```

---

### Task 5: Target Configuration, Build, Flash, and Physical Validation

**Files:**
- Verify only: `/home/yhx/Desktop/openvela/nuttx/.config`
- Verify only: `/home/yhx/Desktop/openvela/nuttx/nuttx.bin`

**Interfaces:**
- Consumes: the committed configuration script and complete AgentGuard target sources from Tasks 1-4.
- Produces: a hash-verified firmware image and physical `D` measurements; no source file is modified in this task.

- [ ] **Step 1: Recheck workspace identity and apply product configuration**

Run:

```bash
skills/develop-agentguard-openvela/scripts/check_workspace.sh \
  /home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  /home/yhx/Desktop/openvela
```

Expected: workspace checks pass for the contest repository, outer openvela
checkout, target board, active app mapping, and branch.

Run:

```bash
tools/apply_agentguard_config.sh \
  /home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  /home/yhx/Desktop/openvela
```

Expected: `AgentGuard configuration applied` and generated NuttX headers
complete successfully.

- [ ] **Step 2: Verify the generated DMA values**

Run:

```bash
rg -n '^CONFIG_ESP32S3_SPI_DMA=y$|^CONFIG_ESP32S3_SPI_DMA_BUFSIZE=2048$|^CONFIG_ESP32S3_SPI_DMATHRESHOLD=64$' \
  /home/yhx/Desktop/openvela/nuttx/.config
```

Expected: exactly the three requested values are printed.

- [ ] **Step 3: Run fresh host and target builds**

Run: `make -C app/agentguard/tests clean test`

Expected: all eleven compiled tests plus the no-stdio and real configuration
behavior tests pass.

Activate `/home/yhx/Desktop/openvela/myenv/bin/activate`, then run:

```bash
make -C /home/yhx/Desktop/openvela/nuttx -j8
```

Expected: the NuttX build exits 0 and generates `nuttx.bin`.

- [ ] **Step 4: Inspect the image and changed-file hygiene**

Run:

```bash
sha256sum /home/yhx/Desktop/openvela/nuttx/nuttx.bin
/home/yhx/Desktop/openvela/myenv/bin/esptool --chip esp32s3 \
  image-info /home/yhx/Desktop/openvela/nuttx/nuttx.bin
```

Expected: record the SHA-256; esptool recognizes an ESP32-S3 image and reports
a valid image checksum.

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  diff --check
```

Expected: no whitespace errors in tracked changes.

- [ ] **Step 5: Flash with write verification**

With `/dev/ttyACM0` free and the same Python environment active, run:

```bash
make -C /home/yhx/Desktop/openvela/nuttx -j8 flash \
  ESPTOOL_PORT=/dev/ttyACM0 ESPTOOL_BINDIR=./
```

Expected: esptool reports `Hash of data verified` and resets the board.

- [ ] **Step 6: Obtain physical acceptance evidence**

Observe the live AgentGuard display and record the stable ranges for
`C`, `Q`, `L`, and `D`. Confirm all of the following:

```text
D is normally below 100 ms
no obvious top-to-bottom or bottom-to-top full-frame repaint
camera motion is substantially smoother
colors, footer, face box, and other overlays are coherent
C/Q/L remain in the same broad range as before the DMA change
```

If the display corrupts or faults, stop acceptance and capture the exact visual
or serial symptom; the next design uses an internal-RAM DMA bounce strategy.
If the image is coherent but `D` remains at least 100 ms, capture its range;
the next design uses a contiguous 240x240 transfer buffer. Do not change those
architectures inside this plan.

- [ ] **Step 7: Record final repository state**

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  status --short
```

Expected: only pre-existing unrelated changes and build artifacts remain. No
commit is created in this verification-only task.
