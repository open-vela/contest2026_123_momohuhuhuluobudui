# ESP-DL Live Face Stage A Diagnostics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a read-only, same-inference diagnostic that identifies the first boundary where live RGB565 face data stops changing correctly between V4L2, MSR preprocessing, MSR outputs, MNP crops, and final ESP-DL output.

**Architecture:** A small C fingerprint library produces deterministic hashes and ranges for RGB565 and signed tensors. The AgentGuard ESP-DL fork captures those fingerprints into a bounded trace, and `vision_espdl.cpp` publishes one mutex-protected snapshot per full inference for the LCD and `agentguard face-diag`; cached frames never publish a new generation. Stage A fixes caps at the already-approved official P3 mode but does not change thresholds, model backends, fallback authority, presence hold, camera, or LCD transport.

**Tech Stack:** C11, C++20, pthread mutexes, ESP-DL v3.2.0 `TensorBase`, NuttX V4L2/NSH, AgentGuard host Make tests, Xtensa NuttX cross-build.

**Spec:** `docs/superpowers/specs/2026-08-21-espdl-live-face-authority-restoration-design.md`

## Global Constraints

- This plan implements Stage A only. Stop after identifying the first divergent boundary; do not implement a behavioral fix.
- Fix live preprocessing to `RGB_SWAP | RGB565_BIG_ENDIAN` (former P3) and do not advance P0-P3 during Stage A.
- Keep MSR and MNP score thresholds at exactly `0.50`.
- Keep portable-C ordinary and depthwise convolution and all other current backend selections unchanged.
- Keep the inference interval at one full inference per three captured frames.
- Do not change camera registers, V4L2 buffer ownership, cache operations, LCD timing, DMA, rotation, preview geometry, face fallback authority, or the 1.5-second presence hold.
- Do not modify or push openvela official source. Application and vendored AgentGuard ESP-DL files are in scope.
- Do not print or write diagnostics in every inference. Publish an in-memory snapshot; output it only on explicit command.
- Preserve all unrelated tracked and untracked workspace changes. Stage exact task files only.
- For every Git operation use `--git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui`; do not rely on the held workspace `.git` entry.
- Record each flashed image hash and physical result in `AGENTGUARD_PROGRESS.md`, keeping that handoff edit separate from code commits.

## File Structure

- `app/agentguard/include/agentguard/diagnostic_fingerprint.h`: public deterministic byte/tensor/RGB565 fingerprint types and functions.
- `app/agentguard/src/diagnostic_fingerprint.c`: FNV-1a hashing, signed range collection, and three-region RGB565 fingerprints.
- `app/agentguard/tests/test_diagnostic_fingerprint.c`: literal hash/range boundary tests.
- `app/agentguard/include/agentguard/face_diagnostics.h`: bounded detector trace, same-inference snapshot, store, change, and formatting interfaces.
- `app/agentguard/src/face_diagnostics.c`: mutex-protected publication, bounded MNP recording, change comparison, and command formatting.
- `app/agentguard/tests/test_face_diagnostics.c`: real store, trace bound, change, and formatting behavior tests.
- `app/agentguard/third_party/human_face_detect/human_face_detect.hpp`: trace members and read-only accessor declarations.
- `app/agentguard/third_party/human_face_detect/human_face_detect.cpp`: fingerprint MSR/MNP tensors at the valid lifetime boundary.
- `app/agentguard/include/agentguard/vision_model.h`: read-only Stage A snapshot query.
- `app/agentguard/src/vision_espdl.cpp`: reference/live trace assembly, fixed official caps, inference generation, and snapshot publication.
- `app/agentguard/include/agentguard/display_ui.h`: compact Stage A UI fields and formatter declaration.
- `app/agentguard/src/display_ui.c`: compact diagnostic footer formatting and priority.
- `app/agentguard/src/agentguard_main.c`: copy the latest snapshot to UI and expose `agentguard face-diag`.
- `app/agentguard/tests/test_display_ui.c`: exact Stage A footer contract.
- `app/agentguard/Makefile`, `app/agentguard/tests/Makefile`: compile the new C modules and tests.

---

### Task 1: Deterministic Fingerprint Library

**Files:**
- Create: `app/agentguard/include/agentguard/diagnostic_fingerprint.h`
- Create: `app/agentguard/src/diagnostic_fingerprint.c`
- Create: `app/agentguard/tests/test_diagnostic_fingerprint.c`
- Modify: `app/agentguard/Makefile`
- Modify: `app/agentguard/tests/Makefile`

**Interfaces:**
- Consumes: caller-owned RGB565, `int8_t`, or `int16_t` data that remains valid for the duration of the call.
- Produces: `bool ag_fingerprint_i8(const int8_t *, size_t, struct ag_data_fingerprint *)`, `bool ag_fingerprint_i16(const int16_t *, size_t, struct ag_data_fingerprint *)`, and `bool ag_fingerprint_rgb565(const uint16_t *, uint16_t, uint16_t, struct ag_rgb565_fingerprint *)`.

- [ ] **Step 1: Write the failing fingerprint test and add its Make target**

Use literal expectations independent of production helpers:

```c
int8_t i8_values[4] = {0, 1, 2, -1};
int16_t i16_values[2] = {-2, 258};
uint16_t rgb[6] = {1, 2, 3, 4, 5, 6};
struct ag_data_fingerprint data;
struct ag_rgb565_fingerprint image;

assert(ag_fingerprint_i8(i8_values, 4, &data));
assert(data.valid && data.elements == 4);
assert(data.minimum == -1 && data.maximum == 2);
assert(data.hash == UINT32_C(0x6fab6075));

assert(ag_fingerprint_i16(i16_values, 2, &data));
assert(data.minimum == -2 && data.maximum == 258);
assert(data.hash == UINT32_C(0xdefc708b));

assert(ag_fingerprint_rgb565(rgb, 2, 3, &image));
assert(image.full_hash == UINT32_C(0x1585f8f2));
assert(image.top_hash == UINT32_C(0x476eaad6));
assert(image.middle_hash == UINT32_C(0x03b84a82));
assert(image.bottom_hash == UINT32_C(0x9ec79836));
assert(image.minimum == 1 && image.maximum == 6);
assert(image.nonzero_pixels == 6);

assert(!ag_fingerprint_i8(NULL, 4, &data));
assert(!ag_fingerprint_i8(i8_values, 0, &data));
assert(!ag_fingerprint_rgb565(rgb, 0, 3, &image));
```

Add `test_diagnostic_fingerprint` to `TARGETS` and the `test` recipe.

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```bash
make -C app/agentguard/tests test_diagnostic_fingerprint
```

Expected: FAIL because `agentguard/diagnostic_fingerprint.h` and its production implementation do not exist.

- [ ] **Step 3: Implement the minimal public types and functions**

The header must define:

```c
enum ag_fingerprint_type
{
  AG_FINGERPRINT_I8 = 1,
  AG_FINGERPRINT_I16 = 2
};

struct ag_data_fingerprint
{
  uint32_t hash;
  uint32_t elements;
  int16_t minimum;
  int16_t maximum;
  uint8_t type;
  bool valid;
};

struct ag_rgb565_fingerprint
{
  uint32_t full_hash;
  uint32_t top_hash;
  uint32_t middle_hash;
  uint32_t bottom_hash;
  uint32_t nonzero_pixels;
  uint16_t minimum;
  uint16_t maximum;
  bool valid;
};
```

Implement FNV-1a with offset `2166136261u` and prime `16777619u`. Hash each
numeric value in explicit low-byte/high-byte order; never hash a C struct or
native `int16_t` storage directly. Split RGB565 rows at `height / 3` and
`(height * 2) / 3`; hash every pixel in exactly one region. Reject null data,
null output, zero elements, zero dimensions, and `width * height` overflow;
zero the output before returning false.

Add `src/diagnostic_fingerprint.c` to `CSRCS`.

- [ ] **Step 4: Run focused and full host tests and verify GREEN**

Run:

```bash
make -C app/agentguard/tests test_diagnostic_fingerprint
app/agentguard/tests/test_diagnostic_fingerprint
make -C app/agentguard/tests test
```

Expected: `AgentGuard diagnostic fingerprint tests: PASS`, followed by all existing tests passing.

- [ ] **Step 5: Commit Task 1 only**

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui add app/agentguard/Makefile \
  app/agentguard/include/agentguard/diagnostic_fingerprint.h \
  app/agentguard/src/diagnostic_fingerprint.c \
  app/agentguard/tests/Makefile \
  app/agentguard/tests/test_diagnostic_fingerprint.c
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui diff --cached --check
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui commit -m "test: add deterministic face diagnostic fingerprints"
```

---

### Task 2: Bounded Trace and Atomic Snapshot Store

**Files:**
- Create: `app/agentguard/include/agentguard/face_diagnostics.h`
- Create: `app/agentguard/src/face_diagnostics.c`
- Create: `app/agentguard/tests/test_face_diagnostics.c`
- Modify: `app/agentguard/Makefile`
- Modify: `app/agentguard/tests/Makefile`

**Interfaces:**
- Consumes: fingerprint values from Task 1 and caller-owned snapshots.
- Produces: `struct ag_face_detector_trace`, `struct ag_face_diag_snapshot`, `AG_FACE_DIAG_STORE_INITIALIZER`, `ag_face_diag_publish`, `ag_face_diag_get`, `ag_face_diag_clear`, `ag_face_diag_mark_changes`, `ag_face_diag_record_mnp`, and `ag_face_diag_format`.

- [ ] **Step 1: Write a failing test for bounded trace, generation-safe storage, changes, and formatting**

Define four as the explicit diagnostic limit:

```c
#define AG_FACE_DIAG_MAX_MNP 4
```

Test these behaviors with real structures and a real pthread mutex:

```c
struct ag_face_diag_store store = AG_FACE_DIAG_STORE_INITIALIZER;
struct ag_face_diag_snapshot previous = {0};
struct ag_face_diag_snapshot current = {0};
struct ag_face_diag_snapshot copy = {0};
struct ag_mnp_diagnostic mnp = {0};
char text[768];
unsigned int i;

assert(!ag_face_diag_get(&store, &copy));
current.valid = true;
current.inference_sequence = 12;
current.raw.valid = true;
current.raw.full_hash = UINT32_C(0x1585f8f2);
current.live.valid = true;
current.live.msr_candidates = 1;
current.live.mnp_accepted = 0;
current.live.final_faces = 0;

for (i = 0; i < 5; i++)
  {
    mnp.crop.x = (uint16_t)(10 + i);
    assert(ag_face_diag_record_mnp(&current.live, &mnp) == (i < 4));
  }
assert(current.live.mnp_attempts == 5);
assert(current.live.mnp_recorded == 4);

ag_face_diag_mark_changes(NULL, &current);
assert(!current.raw_change_valid && !current.input_change_valid);
previous = current;
current.raw.full_hash++;
current.live.msr_input.hash = UINT32_C(0x22222222);
previous.live.msr_input.hash = UINT32_C(0x11111111);
previous.live.msr_input.valid = true;
current.live.msr_input.valid = true;
ag_face_diag_mark_changes(&previous, &current);
assert(current.raw_change_valid && current.raw_changed);
assert(current.input_change_valid && current.input_changed);

ag_face_diag_publish(&store, &current);
assert(ag_face_diag_get(&store, &copy));
assert(copy.inference_sequence == 12);
current.inference_sequence = 99;
assert(copy.inference_sequence == 12);
ag_face_diag_clear(&store);
assert(!ag_face_diag_get(&store, &copy));

assert(ag_face_diag_format(text, sizeof(text), &copy));
assert(strstr(text, "SEQ=12") != NULL);
assert(strstr(text, "RAW=1585f8f3") != NULL);
assert(strstr(text, "M=1 A=5 R=4 F=0") != NULL);
```

Also verify null stores/snapshots and undersized format buffers fail without modifying caller memory outside the provided size.

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```bash
make -C app/agentguard/tests test_face_diagnostics
```

Expected: FAIL because the new header and module are absent.

- [ ] **Step 3: Implement the trace and snapshot types**

Use these exact top-level shapes:

```c
struct ag_mnp_diagnostic
{
  struct ag_face_box crop;
  struct ag_data_fingerprint input;
  struct ag_data_fingerprint score;
  struct ag_data_fingerprint box;
  struct ag_data_fingerprint landmark;
  uint8_t score_percent;
  bool valid;
};

struct ag_face_detector_trace
{
  struct ag_data_fingerprint msr_input;
  struct ag_data_fingerprint msr_score0;
  struct ag_data_fingerprint msr_box0;
  struct ag_data_fingerprint msr_score1;
  struct ag_data_fingerprint msr_box1;
  struct ag_face_box largest_msr;
  struct ag_mnp_diagnostic mnp[AG_FACE_DIAG_MAX_MNP];
  uint8_t msr_candidates;
  uint8_t mnp_attempts;
  uint8_t mnp_recorded;
  uint8_t mnp_accepted;
  uint8_t final_faces;
  bool valid;
};

struct ag_face_diag_snapshot
{
  uint32_t inference_sequence;
  struct ag_rgb565_fingerprint reference_raw;
  struct ag_rgb565_fingerprint raw;
  struct ag_face_detector_trace reference;
  struct ag_face_detector_trace live;
  bool raw_change_valid;
  bool raw_changed;
  bool input_change_valid;
  bool input_changed;
  bool valid;
};

struct ag_face_diag_store
{
  pthread_mutex_t lock;
  struct ag_face_diag_snapshot snapshot;
};

#define AG_FACE_DIAG_STORE_INITIALIZER \
  { PTHREAD_MUTEX_INITIALIZER, {0} }
```

`ag_face_diag_record_mnp` increments `mnp_attempts` up to `UINT8_MAX` for every
attempt and copies only the first four entries. `ag_face_diag_mark_changes`
sets each change-valid bit only when both generations have a valid matching
fingerprint. Publication and retrieval copy the complete snapshot while the
mutex is held. Formatting emits fixed-order ASCII lines for raw, reference,
live, and recorded MNP entries; it performs no allocation.

Add `src/face_diagnostics.c` to `CSRCS`; link the focused host test with
`-pthread`.

- [ ] **Step 4: Run focused and full host tests and verify GREEN**

```bash
make -C app/agentguard/tests test_face_diagnostics
app/agentguard/tests/test_face_diagnostics
make -C app/agentguard/tests test
```

Expected: `AgentGuard face diagnostics tests: PASS`, then all tests pass.

- [ ] **Step 5: Commit Task 2 only**

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui add app/agentguard/Makefile \
  app/agentguard/include/agentguard/face_diagnostics.h \
  app/agentguard/src/face_diagnostics.c \
  app/agentguard/tests/Makefile \
  app/agentguard/tests/test_face_diagnostics.c
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui diff --cached --check
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui commit -m "feat: add bounded face diagnostic snapshots"
```

---

### Task 3: Capture MSR and MNP Tensors at Their Valid Lifetime

**Files:**
- Modify: `app/agentguard/third_party/human_face_detect/human_face_detect.hpp`
- Modify: `app/agentguard/third_party/human_face_detect/human_face_detect.cpp`
- Modify: `app/agentguard/tests/test_face_diagnostics.c`

**Interfaces:**
- Consumes: `ag_fingerprint_i8`, `ag_fingerprint_i16`, `ag_face_diag_record_mnp`, and ESP-DL `TensorBase` objects.
- Produces: `void HumanFaceDetect::get_last_trace(struct ag_face_detector_trace *) const`; one complete `ag_face_detector_trace` per `run()`.

- [ ] **Step 1: Extend the existing failing test with tensor-type dispatch behavior**

Add a tested adapter to `face_diagnostics.h`:

```c
bool ag_face_diag_fingerprint_tensor(const void *data, uint32_t elements,
                                     uint8_t element_bytes,
                                     bool element_signed,
                                     struct ag_data_fingerprint *output);
```

Test exact accepted mappings and rejection:

```c
int8_t i8[4] = {0, 1, 2, -1};
int16_t i16[2] = {-2, 258};

assert(ag_face_diag_fingerprint_tensor(i8, 4, 1, true, &data));
assert(data.type == AG_FINGERPRINT_I8);
assert(data.hash == UINT32_C(0x6fab6075));
assert(ag_face_diag_fingerprint_tensor(i16, 2, 2, true, &data));
assert(data.type == AG_FINGERPRINT_I16);
assert(data.hash == UINT32_C(0xdefc708b));
assert(!ag_face_diag_fingerprint_tensor(i8, 4, 4, true, &data));
assert(!ag_face_diag_fingerprint_tensor(i8, 4, 1, false, &data));
```

- [ ] **Step 2: Run the focused test and verify RED**

```bash
make -C app/agentguard/tests test_face_diagnostics
app/agentguard/tests/test_face_diagnostics
```

Expected: link failure for missing `ag_face_diag_fingerprint_tensor`.

- [ ] **Step 3: Implement the adapter and ESP-DL trace capture**

Implement the adapter as a strict dispatch to the Task 1 functions. In
`human_face_detect.cpp`, add a local `fingerprint_tensor` that maps only
`dl::DATA_TYPE_INT8` and `dl::DATA_TYPE_INT16`:

```cpp
static bool fingerprint_tensor(dl::TensorBase *tensor,
                               ag_data_fingerprint *output)
{
    if (tensor == nullptr || output == nullptr || tensor->get_size() <= 0) {
        return false;
    }

    if (tensor->dtype == dl::DATA_TYPE_INT8) {
        return ag_face_diag_fingerprint_tensor(
            tensor->get_element_ptr<int8_t>(), tensor->get_size(),
            sizeof(int8_t), true, output);
    }

    if (tensor->dtype == dl::DATA_TYPE_INT16) {
        return ag_face_diag_fingerprint_tensor(
            tensor->get_element_ptr<int16_t>(), tensor->get_size(),
            sizeof(int16_t), true, output);
    }

    std::memset(output, 0, sizeof(*output));
    return false;
}
```

Capture MSR input immediately after `preprocess()` and before `Model::run()`.
Capture `score0`, `box0`, `score1`, and `box1` immediately after
`Model::run()` and before postprocessing. Reset the trace at the start of each
`MSRMNP::run()`; after MSR postprocessing, record candidate count and the
largest valid candidate box.

For each MNP candidate, save the squared/clipped crop before preprocessing,
then fingerprint model input after preprocessing and `score`, `box`, and
`landmark` after model execution. Call `ag_face_diag_record_mnp` once per
candidate. Derive `score_percent` with the existing maximum tensor value,
tensor exponent dequantization, and sigmoid calculation, clamped to 99; do not
introduce a second probability formula. After NMS, set `mnp_accepted` and `final_faces` from the final list,
clamped to `UINT8_MAX`, then set `trace.valid = true`.

Declare and implement these forwarding accessors:

```cpp
void MSRMNP::get_last_trace(ag_face_detector_trace *trace) const;
void HumanFaceDetect::get_last_trace(ag_face_detector_trace *trace) const;
```

Null output pointers are no-ops. The trace contains copied scalar data only;
no `TensorBase` or candidate-list pointer escapes `run()`.

- [ ] **Step 4: Verify host tests and the target integration build**

```bash
make -C app/agentguard/tests test
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx -j8
```

Expected: all host tests pass; target output compiles
`human_face_detect.cpp` and links `nuttx` without unsupported tensor-type
warnings or undefined diagnostic symbols.

- [ ] **Step 5: Commit Task 3 only**

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui add app/agentguard/include/agentguard/face_diagnostics.h \
  app/agentguard/src/face_diagnostics.c \
  app/agentguard/tests/test_face_diagnostics.c \
  app/agentguard/third_party/human_face_detect/human_face_detect.hpp \
  app/agentguard/third_party/human_face_detect/human_face_detect.cpp
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui diff --cached --check
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui commit -m "diag: capture ESP-DL face stage fingerprints"
```

---

### Task 4: Publish Same-Inference Reference and Live Snapshots

**Files:**
- Modify: `app/agentguard/include/agentguard/vision_model.h`
- Modify: `app/agentguard/src/vision_espdl.cpp`
- Modify: `app/agentguard/src/agentguard_main.c`
- Modify: `app/agentguard/tests/test_face_diagnostics.c`

**Interfaces:**
- Consumes: `HumanFaceDetect::get_last_trace`, RGB565 fingerprinting, snapshot store, and formatter.
- Produces: `bool ag_vision_model_get_face_diagnostics(struct ag_face_diag_snapshot *)` and the `agentguard face-diag` command.

- [ ] **Step 1: Add a failing test for first-generation and changed-generation assembly**

Add this pure helper to the desired interface:

```c
void ag_face_diag_prepare_snapshot(
  const struct ag_face_diag_snapshot *previous,
  uint32_t sequence,
  const struct ag_rgb565_fingerprint *reference_raw,
  const struct ag_rgb565_fingerprint *raw,
  const struct ag_face_detector_trace *reference,
  const struct ag_face_detector_trace *live,
  struct ag_face_diag_snapshot *output);
```

Test that generation one has invalid change flags, generation two compares
only full raw and MSR-input hashes, and copied reference/live traces retain
their exact values. Pass a null required input and assert the output is zeroed
and invalid.

- [ ] **Step 2: Run the focused test and verify RED**

```bash
make -C app/agentguard/tests test_face_diagnostics
app/agentguard/tests/test_face_diagnostics
```

Expected: link failure for missing `ag_face_diag_prepare_snapshot`.

- [ ] **Step 3: Implement snapshot assembly and wire full inferences only**

Implement the pure helper using structure copies followed by
`ag_face_diag_mark_changes`.

In `vision_espdl.cpp`, add:

```cpp
ag_face_diag_store g_face_diag_store = AG_FACE_DIAG_STORE_INITIALIZER;
ag_rgb565_fingerprint g_reference_raw = {};
ag_face_detector_trace g_reference_trace = {};
ag_face_diag_snapshot g_previous_snapshot = {};
uint32_t g_face_diag_sequence = 0;
```

Fingerprint the embedded reference image with `ag_fingerprint_rgb565` before
running it. After the embedded reference image finishes, call
`g_detector->get_last_trace(&g_reference_trace)`. On a cached frame, return the
cached face result exactly as before and do not fingerprint, increment, or
publish.

For a full live inference:

1. Fingerprint the raw `pixels` before `g_detector->run()`.
2. Fix caps with `g_detector->set_image_caps(kCameraImageCaps)`; remove the
   runtime call to `ag_espdl_pixel_mode()` and do not advance
   `g_live_inference_counter`. Remove that now-unused counter and set
   `g_diagnostics.pixel_mode_scores[0] = 3` so legacy diagnostics truthfully
   identify the fixed official mode.
3. Run the detector and copy `g_detector->get_last_trace(&live_trace)`.
4. Increment the diagnostic sequence once, assemble the snapshot, publish it,
   then save it as `g_previous_snapshot`.
5. Keep `g_cached_result`, thresholds, inference timing, and existing model
   diagnostics behavior unchanged.

Expose the read-only C API:

```c
bool ag_vision_model_get_face_diagnostics(
  struct ag_face_diag_snapshot *diagnostics);
void ag_vision_model_reset_face_diagnostics(void);
```

It delegates to `ag_face_diag_get`; a null pointer or no completed live
inference returns false. The reset function clears the published store,
previous-generation comparison, and change-valid state without destroying the
detector or reference trace. Add and test `ag_face_diag_clear(store)` as the
store-level primitive. Call the model reset immediately before every existing
`ag_video_restart()` attempt (frame timeout, QBUF failure, and any other
restart call) so hashes are never compared across streams even when a restart
attempt itself fails.

- [ ] **Step 4: Add the explicit NSH command without changing normal startup**

Before the default `ag_run()` call in `main()` add:

```c
if (argc == 2 && strcmp(argv[1], "face-diag") == 0)
  {
    struct ag_face_diag_snapshot diagnostics;
    char output[768];

    if (!ag_vision_model_get_face_diagnostics(&diagnostics) ||
        !ag_face_diag_format(output, sizeof(output), &diagnostics))
      {
        puts("face_diag=unavailable");
        return EXIT_FAILURE;
      }

    fputs(output, stdout);
    return EXIT_SUCCESS;
  }
```

Guard the query with `CONFIG_AGENTGUARD_ESP_DL`; when ESP-DL is disabled, the
same command prints `face_diag=unavailable` and fails. It must not initialize a
detector, start a camera, run inference, or write LittleFS.

- [ ] **Step 5: Verify focused tests, full tests, and fixed-caps source behavior through execution**

```bash
make -C app/agentguard/tests test_face_diagnostics
app/agentguard/tests/test_face_diagnostics
make -C app/agentguard/tests test
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx -j8
```

Expected: all tests pass and target link succeeds. Do not add a grep-based
test for P3; the physical snapshot's stable preprocessing comparison and the
existing `test_espdl_pixel_mode` separation provide the behavioral boundary.

- [ ] **Step 6: Commit Task 4 only**

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui add app/agentguard/include/agentguard/face_diagnostics.h \
  app/agentguard/include/agentguard/vision_model.h \
  app/agentguard/src/face_diagnostics.c \
  app/agentguard/src/vision_espdl.cpp \
  app/agentguard/src/agentguard_main.c \
  app/agentguard/tests/test_face_diagnostics.c
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui diff --cached --check
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui commit -m "diag: publish same-inference face snapshots"
```

---

### Task 5: Compact LCD Stage A Status

**Files:**
- Modify: `app/agentguard/include/agentguard/display_ui.h`
- Modify: `app/agentguard/src/display_ui.c`
- Modify: `app/agentguard/src/agentguard_main.c`
- Modify: `app/agentguard/tests/test_display_ui.c`

**Interfaces:**
- Consumes: the most recent valid `ag_face_diag_snapshot` copied in the camera/main thread.
- Produces: `void ag_ui_format_face_diagnostics(char *, size_t, uint32_t, bool, bool, bool, bool, uint8_t, uint8_t, uint8_t)` and a highest-priority footer containing generation, raw-change state, input-change state, MNP attempted/accepted counts, and raw ESP-DL faces in the exact format tested below.

- [ ] **Step 1: Write the failing UI tests**

Add fields to the desired `ag_ui_status` contract for diagnostic generation,
change validity/value, MNP attempts/accepted, raw ESP-DL faces, and a valid
bit. Test direct formatting:

```c
ag_ui_format_face_diagnostics(diagnostics, sizeof(diagnostics),
                              12, true, true, true, false,
                              1, 0, 0);
assert(strcmp(diagnostics, "G:12 R:1 I:0 M:1/0 F:0") == 0);

ag_ui_format_face_diagnostics(diagnostics, sizeof(diagnostics),
                              12345, false, false, false, false,
                              255, 255, 12);
assert(strcmp(diagnostics, "G:999+ R:- I:- M:99+/99+ F:9+") == 0);
```

Set `status.face_diagnostics_valid = true` alongside all existing timing
flags and assert `ag_ui_format_diagnostic_detail` selects the new `G:` form.
Clear the valid bit and assert the existing P/S/M/F/AI selection remains
unchanged.

- [ ] **Step 2: Run the UI test and verify RED**

```bash
make -C app/agentguard/tests test_display_ui
app/agentguard/tests/test_display_ui
```

Expected: compile failure for the absent formatter and status fields.

- [ ] **Step 3: Implement the bounded formatter and selection priority**

Add the fields to `ag_ui_status` and format exactly as tested. Clamp sequence
above 999 to `999+`, counts above 99 to `99+`, and face count above 9 to `9+`.
Use `-` only when the corresponding change-valid flag is false. In
`ag_ui_format_diagnostic_detail`, select Stage A diagnostics before LCD submit,
frame timing, model timing, and TIE details.

In `ag_run()`, after the full model call and before display publication, query
the snapshot. If valid, copy only scalar summary fields into `ui_status`; do
not copy the 768-byte command snapshot into the display-worker status.
`F` must come from `snapshot.live.final_faces`, not the current fallback-
modified `vision_result.face_count`.

- [ ] **Step 4: Run focused and full tests and verify GREEN**

```bash
make -C app/agentguard/tests test_display_ui
app/agentguard/tests/test_display_ui
make -C app/agentguard/tests test
```

Expected: exact formatter assertions and all existing UI/render tests pass.

- [ ] **Step 5: Commit Task 5 only**

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui add app/agentguard/include/agentguard/display_ui.h \
  app/agentguard/src/display_ui.c \
  app/agentguard/src/agentguard_main.c \
  app/agentguard/tests/test_display_ui.c
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui diff --cached --check
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui commit -m "diag: show live ESP-DL stage changes"
```

---

### Task 6: Full Verification, Flash, and Stage A Evidence Collection

**Files:**
- Modify, but keep separate from code commits: `AGENTGUARD_PROGRESS.md`

**Interfaces:**
- Consumes: completed Stage A code and connected ESP32-S3-EYE at `/dev/ttyACM0`.
- Produces: a verified flashed image and a four-scene evidence matrix identifying the first divergent boundary; no behavioral fix.

- [ ] **Step 1: Run fresh repository verification**

```bash
make -C app/agentguard/tests test
skills/develop-agentguard-openvela/scripts/check_workspace.sh \
  /home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  /home/yhx/Desktop/openvela
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui diff --check
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui status --short
```

Expected: every AgentGuard test passes, all six workspace checks are `OK`,
`git diff --check` is silent, and status contains only preserved user files,
build artifacts, and the separate handoff edit.

- [ ] **Step 2: Build and inspect the target artifact**

```bash
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx -j8
stat -c '%s bytes' /home/yhx/Desktop/openvela/nuttx/nuttx.bin
sha256sum /home/yhx/Desktop/openvela/nuttx/nuttx.bin
```

Expected: target output explicitly compiles the two new C modules and modified
face detector, links `nuttx`, and generates `nuttx.bin`. Record the actual size
and pre-flash SHA-256.

- [ ] **Step 3: Flash only after verifying `/dev/ttyACM0` is the Espressif board**

```bash
lsusb
ls -l /dev/ttyACM0
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx -j8 flash \
  ESPTOOL_PORT=/dev/ttyACM0 ESPTOOL_BINDIR=./
```

Expected: USB ID `303a:1001`, ESP32-S3 rev 0.2, 8 MB PSRAM, 8 MB flash, and
`Hash of data verified`. Do not open a serial monitor immediately after flash;
allow the RTS reset to boot normally. Recompute SHA-256 because the flash
target relinks version metadata.

- [ ] **Step 4: Collect four full-inference samples for each physical scene**

Hold each of `EMPTY`, `FACE`, `PALM`, and `EMPTY2` until the `G:` generation
changes at least four times. For every generation record these exact columns:
scene name, sample number 1–4, generation, raw-change indicator, MSR-input-
change indicator, MNP attempted count, MNP accepted count, and raw neural face
count.

For one generation in each scene, explicitly run `agentguard face-diag` from
NSH and capture the complete fixed-order snapshot. Record only the observed
numeric output in the handoff document.

- [ ] **Step 5: Apply the Stage A decision table and stop**

Classify exactly one first divergence:

```text
RAW unchanged across obvious scene changes
  -> V4L2 buffer content/ownership/completeness boundary
RAW changes, MSR input unchanged
  -> preprocessor read/address/cache/caps propagation boundary
MSR input changes, MSR score/box tensors unchanged
  -> MSR numerical execution/tensor reuse boundary
MSR tensors/candidates change, MNP input unchanged or wrong crop
  -> MNP ROI transformation/preprocessor boundary
MNP input changes, MNP output stays fixed/rejected
  -> MNP numerical execution/postprocessor boundary
```

Do not change code after classification. Report the evidence and obtain a new
design approval for Stage B.

- [ ] **Step 6: Update the handoff record without mixing it into code commits**

Append the task commits, RED/GREEN evidence, test/build output, image size,
final SHA-256, esptool verification, four-scene matrix, full snapshots, and
the single classified boundary to `AGENTGUARD_PROGRESS.md`. Explicitly record
that fallback/hold behavior remains active and Stage A did not fix face
accuracy.

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui diff --check -- AGENTGUARD_PROGRESS.md
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui status --short -- AGENTGUARD_PROGRESS.md
```

Expected: no whitespace errors; the handoff remains a separate modified file.
