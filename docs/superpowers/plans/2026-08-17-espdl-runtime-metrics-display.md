# ESP-DL Runtime Metrics Display Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show existing ESP-DL candidate count and inference milliseconds after the first valid live inference while preserving startup `D:42 K:11` beforehand.

**Architecture:** A pure UI helper selects the diagnostic footer text independently of pixel drawing. Model diagnostics take priority once valid; otherwise the existing TIE progress remains visible.

**Tech Stack:** C11, host Make tests with warnings as errors, NuttX cross-build, esptool.

## Global Constraints

- Do not alter the model, thresholds, backend, inference interval, camera path, buffers, LCD refresh interval, or TIE self-test.
- Keep `K:` bits in RAM-then-Flash order.
- Keep `D:42 K:xy` until live model diagnostics become valid.
- After model diagnostics become valid, render `M:<candidates> AI:<milliseconds> K:<ram><flash>`.
- Preserve unrelated workspace changes.

---

### Task 1: Testable Diagnostic Footer Selection

**Files:**
- Modify: `app/agentguard/include/agentguard/display_ui.h`
- Modify: `app/agentguard/src/display_ui.c`
- Test: `app/agentguard/tests/test_display_ui.c`

**Interfaces:**
- Produces: `bool ag_ui_format_diagnostic_detail(char *buffer, size_t buffer_size, const struct ag_ui_status *status)`.
- Consumes: existing `ag_ui_format_model_diagnostics()` and `ag_ui_format_tie_progress()`.

- [ ] **Step 1: Write failing precedence tests**

In `test_display_ui.c`, initialize `status` and assert:

```c
status.tie_selftest_stage = 42;
status.tie_selftest_valid = true;
status.tie_ram_pass = true;
status.tie_flash_pass = true;
status.model_diagnostics_valid = true;
status.msr_candidates = 3;
status.inference_ms = 847;
assert(ag_ui_format_diagnostic_detail(diagnostics, sizeof(diagnostics),
                                      &status));
assert(strcmp(diagnostics, "M:3 AI:847 K:11") == 0);

status.model_diagnostics_valid = false;
assert(ag_ui_format_diagnostic_detail(diagnostics, sizeof(diagnostics),
                                      &status));
assert(strcmp(diagnostics, "D:42 K:11") == 0);

memset(&status, 0, sizeof(status));
assert(!ag_ui_format_diagnostic_detail(diagnostics, sizeof(diagnostics),
                                       &status));
```

- [ ] **Step 2: Verify RED**

Run: `make -C app/agentguard/tests test_display_ui`

Expected: compilation fails under `-Werror` because `ag_ui_format_diagnostic_detail` is undeclared.

- [ ] **Step 3: Implement the minimal helper and renderer delegation**

Declare the helper in `display_ui.h`. It returns false for null/empty arguments, formats model diagnostics first when valid, otherwise formats TIE progress when stage/result exists, and returns false when neither exists.

Replace the renderer's current model/TIE conditional block with one helper call. Keep the existing seated-time/frame fallback when the helper returns false.

- [ ] **Step 4: Verify GREEN and full regression**

Run: `make -C app/agentguard/tests clean test`

Expected: all eight compiled host tests and the no-stdio contract pass.

- [ ] **Step 5: Commit**

Stage only `display_ui.h`, `display_ui.c`, and `test_display_ui.c`. Commit as `feat: show ESP-DL runtime metrics after startup` using explicit repo paths.

---

### Task 2: Build, Flash, and Measure

**Files:**
- Inspect: `/home/yhx/Desktop/openvela/nuttx/nuttx.bin`
- Hardware: `/dev/ttyACM0`

**Interfaces:**
- Consumes: Task 1 footer selection.
- Produces: a flashed measurement firmware and physical `M:`/`AI:` values.

- [ ] **Step 1: Cross-build**

Activate `/home/yhx/Desktop/openvela/myenv/bin/activate` and run `make -C /home/yhx/Desktop/openvela/nuttx -j8`.

Expected: successful `nuttx.bin` generation.

- [ ] **Step 2: Capture and validate the exact image**

Record size and SHA-256, then run `esptool --chip esp32s3 image-info`. Require a valid ESP32-S3 checksum and size below `0x300000`.

- [ ] **Step 3: Flash without rebuilding**

Directly write the captured `nuttx.bin` to offset zero on `/dev/ttyACM0` and require `Hash of data verified`. If the device node is absent, request one physical RESET and resume with the same captured image.

- [ ] **Step 4: Request the measurement**

After physical RESET, ask for the exact `M:`, `AI:`, and `K:` footer plus approximate camera image interval. Do not optimize until the measured `AI:` value confirms or rejects the inference-bottleneck hypothesis.
