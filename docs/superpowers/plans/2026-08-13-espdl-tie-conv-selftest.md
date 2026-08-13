# ESP-DL TIE Convolution Self-Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a one-shot target diagnostic that compares the same TIE int8 convolution using RAM and Flash/DROM weights against a deterministic scalar reference.

**Architecture:** Pure, host-testable logic owns the fixed scalar fixture, output comparison, result classification, and run-once guard. A target-only adapter owns aligned RAM/DROM buffers, `ArgsType<int8_t>` construction, compile-time ABI checks, the two direct assembly calls, and serial reporting; `vision_espdl.cpp` invokes it once immediately before detector construction.

**Tech Stack:** C++20, ESP-DL `ArgsType<int8_t>`, ESP32-S3 TIE728 assembly, NuttX application Makefiles, host Make tests, serial boot capture.

## Global Constraints

- Do not change face-detection thresholds, model behavior, or the camera/display pipeline.
- Run the diagnostic exactly once before model inference starts.
- Use static 16-byte-aligned storage; allocate no diagnostic buffers from the heap or stack.
- A value mismatch must be logged and must not prevent the normal application from continuing.
- Missing TIE symbols or unexpected `ArgsType<int8_t>` offsets must fail the target build.
- Do not implement a runtime fix until the self-test identifies the failing boundary.

## File Structure

- Create `app/agentguard/include/agentguard/espdl_tie_selftest.h`: C-compatible result enum and pure-logic function declarations plus the target entry point.
- Create `app/agentguard/src/espdl_tie_selftest_logic.cpp`: fixed scalar dot product, 16-lane comparison, classification, name mapping, and run-once state transition.
- Create `app/agentguard/src/espdl_tie_selftest.cpp`: target-only aligned fixtures, ABI assertions, direct TIE calls, and serial record.
- Create `app/agentguard/tests/test_espdl_tie_selftest.cpp`: host tests for the fixture, classification matrix, names, and run-once behavior.
- Modify `app/agentguard/tests/Makefile`: compile and execute the new host test.
- Modify `app/agentguard/Makefile`: link both new C++ sources in ESP-DL builds.
- Modify `app/agentguard/src/vision_espdl.cpp`: invoke the target entry point before constructing `HumanFaceDetect`.

---

### Task 1: Host-Tested Self-Test Logic

**Files:**
- Create: `app/agentguard/include/agentguard/espdl_tie_selftest.h`
- Create: `app/agentguard/src/espdl_tie_selftest_logic.cpp`
- Create: `app/agentguard/tests/test_espdl_tie_selftest.cpp`
- Modify: `app/agentguard/tests/Makefile`

**Interfaces:**
- Produces: `int8_t ag_tie_selftest_scalar_lane(const int8_t input[16], const int8_t filter[256], size_t lane)`.
- Produces: `bool ag_tie_selftest_all_equal(const int8_t output[16], int8_t expected)`.
- Produces: `enum ag_tie_selftest_classification ag_tie_selftest_classify(bool ram_pass, bool flash_pass)` with values `AG_TIE_RAM_PASS_FLASH_FAIL`, `AG_TIE_RAM_FAIL_FLASH_FAIL`, `AG_TIE_RAM_PASS_FLASH_PASS`, and `AG_TIE_RAM_FAIL_FLASH_PASS`.
- Produces: `const char *ag_tie_selftest_classification_name(enum ag_tie_selftest_classification value)`.
- Produces: `bool ag_tie_selftest_claim(bool *already_ran)`; the first call changes `false` to `true` and returns `true`, later calls return `false`.
- Produces: `void ag_espdl_tie_conv_selftest_run_once(void)` declaration for Task 2.

- [ ] **Step 1: Write and register the failing host test**

Create `test_espdl_tie_selftest.cpp` with 16 input values of `1`, 256 filter values of `1`, and assertions that all 16 scalar lanes equal `16`. Add independent assertions for the four classification combinations, exact classification strings, one mismatched output lane, and two consecutive `ag_tie_selftest_claim()` calls.

In the same step, add `test_espdl_tie_selftest` to `TARGETS`, compile it from
the test and the not-yet-created `src/espdl_tie_selftest_logic.cpp`, and add it
to the `test` recipe. This makes the missing implementation observable as the
required RED failure.

```cpp
int main()
{
  alignas(16) int8_t input[16];
  alignas(16) int8_t filter[256];
  alignas(16) int8_t output[16];
  std::fill_n(input, 16, int8_t{1});
  std::fill_n(filter, 256, int8_t{1});
  std::fill_n(output, 16, int8_t{16});

  for (size_t lane = 0; lane < 16; ++lane)
    assert(ag_tie_selftest_scalar_lane(input, filter, lane) == 16);
  assert(ag_tie_selftest_all_equal(output, 16));
  output[7] = 15;
  assert(!ag_tie_selftest_all_equal(output, 16));

  assert(ag_tie_selftest_classify(true, false) == AG_TIE_RAM_PASS_FLASH_FAIL);
  assert(ag_tie_selftest_classify(false, false) == AG_TIE_RAM_FAIL_FLASH_FAIL);
  assert(ag_tie_selftest_classify(true, true) == AG_TIE_RAM_PASS_FLASH_PASS);
  assert(ag_tie_selftest_classify(false, true) == AG_TIE_RAM_FAIL_FLASH_PASS);
  assert(std::strcmp(ag_tie_selftest_classification_name(
    AG_TIE_RAM_PASS_FLASH_FAIL), "RAM_PASS_FLASH_FAIL") == 0);

  bool already_ran = false;
  assert(ag_tie_selftest_claim(&already_ran));
  assert(already_ran);
  assert(!ag_tie_selftest_claim(&already_ran));
}
```

- [ ] **Step 2: Run the new test and verify RED**

Run: `make -C app/agentguard/tests test_espdl_tie_selftest`

Expected: build failure because `agentguard/espdl_tie_selftest.h` and the declared functions do not exist.

- [ ] **Step 3: Implement the minimal pure logic**

Use output-major filter indexing, `filter[lane * 16 + input_channel]`, with an `int32_t` accumulator and an explicit int8 saturation helper. `ag_tie_selftest_all_equal()` must inspect all 16 lanes. Implement classification with four explicit branches and return the exact names from the design. `ag_tie_selftest_claim(nullptr)` returns `false` without changing state.

- [ ] **Step 4: Verify the registered host test turns GREEN**

Run: `make -C app/agentguard/tests clean test`

Expected: all eight host test executables build and pass with exit status 0.

- [ ] **Step 5: Commit the pure logic**

```bash
git add app/agentguard/include/agentguard/espdl_tie_selftest.h \
  app/agentguard/src/espdl_tie_selftest_logic.cpp \
  app/agentguard/tests/test_espdl_tie_selftest.cpp \
  app/agentguard/tests/Makefile
git commit -m "test: define TIE convolution self-test"
```

---

### Task 2: Target TIE Adapter and Startup Integration

**Files:**
- Create: `app/agentguard/src/espdl_tie_selftest.cpp`
- Modify: `app/agentguard/Makefile`
- Modify: `app/agentguard/src/vision_espdl.cpp`
- Test: NuttX target link and `ArgsType<int8_t>` compile-time assertions

**Interfaces:**
- Consumes: every Task 1 interface.
- Consumes: `extern "C" void dl_tie728_s8_conv2d_11cn(int8_t *output, int8_t *input, void *args)` from the linked TIE assembly.
- Produces: `extern "C" void ag_espdl_tie_conv_selftest_run_once(void)`.

- [ ] **Step 1: Wire the target entry point before implementing it**

Add `src/espdl_tie_selftest_logic.cpp` and the not-yet-created
`src/espdl_tie_selftest.cpp` to `CXXSRCS` in `app/agentguard/Makefile`.
Include the public header in `vision_espdl.cpp` and call
`ag_espdl_tie_conv_selftest_run_once()` inside the existing
`g_detector == nullptr` block immediately before `new HumanFaceDetect(...)`.

- [ ] **Step 2: Run the source/configuration test and verify RED**

Run: `make -C /home/yhx/Desktop/openvela/nuttx -j8`

Expected: failure because `src/espdl_tie_selftest.cpp` does not exist, proving
the target build now requires the diagnostic implementation.

- [ ] **Step 3: Implement the target adapter**

Create static 16-byte-aligned writable arrays for input, RAM filter, and two outputs. Create a static `const`, 16-byte-aligned Flash filter initialized to 256 ones. At entry, claim a static `already_ran`; return immediately if it was already claimed. Fill writable fixtures with ones and outputs with a sentinel.

Construct a zero-initialized `dl::base::ArgsType<int8_t>` and set:

```cpp
args.input_element = kInput;
args.input_channel = 16;
args.output_element = output;
args.output_channel = 16;
args.filter_element = filter;
args.filter_height = 1;
args.filter_width = 1;
args.filter_y_offset = 16;
args.filter_n_offset = 16;
args.mac_shift = 0;
args.bias_element = nullptr;
args.activation_type = dl::Linear;
args.n_div_x = 1;
args.c_div_x_1 = 0;
```

Add `static_assert(offsetof(...))` checks for assembly offsets 0, 24, 48, 64, 68, 72, 96, 100, and 104. Run RAM first and Flash second, changing only `output_element` and `filter_element`. Compute expected lane zero through the Task 1 scalar function, compare all lanes, classify, and print exactly one record beginning with `agentguard: TIE_SELFTEST` containing the five addresses, expected value, both 16-byte result vectors, and classification name.

- [ ] **Step 4: Verify the target adapter satisfies the new wiring**

Run: `make -C /home/yhx/Desktop/openvela/nuttx -j8`

Expected: successful NuttX build; no missing TIE symbol and no failed ABI
assertion. This turns the target integration from RED to GREEN.

- [ ] **Step 5: Verify host tests and target compilation**

Run: `make -C app/agentguard/tests clean test`

Expected: all eight host tests pass.

Run: `make -C /home/yhx/Desktop/openvela/nuttx -j8`

Expected: successful NuttX build; no missing TIE symbol and no failed ABI assertion.

Run: `/home/yhx/Desktop/openvela/prebuilts/gcc/linux-x86_64/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-nm -C /home/yhx/Desktop/openvela/nuttx/nuttx | rg "ag_espdl_tie_conv_selftest_run_once|dl_tie728_s8_conv2d_11cn$"`

Expected: both symbols appear in the firmware.

- [ ] **Step 6: Commit target integration**

```bash
git add app/agentguard/src/espdl_tie_selftest.cpp \
  app/agentguard/src/vision_espdl.cpp app/agentguard/Makefile \
  app/agentguard/src/espdl_tie_selftest_logic.cpp
git commit -m "feat: diagnose TIE convolution memory access"
```

---

### Task 3: Board Evidence Capture

**Files:**
- Inspect: `/home/yhx/Desktop/openvela/nuttx/nuttx.bin`
- Inspect: serial output from `/dev/ttyACM0`

**Interfaces:**
- Consumes: the `agentguard: TIE_SELFTEST` record from Task 2.
- Produces: one evidence-backed classification that selects the next root-cause investigation; it does not modify runtime code.

- [ ] **Step 1: Record the exact firmware identity**

Run: `sha256sum /home/yhx/Desktop/openvela/nuttx/nuttx.bin`

Expected: one SHA-256 hash saved with the diagnostic transcript.

- [ ] **Step 2: Flash the firmware**

Run: `source /home/yhx/Desktop/openvela/myenv/bin/activate && make -C /home/yhx/Desktop/openvela/nuttx -j8 flash ESPTOOL_PORT=/dev/ttyACM0 ESPTOOL_BINDIR=./`

Expected: flash verification succeeds and the board reconnects as `/dev/ttyACM0`.

- [ ] **Step 3: Reset and capture startup output automatically**

Use the existing serial probe/reset tooling to capture at least 30 seconds from reset without asking the user to press a button. Search the transcript for `TIE_SELFTEST`, panic registers, reboot loops, and the normal AgentGuard startup line.

Expected: exactly one self-test record and continued startup into the normal application loop.

- [ ] **Step 4: Interpret without applying a fix**

Map the record to exactly one design classification:

- `RAM_PASS_FLASH_FAIL`: next test copies one real model filter to internal RAM while leaving all other arguments unchanged.
- `RAM_FAIL_FLASH_FAIL`: next test validates the direct-call ABI and instruction semantics against an ESP-IDF reference build.
- `RAM_PASS_FLASH_PASS`: next test compares C and TIE outputs at the first model convolution layer.
- `RAM_FAIL_FLASH_PASS`: preserve raw addresses/results and investigate internal-memory region requirements before another modification.

Run: `git status --short` and `git diff --check`.

Expected: no whitespace errors; unrelated pre-existing workspace changes remain untouched.
