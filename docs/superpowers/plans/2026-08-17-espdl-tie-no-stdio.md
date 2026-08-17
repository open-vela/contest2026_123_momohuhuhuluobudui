# ESP-DL TIE Startup No-stdio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove blocking standard-I/O from the completed TIE startup self-test so AgentGuard can proceed from `D:42 K:11 CAM:0` into camera initialization.

**Architecture:** A host source-contract test permanently prohibits stdio dependencies in the target self-test translation unit. The self-test retains its direct TIE calls and atomic LCD publication, then returns immediately after publishing stage 42.

**Tech Stack:** Python 3 source-contract test, C++20 atomics, NuttX/ESP-DL cross-build, Xtensa objdump, esptool.

## Global Constraints

- Preserve direct TIE call order, arguments, fixtures, ABI checks, and atomic acquire/release ordering.
- Preserve `D:40`, `D:41`, and `D:42 K:xy` LCD behavior.
- Do not add deferred logging, timeouts, cancellation, new threads, or alternate output sinks.
- Do not change the model, thresholds, inference cadence, camera configuration, or TIE assembly.
- Preserve unrelated workspace changes.

---

### Task 1: Prohibit Startup Self-Test stdio

**Files:**
- Create: `app/agentguard/tests/test_espdl_tie_no_stdio.py`
- Modify: `app/agentguard/tests/Makefile`
- Modify: `app/agentguard/src/espdl_tie_selftest.cpp`

**Interfaces:**
- Produces: a host regression contract that rejects stdio in `espdl_tie_selftest.cpp`.
- Preserves: `ag_espdl_tie_conv_selftest_run_once()`, `get_stage()`, and `get_result()` public behavior.

- [ ] **Step 1: Write the failing source-contract test**

Create a Python test that reads `src/espdl_tie_selftest.cpp` and asserts that none of these tokens occur: `<cstdio>`, `stderr`, `stdout`, `fprintf`, `printf`, `fputc`, `fwrite`, or `print_vector`. Print `AgentGuard TIE no-stdio contract: PASS` on success.

- [ ] **Step 2: Run the contract and verify RED**

Run: `python3 app/agentguard/tests/test_espdl_tie_no_stdio.py`

Expected: failure identifying `<cstdio>` and the current printing code.

- [ ] **Step 3: Remove only the blocking output path**

Delete `<cstdio>`, `print_vector()`, the classification local used only by the log, and every final `std::fprintf` call. Leave result calculation and these final operations adjacent:

```cpp
g_result.store(result, std::memory_order_release);
g_stage.store(AG_TIE_SELFTEST_STAGE_COMPLETE,
              std::memory_order_release);
```

The function must return immediately afterward.

- [ ] **Step 4: Register the contract and verify GREEN**

Add `python3 test_espdl_tie_no_stdio.py` to the host `test` recipe after the eight compiled tests. Run:

```bash
python3 app/agentguard/tests/test_espdl_tie_no_stdio.py
make -C app/agentguard/tests clean test
```

Expected: the contract and all eight existing test executables pass.

- [ ] **Step 5: Cross-build and inspect the exact target function**

Run the NuttX `make -j8` build in the activated openvela virtual environment. Disassemble from `<ag_espdl_tie_conv_selftest_run_once>` to `<ag_espdl_tie_conv_selftest_get_stage>` and assert that the range contains none of `lib_get_stream`, `fprintf`, `fputc`, or `fwrite`. Confirm RAM and Flash filters remain in `0x3fc...` and `0x3c...`.

- [ ] **Step 6: Commit the tested fix**

Stage only the new contract test, test Makefile, and self-test source. Commit as `fix: remove blocking TIE startup logging` using the explicit repo `--git-dir` and `--work-tree` paths.

---

### Task 2: Flash and Record Physical Follow-up

**Files:**
- Modify: `AGENTGUARD_PROGRESS.md`
- Hardware: `/dev/ttyACM0`

**Interfaces:**
- Consumes: the verified no-stdio target image.
- Produces: flashed firmware and a handoff awaiting `CAM:` progression.

- [ ] **Step 1: Capture image evidence**

Record `nuttx.bin` size, SHA-256, and `esptool image-info`. Require an ESP32-S3 image with valid checksum below the `0x300000` LittleFS boundary.

- [ ] **Step 2: Flash without rebuilding the captured image**

Write the captured `nuttx.bin` directly to offset zero with esptool and require `Hash of data verified`. If `/dev/ttyACM0` is absent after the prior run, request one physical RESET and resume this step without changing the image.

- [ ] **Step 3: Run final software verification**

Freshly run the full host suite, NuttX target build, `git diff --check`, and the project workspace checker. Because rebuilding changes embedded build metadata, record the post-verification image hash separately and do not confuse it with the captured flashed hash.

- [ ] **Step 4: Update and commit the handoff**

Record the physical `D:42 K:11 CAM:0` evidence, root cause, RED/GREEN evidence, target disassembly result, image hashes, flash result, and pending expectation that `CAM:` advances beyond zero. Commit only `AGENTGUARD_PROGRESS.md` as `docs: hand off no-stdio TIE startup`.

- [ ] **Step 5: Request the physical result**

Ask for `D:`, `K:`, and `CAM:` after reset. Do not claim the full-model TIE issue or face detection is fixed.
