# ESP-DL TIE Progress Display Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep the LCD alive during the synchronous TIE self-test and show the exact RAM/Flash call boundary as `D:40`, `D:41`, or `D:42 K:xy`.

**Architecture:** The existing display worker starts before the self-test and polls a C-compatible stage query. The target C++ self-test publishes stage and result state with acquire/release atomics while retaining both direct TIE calls in the AgentGuard main thread.

**Tech Stack:** C11, C++20 `std::atomic<uint8_t>`, NuttX pthread/LCD APIs, ESP-DL TIE728 assembly, host Make tests, Xtensa cross-build and objdump.

## Global Constraints

- Do not change the ESP-DL model, face thresholds, inference cadence, camera pipeline, private memory pool, or TIE assembly.
- Keep both direct TIE calls synchronous in the AgentGuard main thread.
- Start the self-test only after the LCD display worker has started and received two refresh intervals.
- Publish stages exactly as `40` before the RAM call, `41` after the RAM call and before the Flash call, and `42` after the Flash call returns.
- Use atomic acquire/release publication; do not expose an atomic C++ object directly to C code.
- Keep final `K:xy` bits in RAM-then-Flash order and unavailable until both calls and comparisons finish.
- Add no timeout, cancellation, recovery, heap allocation, or logging before the final self-test result.
- Preserve all unrelated and pre-existing workspace changes.

## File Structure

- Modify `app/agentguard/include/agentguard/display_ui.h`: add the UI stage field and progress formatter declaration.
- Modify `app/agentguard/src/display_ui.c`: format and render stage-aware diagnostic text.
- Modify `app/agentguard/tests/test_display_ui.c`: assert exact progress strings.
- Modify `app/agentguard/include/agentguard/espdl_tie_selftest.h`: define stable stage values and the C stage query.
- Modify `app/agentguard/tests/test_espdl_tie_selftest.cpp`: lock the numeric stage ABI.
- Modify `app/agentguard/src/agentguard_main.c`: start the display worker first and poll the stage query.
- Modify `app/agentguard/src/espdl_tie_selftest.cpp`: atomically publish progress and final results.
- Modify `AGENTGUARD_PROGRESS.md`: record build, image, flash, and pending physical evidence.

---

### Task 1: Stage-Aware LCD Formatting

**Files:**
- Modify: `app/agentguard/include/agentguard/display_ui.h`
- Modify: `app/agentguard/src/display_ui.c`
- Test: `app/agentguard/tests/test_display_ui.c`

**Interfaces:**
- Produces: `uint8_t ag_ui_status::tie_selftest_stage`.
- Produces: `void ag_ui_format_tie_progress(char *buffer, size_t buffer_size, uint8_t stage, bool valid, bool ram_pass, bool flash_pass)`.
- Consumes: existing `ag_ui_format_tie_selftest()` idle-result semantics.

- [ ] **Step 1: Write the failing progress-format tests**

Add to `test_display_ui.c` before a production declaration or implementation exists:

```c
ag_ui_format_tie_progress(diagnostics, sizeof(diagnostics), 0,
                          false, false, false);
assert(strcmp(diagnostics, "K:--") == 0);
ag_ui_format_tie_progress(diagnostics, sizeof(diagnostics), 40,
                          false, false, false);
assert(strcmp(diagnostics, "D:40") == 0);
ag_ui_format_tie_progress(diagnostics, sizeof(diagnostics), 41,
                          false, false, false);
assert(strcmp(diagnostics, "D:41") == 0);
ag_ui_format_tie_progress(diagnostics, sizeof(diagnostics), 42,
                          true, true, false);
assert(strcmp(diagnostics, "D:42 K:10") == 0);
```

- [ ] **Step 2: Run the focused test and verify RED**

Run: `make -C app/agentguard/tests test_display_ui`

Expected: compilation fails under `-Werror` because `ag_ui_format_tie_progress` is undeclared.

- [ ] **Step 3: Implement the minimal formatter and UI field**

Add `uint8_t tie_selftest_stage` beside the existing TIE result fields in `struct ag_ui_status`, declare the formatter, and implement:

```c
void ag_ui_format_tie_progress(char *buffer, size_t buffer_size,
                               uint8_t stage, bool valid,
                               bool ram_pass, bool flash_pass)
{
  if (buffer == NULL || buffer_size == 0)
    {
      return;
    }

  if (stage == 0)
    {
      ag_ui_format_tie_selftest(buffer, buffer_size, valid,
                                ram_pass, flash_pass);
    }
  else if (stage == 42 && valid)
    {
      snprintf(buffer, buffer_size, "D:42 K:%u%u",
               ram_pass ? 1 : 0, flash_pass ? 1 : 0);
    }
  else
    {
      snprintf(buffer, buffer_size, "D:%u", stage);
    }
}
```

In `ag_ui_render_rgb565()`, choose this formatter whenever `tie_selftest_stage != 0` or `tie_selftest_valid` is true on the no-model-diagnostics path. Leave model diagnostics and `CAM:<phase>` unchanged.

- [ ] **Step 4: Verify GREEN and the full host suite**

Run: `make -C app/agentguard/tests clean test`

Expected: all eight test executables compile with warnings as errors and pass.

- [ ] **Step 5: Commit the LCD behavior**

Stage only the header, UI implementation, and display test. Commit as `feat: display TIE self-test progress` using the repository's explicit `--git-dir` and `--work-tree` arguments.

---

### Task 2: Atomic Progress Publication and Startup Ordering

**Files:**
- Modify: `app/agentguard/include/agentguard/espdl_tie_selftest.h`
- Modify: `app/agentguard/tests/test_espdl_tie_selftest.cpp`
- Modify: `app/agentguard/src/agentguard_main.c`
- Modify: `app/agentguard/src/espdl_tie_selftest.cpp`

**Interfaces:**
- Produces: `AG_TIE_SELFTEST_STAGE_IDLE = 0`, `AG_TIE_SELFTEST_STAGE_RAM = 40`, `AG_TIE_SELFTEST_STAGE_FLASH = 41`, and `AG_TIE_SELFTEST_STAGE_COMPLETE = 42`.
- Produces: `uint8_t ag_espdl_tie_conv_selftest_get_stage(void)`.
- Consumes: `ag_ui_status::tie_selftest_stage` from Task 1.

- [ ] **Step 1: Define the stage ABI and lock it in a host test**

Add the enum and stage-query declaration to `espdl_tie_selftest.h`. Add:

```cpp
static_assert(AG_TIE_SELFTEST_STAGE_IDLE == 0);
static_assert(AG_TIE_SELFTEST_STAGE_RAM == 40);
static_assert(AG_TIE_SELFTEST_STAGE_FLASH == 41);
static_assert(AG_TIE_SELFTEST_STAGE_COMPLETE == 42);
```

Run: `make -C app/agentguard/tests test_espdl_tie_selftest`

Expected: the focused host test passes and locks the C ABI values before target wiring.

- [ ] **Step 2: Wire the target consumer before implementing the getter**

In each display-worker iteration under `CONFIG_AGENTGUARD_ESP_DL`, assign:

```c
status.tie_selftest_stage = ag_espdl_tie_conv_selftest_get_stage();
```

Move `ag_espdl_tie_conv_selftest_run_once()` from before `ag_display_worker_start()` to immediately after the existing two-refresh `usleep()`. Keep the display-worker retry loop ahead of the wait and self-test.

- [ ] **Step 3: Run the NuttX build and verify target RED**

Run:

```bash
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx -j8
```

Expected: final link fails with undefined `ag_espdl_tie_conv_selftest_get_stage`, proving the NuttX display path consumes the new API. If incremental dependencies do not relink, touch `app/agentguard/src/agentguard_main.c` and repeat.

- [ ] **Step 4: Implement atomic stage and final-result publication**

In `espdl_tie_selftest.cpp`, include `<atomic>` and replace plain result globals with:

```cpp
constexpr uint8_t kResultValid = 1u << 2;
constexpr uint8_t kResultRamPass = 1u << 1;
constexpr uint8_t kResultFlashPass = 1u << 0;
std::atomic<uint8_t> g_stage{AG_TIE_SELFTEST_STAGE_IDLE};
std::atomic<uint8_t> g_result{0};
```

Publish `RAM` immediately before the first TIE call, `FLASH` after the first returns and before the second, and `COMPLETE` after the second returns and the result has been published. Use release stores. Implement `get_stage()` and the rewritten `get_result()` with acquire loads. Decode the result from one loaded byte. Do not print or allocate before stage 42.

- [ ] **Step 5: Verify target GREEN and full host regression**

Run:

```bash
make -C app/agentguard/tests clean test
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx -j8
```

Expected: all eight host tests pass and NuttX links without unresolved atomic helper symbols.

- [ ] **Step 6: Inspect target wiring and fixture placement**

Use `xtensa-esp32s3-elf-nm -an` and `xtensa-esp32s3-elf-objdump -d -C` on `/home/yhx/Desktop/openvela/nuttx/nuttx`. Confirm `ag_run` reaches `run_once`, the display worker reaches `get_stage` and `get_result`, RAM storage maps in `0x3fc...`, and the const Flash filter maps in `0x3c...`.

- [ ] **Step 7: Commit the atomic target integration**

Stage only the self-test header/test, `agentguard_main.c`, and `espdl_tie_selftest.cpp`. Commit as `fix: keep LCD alive during TIE diagnostic` using explicit repository paths.

---

### Task 3: Firmware Evidence, Flash, and Handoff

**Files:**
- Modify: `AGENTGUARD_PROGRESS.md`
- Inspect: `/home/yhx/Desktop/openvela/nuttx/nuttx.bin`
- Hardware: `/dev/ttyACM0`

**Interfaces:**
- Consumes: the verified diagnostic image from Task 2.
- Produces: a reproducible evidence record and a flashed board awaiting one LCD readout.

- [ ] **Step 1: Verify image size, boundary, and hash**

Run `stat -c '%s'`, `sha256sum`, and `esptool --chip esp32s3 image-info` on `nuttx.bin`.

Expected: ESP32-S3 image with valid checksum and size below `0x300000` (3,145,728 bytes), the LittleFS start.

- [ ] **Step 2: Flash and require write-hash verification**

Run:

```bash
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx -j8 flash \
  ESPTOOL_PORT=/dev/ttyACM0 ESPTOOL_BINDIR=./
```

Expected: esptool reports `Hash of data verified`. Do not interpret the known software-reset limitation as LCD evidence.

- [ ] **Step 3: Re-run non-hardware completion checks**

Run the eight host tests, `git diff --check`, and `skills/develop-agentguard-openvela/scripts/check_workspace.sh` with the contest and openvela root paths.

Expected: all tests pass, no whitespace errors appear, and all workspace checks print `OK`.

- [ ] **Step 4: Update and commit the handoff evidence**

Add a dated subsection near the top of `AGENTGUARD_PROGRESS.md` recording implementation commit hashes, exact commands/results, symbol and fixture addresses, image size/SHA-256, flash verification, physical LCD observation pending, and the `D:40`/`D:41`/`D:42 K:xy` interpretation table. Stage only the handoff file and commit as `docs: hand off TIE progress diagnostic`.

- [ ] **Step 5: Request the single physical observation**

Ask the user to report the visible `D:` value, the `K:` value if present, and the `CAM:` value if present. Do not claim a TIE fix or usable face detection until that evidence is received.
