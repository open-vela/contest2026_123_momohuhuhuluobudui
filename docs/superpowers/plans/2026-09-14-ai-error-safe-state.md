# AgentGuard AI Error Safe-State Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a debounced, visible, logged safe state for sustained ESP-DL inference failures while freezing all face-dependent policy timers until trustworthy inference recovers.

**Architecture:** A small `vision_health` module converts per-frame inference outcomes into entry/recovery transitions. Core accepts explicitly valid or invalid vision observations and freezes its absolute timers across invalid intervals. The existing main loop, event log, and RGB565 UI consume those two interfaces without changing the ESP-DL model or detector.

**Tech Stack:** C11, NuttX/openvela, ESP-DL adapter, RGB565 UI, JSONL storage, host `make` tests, ESP32-S3 target build.

**Spec:** `docs/superpowers/specs/2026-09-14-ai-error-safe-state-design.md`

## Execution Adjustments (2026-09-15)

- Tasks 1-3 are implemented and host-verified in commits `4b3f2a0`,
  `0b68a0f`, and `e67b45a`.
- Task 4's proposed static source integration test was replaced by real runtime
  tests of `ag_vision_health_gate()`. Main calls this gate to clear all
  untrusted results. Tests cover failed data, the first recovery success, and
  successful recovery while preserving faces; actual main-loop wiring was
  independently reviewed and target-compiled rather than claiming a source
  string check proves runtime integration.
- Review reproduced invalid ACK followed by immediate renewed sedentary alert.
  A failing core regression preceded the `vision_ack_pending` correction.
- The adapter now leaves its three-frame schedule on a fresh-inference slot
  after failure, preventing two cached-success frames from masking the fault.
- A clean target worktree build exposed the missing official
  `dl_image_bmp.hpp` dependency. The unchanged header was copied from the
  original checkout, verified with `cmp`, and added to submission sources.
- Independent re-review found no remaining Critical/Important code issues;
  physical smoke evidence remains a separate completion gate.
- The next target link exposed the missing official ESP32-S3
  `libfbs_model.a`. The unchanged original archive is now tracked with an
  exact `.gitignore` exception; SHA256 is
  `d0250350c897f2c5243066ae1c9eb337fb926515c9553a8a422a4fe5d6d82020`.
- Switching source paths left both original and worktree object members in
  NuttX's incremental `libapps.a`. The first successful link still selected
  old code and is explicitly rejected as integration evidence. After backing
  up that archive to `/tmp/agentguard-archive-backup.pxZIGJ/libapps.a`, only
  the 102 original AgentGuard object members were removed and the image was
  relinked. `nm` confirms `ag_vision_health_init`, `ag_vision_health_update`,
  and `ag_vision_health_gate` are present in the final ELF.
- Software verification: 27 host C/C++ executables and 4 Python checks pass;
  target build exits zero; `git diff --check` passes. Image size is 2,414,644
  bytes (below 3,145,728); SHA256 is
  `3bd26040177ef44a74226e5176e0441a194aa2834afe5a727b954b0dcedaf481`.
  The application source link has been restored to the original checkout.
- Tasks 1-4 and Task 5 review steps are software-complete under the adjustments
  above. Flash, normal-path physical smoke, and hardware fault/recovery smoke
  are not complete: `/dev/ttyACM0` is currently unavailable. No flash or erase
  was performed. The original detailed checkboxes below are historical plan
  instructions, not evidence overriding this execution record.

## Global Constraints

- Enter AI error after exactly 3 consecutive failed inference attempts.
- Recover after exactly 2 consecutive successful inference attempts.
- Freeze face-dependent timers from the first failed attempt until recovery.
- In AI error, show `FACE:--` and `AI ERROR`, publish no face boxes, and never substitute heuristic results.
- Log one `ai_error` event per entry and one `ai_recovered` event per recovery.
- Do not alter ESP-DL source, model bytes, score thresholds, face-presence hold duration, camera restart behavior, KWS, gestures, or owner recognition.
- Keep `nuttx.bin` below the LittleFS boundary at `0x300000` (3,145,728 bytes).
- Preserve all unrelated dirty workspace changes.

---

### Task 1: Debounce Inference Health

**Files:**
- Create: `app/agentguard/include/agentguard/vision_health.h`
- Create: `app/agentguard/src/vision_health.c`
- Create: `app/agentguard/tests/test_vision_health.c`
- Modify: `app/agentguard/tests/Makefile`
- Modify: `app/agentguard/Makefile`
- Modify: `app/agentguard/CMakeLists.txt`

**Interfaces:**
- Consumes: one Boolean `inference_ok` outcome per captured frame.
- Produces: `ag_vision_health_init()`, `ag_vision_health_update()`, `struct ag_vision_health_state`, and `enum ag_vision_health_transition`.

- [ ] **Step 1: Write the failing health-state test**

Create `test_vision_health.c` with tests that assert:

```c
struct ag_vision_health_state state;

ag_vision_health_init(&state);
assert(!state.error_active);
assert(ag_vision_health_update(&state, false) ==
       AG_VISION_HEALTH_NO_CHANGE);
assert(ag_vision_health_update(&state, true) ==
       AG_VISION_HEALTH_NO_CHANGE); /* clears the first failure */
assert(ag_vision_health_update(&state, false) ==
       AG_VISION_HEALTH_NO_CHANGE);
assert(ag_vision_health_update(&state, false) ==
       AG_VISION_HEALTH_NO_CHANGE);
assert(ag_vision_health_update(&state, false) ==
       AG_VISION_HEALTH_ENTERED_ERROR);
assert(state.error_active);
assert(ag_vision_health_update(&state, false) ==
       AG_VISION_HEALTH_NO_CHANGE); /* no duplicate entry */
assert(ag_vision_health_update(&state, true) ==
       AG_VISION_HEALTH_NO_CHANGE);
assert(ag_vision_health_update(&state, false) ==
       AG_VISION_HEALTH_NO_CHANGE); /* clears recovery progress */
assert(ag_vision_health_update(&state, true) ==
       AG_VISION_HEALTH_NO_CHANGE);
assert(ag_vision_health_update(&state, true) ==
       AG_VISION_HEALTH_RECOVERED);
assert(!state.error_active);
```

Add `test_vision_health` to `TARGETS`, its build rule, and the `test` recipe.

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```bash
make -C app/agentguard/tests test_vision_health
```

Expected: compilation fails because `agentguard/vision_health.h` and its API do not exist.

- [ ] **Step 3: Implement the minimal health module**

Define:

```c
#define AG_VISION_FAILURE_LIMIT 3
#define AG_VISION_RECOVERY_LIMIT 2

enum ag_vision_health_transition
{
  AG_VISION_HEALTH_NO_CHANGE = 0,
  AG_VISION_HEALTH_ENTERED_ERROR,
  AG_VISION_HEALTH_RECOVERED,
};

struct ag_vision_health_state
{
  uint8_t consecutive_failures;
  uint8_t consecutive_successes;
  bool error_active;
};

void ag_vision_health_init(struct ag_vision_health_state *state);
enum ag_vision_health_transition ag_vision_health_update(
  struct ag_vision_health_state *state, bool inference_ok);
```

Before entry, success resets failures; failure saturates the failure counter
and enters exactly at `AG_VISION_FAILURE_LIMIT`. During error, failure resets
successes; success saturates the success counter and recovers exactly at
`AG_VISION_RECOVERY_LIMIT`.

Add `src/vision_health.c` to both application source lists.

- [ ] **Step 4: Run the focused test and verify GREEN**

Run:

```bash
make -C app/agentguard/tests test_vision_health
./app/agentguard/tests/test_vision_health
```

Expected: `AgentGuard vision health tests: PASS`.

- [ ] **Step 5: Run the full host suite**

Run:

```bash
make -C app/agentguard/tests clean test
```

Expected: every C/C++ and Python test passes.

- [ ] **Step 6: Commit the isolated module**

Stage only the six Task 1 files and commit:

```bash
git commit -m "feat: debounce ESP-DL inference health"
```

---

### Task 2: Freeze Policy Timers and Add AI Events

**Files:**
- Modify: `app/agentguard/include/agentguard/core.h`
- Modify: `app/agentguard/src/core.c`
- Modify: `app/agentguard/include/agentguard/storage.h`
- Modify: `app/agentguard/tests/test_core.c`
- Modify: `app/agentguard/tests/test_storage.c`

**Interfaces:**
- Consumes: `struct ag_observation.vision_valid` on every completed camera frame.
- Produces: pause-safe `ag_step()` behavior plus `AG_EVENT_AI_ERROR` and `AG_EVENT_AI_RECOVERED` names usable by main and storage.

- [ ] **Step 1: Write failing core tests for invalid observations**

Update the existing `step()` helper to accept a `bool vision_valid` argument
and set the new field. Update all existing calls to pass `true`. Add tests
covering these behaviors:

```c
/* Invalid face_count must not change presence or trigger break/privacy. */
events = step(&state, &config, 500, 0, 100, AG_COMMAND_NONE, false);
assert(events == 0);
assert(state.present);

/* Explicit commands still work while vision is invalid. */
events = step(&state, &config, 600, 0, 0, AG_COMMAND_PAUSE, false);
assert(events & AG_EVENT_REMINDERS_PAUSED);

/* 900 ms of valid sitting + 5 s invalid + 99 ms valid is not 1 s. */
step(&state, &config, 100, 1, 0, AG_COMMAND_NONE, true);
step(&state, &config, 1000, 1, 0, AG_COMMAND_NONE, true);
step(&state, &config, 1100, 0, 0, AG_COMMAND_NONE, false);
assert(!(step(&state, &config, 6100, 1, 0, AG_COMMAND_NONE, true) &
         AG_EVENT_SEDENTARY_ALERT));
assert(step(&state, &config, 6200, 1, 0, AG_COMMAND_NONE, true) &
       AG_EVENT_SEDENTARY_ALERT);
```

Also test that recovery shifts active absence, posture, and acknowledgement
timestamps, and that shifting a timestamp near `UINT64_MAX` saturates rather
than wrapping.

- [ ] **Step 2: Add failing event-name and storage-stat assertions**

Assert:

```c
assert(strcmp(ag_event_name(AG_EVENT_AI_ERROR), "ai_error") == 0);
assert(strcmp(ag_event_name(AG_EVENT_AI_RECOVERED), "ai_recovered") == 0);
```

Append both events to the storage test log and assert each count is one.

- [ ] **Step 3: Run focused tests and verify RED**

Run:

```bash
make -C app/agentguard/tests test_core test_storage
```

Expected: compilation fails because the observation field and AI events do not exist.

- [ ] **Step 4: Implement invalid-observation pause semantics**

Add to `struct ag_observation`:

```c
bool vision_valid;
```

Add to `struct ag_state`:

```c
bool vision_paused;
uint64_t vision_paused_since_ms;
```

In `ag_step()`, apply the explicit command first. On the first invalid
observation, set the pause flag/time and return command events. On a valid
observation after a pause, compute `paused_ms`, shift each nonzero active
timer with a saturating helper, clear the pause state, and then execute the
existing face-dependent logic unchanged.

- [ ] **Step 5: Implement event vocabulary and storage slots**

Add bits 14 and 15:

```c
AG_EVENT_AI_ERROR     = 1u << 14,
AG_EVENT_AI_RECOVERED = 1u << 15,
```

Map them to `ai_error` and `ai_recovered` in `ag_event_name()`. Change
`AG_STORAGE_EVENT_SLOTS` from 14 to 16; the existing generic parser and stats
arrays then handle both events without special cases.

- [ ] **Step 6: Run focused tests and verify GREEN**

Run:

```bash
make -C app/agentguard/tests test_core test_storage
./app/agentguard/tests/test_core
./app/agentguard/tests/test_storage
```

Expected: both test programs print `PASS`.

- [ ] **Step 7: Run the full host suite and commit**

Run `make -C app/agentguard/tests clean test`, stage only the five Task 2
files, and commit:

```bash
git commit -m "feat: freeze policy during invalid vision"
```

---

### Task 3: Render the AI Error State

**Files:**
- Modify: `app/agentguard/include/agentguard/display_ui.h`
- Modify: `app/agentguard/src/display_ui.c`
- Modify: `app/agentguard/tests/test_display_ui.c`

**Interfaces:**
- Consumes: `struct ag_ui_status.ai_error` supplied by main.
- Produces: `ag_ui_format_face_count()` and `ag_ui_primary_status()` for directly testable `FACE:--` and `AI ERROR` presentation.

- [ ] **Step 1: Write failing UI behavior tests**

Add `bool ai_error` to the expected status contract and tests:

```c
char text[32];
uint16_t color;

memset(&status, 0, sizeof(status));
status.ai_error = true;
ag_ui_format_face_count(text, sizeof(text), &status);
assert(strcmp(text, "FACE:--") == 0);
assert(strcmp(ag_ui_primary_status(&status, &color), "AI ERROR") == 0);
assert(color == ag_ui_face_color(&status));

status.camera_stale = true;
ag_ui_render_rgb565(frame, WIDTH, HEIGHT, VIEW_WIDTH, HEIGHT, &status);
/* Existing CAM status pixel assertion remains valid: render-time camera
 * stale selection must take precedence over ag_ui_primary_status(). */
```

Also assert healthy counts still format as `FACE:0`, `FACE:1`, and `FACE:9+`.

- [ ] **Step 2: Run focused test and verify RED**

Run:

```bash
make -C app/agentguard/tests test_display_ui
```

Expected: compilation fails because `ai_error` and the formatting APIs do not exist.

- [ ] **Step 3: Implement the UI state**

Add `bool ai_error` to `struct ag_ui_status`. Make
`ag_ui_format_face_count()` own the existing numeric formatting and return
`FACE:--` first when `ai_error` is true. Make the existing static primary
status selector public as `ag_ui_primary_status()` and check `ai_error`
before paused/no-person states, returning `AI ERROR` in red. Make
`ag_ui_face_color()` return red first for AI error. Keep the existing
render-level `camera_stale` branch unchanged so `CAM:*` remains higher
priority.

- [ ] **Step 4: Run focused and full tests and verify GREEN**

Run:

```bash
make -C app/agentguard/tests test_display_ui
./app/agentguard/tests/test_display_ui
make -C app/agentguard/tests clean test
```

Expected: all tests pass.

- [ ] **Step 5: Commit the UI change**

Stage only the three Task 3 files and commit:

```bash
git commit -m "feat: display unavailable AI results safely"
```

---

### Task 4: Integrate Health, Policy, Logging, and Display

**Files:**
- Modify: `app/agentguard/src/agentguard_main.c`
- Create: `app/agentguard/tests/test_ai_error_integration.py`
- Modify: `app/agentguard/tests/Makefile`

**Interfaces:**
- Consumes: Task 1 health transitions, Task 2 valid-observation semantics/events, and Task 3 UI status.
- Produces: complete runtime behavior for inference error entry, sustained error, and automatic recovery.

- [ ] **Step 1: Write the failing source-level integration guard**

Create a Python test that reads `src/agentguard_main.c` and asserts all of the
following structural requirements:

```python
assert "struct ag_vision_health_state vision_health;" in source
assert "ag_vision_health_init(&vision_health);" in source
assert "ag_vision_health_update(&vision_health, inference_ok)" in source
assert "AG_EVENT_AI_ERROR" in source
assert "AG_EVENT_AI_RECOVERED" in source
assert "observation.vision_valid = inference_trusted;" in source
assert "ui_status.ai_error = true;" in source
assert "ag_face_presence_reset(&face_presence);" in source
```

Parse the main-loop section and assert the textual order is inference call,
health update, invalid/trusted branch, face-authority application, then camera
buffer requeue. Add the Python test to the host `test` recipe.

- [ ] **Step 2: Run the integration guard and verify RED**

Run:

```bash
python3 app/agentguard/tests/test_ai_error_integration.py
```

Expected: assertion failure at the missing health state.

- [ ] **Step 3: Integrate one health update per inference attempt**

Initialize `vision_health` next to `vision` and `face_presence`. In each frame,
store the return of `ag_vision_process_rgb565()` as `inference_ok`, update
health, and compute:

```c
inference_trusted = inference_ok && !vision_health.error_active;
```

On `AG_VISION_HEALTH_ENTERED_ERROR`, reset face presence and dispatch only
`AG_EVENT_AI_ERROR`. On `AG_VISION_HEALTH_RECOVERED`, dispatch only
`AG_EVENT_AI_RECOVERED`.

- [ ] **Step 4: Submit invalid observations and publish the error UI**

Read the button once per captured frame. Set
`observation.vision_valid = inference_trusted`; use zero face/posture values
when it is false, and call `ag_step()` even for invalid observations so timer
pause/resume bookkeeping is continuous.

When `vision_health.error_active` is true, zero a separate display result,
set `ui_status.ai_error = true`, preserve frame timing/state flags, and call
`ag_display_publish()` with zero boxes. For the first two failures before
entry, do not publish untrusted face data. When inference is trusted, run the
existing face-authority and normal UI path without changing its diagnostics.

- [ ] **Step 5: Run integration and complete host verification**

Run:

```bash
python3 app/agentguard/tests/test_ai_error_integration.py
make -C app/agentguard/tests clean test
git diff --check
```

Expected: integration guard and full host suite pass; diff check reports no errors.

- [ ] **Step 6: Build the ESP32-S3 target**

Apply the reproducible project config and build:

```bash
tools/apply_agentguard_config.sh
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx -j8
```

Then verify:

```bash
test "$(stat -c %s /home/yhx/Desktop/openvela/nuttx/nuttx.bin)" -lt 3145728
sha256sum /home/yhx/Desktop/openvela/nuttx/nuttx.bin
```

Expected: build exits zero and the image remains below `0x300000`.

- [ ] **Step 7: Commit runtime integration**

Stage only the three Task 4 files and commit:

```bash
git commit -m "feat: enter safe state on ESP-DL failure"
```

---

### Task 5: Review and Hardware Smoke Test

**Files:**
- Modify only if review finds an approved issue in the Task 1-4 files.

**Interfaces:**
- Consumes: all implementation commits and the built image.
- Produces: reviewed, flashed, physically verified submission candidate.

- [ ] **Step 1: Request code review**

Use `superpowers:requesting-code-review` and review against the design spec,
with special attention to timer arithmetic, event de-duplication, invalid data
leaks, and preservation of the normal ESP-DL path.

- [ ] **Step 2: Handle review findings rigorously**

Use `superpowers:receiving-code-review`; reproduce each valid finding, add a
failing regression test, implement the minimal fix, and rerun the full host
suite plus target build.

- [ ] **Step 3: Flash without erasing LittleFS**

With `/dev/ttyACM0` available, run the established project flash command. Do
not run erase-flash and do not write at or above `0x300000`. Require the tool
to report `Hash of data verified`.

- [ ] **Step 4: Perform the live normal-path smoke test**

Verify on the device:

- empty scene displays stable `FACE:0`;
- one front-facing face for about 15 seconds displays stable `FACE:1` and one
  box;
- two faces retain the previously accepted count and multi-box behavior;
- no `AI ERROR` appears during ordinary successful inference;
- existing event log remains readable with `agentguard stats`.

- [ ] **Step 5: Record final evidence**

Record commit IDs, host test count/output, target image byte size, SHA-256,
flash verification, and hardware observations in the existing dated progress
log. Do not edit unrelated historical evidence.
