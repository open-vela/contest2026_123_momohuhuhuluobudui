# ESP-DL Submission Baseline and Evidence Gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a clean-clone-reproducible AgentGuard repository and collect the missing full-C ESP-DL face-versus-empty evidence that selects the next implementation branch.

**Architecture:** This is the first of two implementation plans for the approved deadline design. It first turns the existing working tree into a reproducible submission without changing detection behavior, then uses the already-flashed full-C ESP-DL diagnostic boundary to compare repeated empty and single-face samples. The evidence gate ends with exactly one decision: plan deterministic post-processing only if neural outputs distinguish the scenes, otherwise plan a bounded official model/version replacement.

**Tech Stack:** Git/repo manifests, Bash, C11/C++20 host tests, Python `unittest`, openvela/NuttX, ESP-DL v3.2.0-compatible sources, ESP32-S3 OpenOCD/JTAG.

**Spec:** `docs/superpowers/specs/2026-09-05-espdl-submission-scope-design.md`

## Global Constraints

- The formal face source is ESP-DL only; the skin-color fallback must not be presented as a product detector.
- The final public result is `0`, `1`, or `2+`; identity recognition and exact counts above two are out of scope.
- Do not modify public `nuttx/`, `apps/`, `packages/`, or `vendor/` source in this plan.
- Preserve unrelated pre-existing changes in `README.md`, `AGENTGUARD_PROGRESS.md`, `board/`, `logs/`, and the manifest until a task explicitly names them.
- Never commit Wi-Fi credentials, bearer tokens, personal network addresses, firmware images, object files, JTAG dumps, Python caches, or test binaries.
- Keep `logs/` available for the contest; do not add a broad ignore rule for it.
- Treat host tests, target builds, flashing, and physical detection as separate evidence gates.
- Existing-model evidence collection is limited to half a working day. Do not tune thresholds or change behavior during this gate.
- Any JTAG transaction must resume the target on both success and failure paths.

---

## File Structure

### Files created by this plan

- `.gitignore`: repository-wide generated-file and local-artifact exclusions.
- `docs/submission/2026-09-05-source-inventory.md`: records what must exist in a clean submission and why large vendored/model files are present.

### Existing files added to Git without behavior changes

- `app/agentguard/`: application metadata, portable core, adapters, tests, ESP-DL compatibility sources, the minimal vendored ESP-DL source set, face model, reference image, and licenses.
- `monitor/agentguard_pc.py`: authenticated desktop agent.
- `monitor/test_agentguard_pc.py`: PC-agent regression tests.
- `monitor/README.md`, `monitor/requirements.txt`: PC-agent operation and dependencies.
- `skills/develop-agentguard-openvela/`: contest/openvela development Skill and workspace checks.
- `tools/apply_agentguard_config.sh`: reproducible ESP32-S3-EYE target configuration.
- `tools/hw_raw_serial.py`, `tools/hw_serial_probe.py`: existing hardware diagnostic helpers; source only, no captures.

### Existing files modified by this plan

- `contest2026_123_momohuhuhuluobudui.xml`: commit the existing AgentGuard linkfile so a clean repo sync exposes the application.
- `AGENTGUARD_PROGRESS.md`: append only the clean-clone evidence and full-C scene comparison; do not rewrite older evidence.

### Files deliberately left for a later plan

- `app/agentguard/src/vision_espdl.cpp`: candidate filtering and result folding depend on the evidence gate.
- `app/agentguard/src/agentguard_main.c`: removal of the skin fallback belongs to the selected product implementation branch.
- `app/agentguard/src/face_presence.c`: three-state temporal stabilization belongs to the selected product implementation branch.
- `README.md`: final public claims must wait for actual ESP-DL physical acceptance.
- `board/contest_board/**`: unrelated scaffold edits are not part of AgentGuard's ESP32-S3-EYE application path.
- `logs/yhx06/**`: log export reconciliation is a submission-material task after the code baseline is safe.

---

### Task 1: Establish a Safe Ignore Boundary

**Files:**
- Create: `.gitignore`
- Reference: `.gitignore.example`

**Interfaces:**
- Consumes: current untracked build products under `app/agentguard/`, `monitor/`, and `tools/`.
- Produces: an ignore policy under which `git add app/agentguard` adds sources/models/licenses but not generated objects or test executables.

- [ ] **Step 1: Record the current failure state**

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  status --short | rg '\.(o|pyc)$|app/agentguard/tests/test_[^./]+$|app/agentguard/\.built|app/agentguard/\.depend|app/agentguard/Make\.dep'
```

Expected: the command prints object files, Python bytecode, generated Make metadata, and extensionless test executables. This is the RED submission-hygiene evidence.

- [ ] **Step 2: Create the minimal `.gitignore`**

Create exactly this initial policy, extending the supplied example only for artifacts already present in this project:

```gitignore
# Build outputs
build/
out/
*.o
*.a
*.elf
*.hex
*.map
*.pyc
__pycache__/

# NuttX application dependency markers generated in the source link
/app/agentguard/.built
/app/agentguard/.depend
/app/agentguard/Make.dep

# Extensionless host-test executables; keep their source files
/app/agentguard/tests/test_*
!/app/agentguard/tests/test_*.c
!/app/agentguard/tests/test_*.cpp
!/app/agentguard/tests/test_*.py

# Local checkout and diagnostic material, never part of the submission
/.git.codex-hold
/third_party/
/test_lcd.c
/ESP32-S3-EYE_Face_Detection_Guide.md
/Quick_Start_Face_Detection.md
/申请材料.docx
/2026 首届 openvela AI 硬件开发者大赛 - 作品提交模板.docx
```

- [ ] **Step 3: Verify generated files are ignored and required inputs are not**

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  check-ignore -v app/agentguard/tests/test_core app/agentguard/src/core.c.home.yhx.Desktop.openvela.contest2026_123_momohuhuhuluobudui.app.agentguard.o monitor/__pycache__/agentguard_pc.cpython-310.pyc
```

Expected: all three paths print matching ignore rules.

Run each required-input check separately:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  check-ignore app/agentguard/src/core.c
```

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  check-ignore app/agentguard/third_party/human_face_detect/human_face_detect.espdl
```

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  check-ignore logs/yhx06/manifest.json
```

Expected: each command exits with status 1 and prints nothing, proving the source, model, and logs remain addable.

- [ ] **Step 4: Review and commit only the ignore policy**

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  diff --check -- .gitignore
```

Then stage and commit:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  add .gitignore
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  commit -m "chore: ignore generated AgentGuard artifacts"
```

Expected: exactly `.gitignore` is committed.

---

### Task 2: Make the AgentGuard Source Tree Clone-Reproducible

**Files:**
- Add: `app/agentguard/**` after Task 1 ignore rules are active
- Add: `monitor/README.md`
- Add: `monitor/agentguard_pc.py`
- Add: `monitor/requirements.txt`
- Add: `monitor/test_agentguard_pc.py`
- Add: `skills/develop-agentguard-openvela/**`
- Add: `tools/apply_agentguard_config.sh`
- Add: `tools/hw_raw_serial.py`
- Add: `tools/hw_serial_probe.py`
- Modify: `contest2026_123_momohuhuhuluobudui.xml`
- Create: `docs/submission/2026-09-05-source-inventory.md`

**Interfaces:**
- Consumes: Task 1's `.gitignore` and the current verified working-tree sources.
- Produces: a Git commit containing every input referenced by `app/agentguard/Makefile`, `src/espdl_model.S`, the manifest, host tests, and PC-agent tests.

- [ ] **Step 1: Prove HEAD is currently incomplete**

Run:

```bash
for path in \
  app/agentguard/Kconfig \
  app/agentguard/src/core.c \
  app/agentguard/src/vision.c \
  app/agentguard/src/storage.c \
  app/agentguard/third_party/human_face_detect/human_face_detect.espdl \
  app/agentguard/third_party/esp-dl/dl/dl_define.hpp \
  monitor/agentguard_pc.py \
  skills/develop-agentguard-openvela/SKILL.md \
  tools/apply_agentguard_config.sh; do
  git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
    --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
    ls-files --error-unmatch "$path"
done
```

Expected: one or more `pathspec ... did not match` errors. Do not suppress them; they are the RED clean-clone evidence.

- [ ] **Step 2: Run the working-tree tests before staging**

Run:

```bash
make -C app/agentguard/tests clean test
```

Expected: all C/C++ executables, the TIE no-stdio contract, and the LCD DMA configuration test pass.

Run:

```bash
python3 -m unittest -v monitor/test_agentguard_pc.py
```

Expected: all PC-agent authorization and dispatch tests pass.

- [ ] **Step 3: Add a source inventory document**

Create `docs/submission/2026-09-05-source-inventory.md` with these sections and facts:

```markdown
# AgentGuard Submission Source Inventory

## Required application inputs

- `app/agentguard/src`, `include`, `compat`: owned application and compatibility code.
- `app/agentguard/third_party/esp-dl`: minimal vendored ESP-DL source subset used by the NuttX Makefile; generated `.o` files are excluded.
- `app/agentguard/third_party/human_face_detect/human_face_detect.espdl`: 191,248-byte embedded MSR+MNP model.
- `app/agentguard/third_party/human_face_detect/human_face_rgb565be.bin`: 153,600-byte diagnostic reference frame.
- Both third-party license files are retained next to their sources.

## Required integration inputs

- `contest2026_123_momohuhuhuluobudui.xml`: links `app/agentguard` into `packages/demos/contest2026_123_agentguard`.
- `tools/apply_agentguard_config.sh`: applies the reproducible ESP32-S3-EYE product configuration.
- `monitor`: authenticated PC event receiver and tests.
- `skills/develop-agentguard-openvela`: project development and evidence rules.

## Excluded generated and local inputs

Object files, dependency markers, test executables, Python caches, firmware images, JTAG dumps, the standalone root `third_party` checkout, private tokens, Wi-Fi credentials, and local submission templates are not source inputs and are excluded.
```

- [ ] **Step 4: Stage only the required source set**

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  add app/agentguard monitor/README.md monitor/agentguard_pc.py monitor/requirements.txt monitor/test_agentguard_pc.py skills/develop-agentguard-openvela tools/apply_agentguard_config.sh tools/hw_raw_serial.py tools/hw_serial_probe.py contest2026_123_momohuhuhuluobudui.xml docs/submission/2026-09-05-source-inventory.md
```

Do not stage `README.md`, `AGENTGUARD_PROGRESS.md`, `board/`, `logs/`, `monitor/sit_reminder_monitor.py`, the root `third_party/`, or any `.docx` file in this task.

- [ ] **Step 5: Audit staged paths for generated files and secrets**

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  diff --cached --name-only | rg '\.(o|pyc|elf|map)$|(^|/)test_[^./]+$|(^|/)nuttx\.bin$'
```

Expected: no output and exit status 1.

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  diff --cached --check
```

Expected: no output.

Run a content scan limited to the staged source paths:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  diff --cached -- | rg -i 'BEGIN (RSA|OPENSSH|EC) PRIVATE KEY|AGENTGUARD_TOKEN=[^<]|wapi psk [^<]|password\s*[:=]\s*[^<]'
```

Expected: no output. If a match is an example placeholder, inspect it before proceeding; never weaken this scan to hide a real credential.

- [ ] **Step 6: Commit the reproducible source boundary**

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  commit -m "build: make AgentGuard source tree reproducible"
```

Expected: the commit contains only the staged files named in Step 4.

- [ ] **Step 7: Verify the committed archive, not the dirty working tree**

Create an isolated directory:

```bash
AG_SUBMISSION_TMP=$(mktemp -d /tmp/agentguard-submission-XXXXXX)
```

Archive and extract HEAD:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  archive HEAD -o "$AG_SUBMISSION_TMP/repo.tar"
tar -xf "$AG_SUBMISSION_TMP/repo.tar" -C "$AG_SUBMISSION_TMP"
```

Run tests from the extracted archive:

```bash
make -C "$AG_SUBMISSION_TMP/app/agentguard/tests" clean test
```

```bash
python3 "$AG_SUBMISSION_TMP/monitor/test_agentguard_pc.py" -v
```

Expected: both suites pass without reading source files from the original worktree.

Run the structural check against the archive and the real openvela dependency root:

```bash
"$AG_SUBMISSION_TMP/skills/develop-agentguard-openvela/scripts/check_workspace.sh" \
  "$AG_SUBMISSION_TMP" /home/yhx/Desktop/openvela
```

Expected: the manifest, BSP, Kconfig, Make.defs, AgentGuard link mapping, and package-link checks are `OK` or the package-link check is informational.

Leave the temporary directory in place until Task 3 completes so a failure can be inspected; remove only that exact `mktemp` path afterward.

---

### Task 3: Rebuild the ESP32-S3-EYE Target from the Committed Inputs

**Files:**
- Read: `skills/develop-agentguard-openvela/references/esp32s3-eye.md`
- Read: `tools/apply_agentguard_config.sh`
- Verify: `/home/yhx/Desktop/openvela/nuttx/nuttx`
- Verify: `/home/yhx/Desktop/openvela/nuttx/nuttx.bin`

**Interfaces:**
- Consumes: Task 2's committed source boundary and the official ESP32-S3-EYE openvela checkout.
- Produces: a target ELF and image whose symbols/configuration correspond to the committed full-C diagnostic code.

- [ ] **Step 1: Read the hardware constraints before touching the target**

Run:

```bash
sed -n '1,320p' skills/develop-agentguard-openvela/references/esp32s3-eye.md
```

Expected: confirm official device nodes, virtual-environment requirement, flash procedure, and physical-evidence boundaries.

- [ ] **Step 2: Re-run the repository workspace contract**

Run:

```bash
skills/develop-agentguard-openvela/scripts/check_workspace.sh \
  /home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  /home/yhx/Desktop/openvela
```

Expected: all required structural checks pass.

- [ ] **Step 3: Confirm the full-C backend contract**

Run:

```bash
make -C app/agentguard/tests test_espdl_backend_config
./app/agentguard/tests/test_espdl_backend_config
```

Expected: PASS with compile-time assertions enforcing `CONFIG_AGENTGUARD_ESP_DL_FORCE_C=1`, `CONFIG_XTENSA_BOOST=0`, `CONFIG_TIE728_BOOST=0`, and portable C convolution/depthwise paths.

- [ ] **Step 4: Apply the product configuration without embedding secrets**

Run from the contest repository with no PC address or token environment variables:

```bash
tools/apply_agentguard_config.sh
```

Expected: configuration completes and does not print a token. Verify these values:

```bash
rg 'CONFIG_LVX_USE_DEMO_CONTEST2026_123_AGENTGUARD|CONFIG_AGENTGUARD_ESP_DL|CONFIG_LCD_ST7789_FREQUENCY|CONFIG_ESP32S3_SPI_DMA_BUFSIZE|CONFIG_SMP' /home/yhx/Desktop/openvela/nuttx/.config
```

Expected: AgentGuard and ESP-DL enabled, LCD frequency `80000000`, DMA buffer `15360`, and SMP disabled.

- [ ] **Step 5: Perform a clean target build**

Run:

```bash
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx clean
make -C /home/yhx/Desktop/openvela/nuttx -j8
```

Expected: the official ESP32-S3 target links and generates `nuttx.bin`. Do not claim physical detection from this result.

- [ ] **Step 6: Verify image boundary and diagnostic symbol**

Run:

```bash
stat -c '%s %n' /home/yhx/Desktop/openvela/nuttx/nuttx.bin
sha256sum /home/yhx/Desktop/openvela/nuttx/nuttx.bin
```

Enforce the partition boundary:

```bash
AG_IMAGE_SIZE=$(stat -c '%s' /home/yhx/Desktop/openvela/nuttx/nuttx.bin)
test "$AG_IMAGE_SIZE" -lt 3145728
```

Expected: the boundary command exits 0. Record the size and SHA-256 in the task notes.

Locate the diagnostic store using the target `nm` tool rather than reusing the old address:

```bash
/home/yhx/.espressif/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin/xtensa-esp32s3-elf-nm \
  -S -C /home/yhx/Desktop/openvela/nuttx/nuttx | rg 'g_face_diag_store'
```

Expected: one BSS symbol. The current ABI size must be hexadecimal `000003f8` (1016 bytes); stop if it differs.

Verify the target ABI offsets offline without connecting GDB to OpenOCD:

```bash
/home/yhx/.espressif/tools/xtensa-esp-elf-gdb/17.1_20260402/xtensa-esp-elf-gdb/bin/xtensa-esp32s3-elf-gdb \
  --batch /home/yhx/Desktop/openvela/nuttx/nuttx \
  -ex 'ptype /o struct ag_face_diag_store' \
  -ex 'ptype /o struct ag_face_diag_snapshot' \
  -ex 'ptype /o struct ag_face_detector_trace'
```

Expected target-file offsets, all relative to the beginning of `g_face_diag_store`:

```text
snapshot.inference_sequence               28
snapshot.raw.full_hash                    60
snapshot.live.msr_input.hash             548
snapshot.live.msr_resize                 628
snapshot.live.msr_candidate_before_clip  656
snapshot.live.msr_candidate_after_clip   676
snapshot.live.msr_candidates            1000
snapshot.live.mnp_attempts              1001
snapshot.live.mnp_recorded              1002
snapshot.live.mnp_accepted              1003
snapshot.live.final_faces               1004
snapshot.live.valid                     1005
snapshot.valid                          1012
```

Stop if GDB reports different offsets. Record the symbol address, size, and confirmed offsets; Stage A's historical address must not be assumed.

- [ ] **Step 7: Flash only if the rebuilt image differs from the image currently under test**

First inspect the connected target and port without opening the serial console:

```bash
lsusb
ls -l /dev/ttyACM0
fuser /dev/ttyACM0
```

Expected: USB `303a:1001`, `/dev/ttyACM0` exists, and `fuser` prints no owner.

If the rebuilt hash differs from the recorded full-C image being tested, flash:

```bash
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx -j8 flash ESPTOOL_PORT=/dev/ttyACM0 ESPTOOL_BINDIR=./
```

Expected: esptool reports `Hash of data verified`. Record the post-flash relinked image hash. If the current board image is already proven identical, skip flashing and record the equality evidence.

---

### Task 4: Collect the Full-C Empty/Face Evidence Gate

**Files:**
- Create outside repository: `/tmp/agentguard-full-c-empty-{1,2,3}.bin`
- Create outside repository: `/tmp/agentguard-full-c-face-{1,2,3}.bin`
- Modify: `AGENTGUARD_PROGRESS.md`

**Interfaces:**
- Consumes: Task 3's exact ELF diagnostic-store address/size and the running full-C firmware.
- Produces: repeated scene evidence and a binary decision for the next plan: `POSTPROCESS` or `REPLACE_MODEL`.

- [ ] **Step 1: Confirm the USB-JTAG node and target state**

Run:

```bash
lsusb
lsusb -t
```

Expected: identify the current bus/device for USB `303a:1001`. If its `/dev/bus/usb/BBB/DDD` node is not writable, ask the user to grant permission to that exact node; do not request a broad USB permission change.

- [ ] **Step 2: Prepare a resume-safe OpenOCD capture command**

Read the current symbol directly from the Task 3 ELF and validate its exact size before every capture:

```bash
read -r AG_DIAG_ADDRESS AG_DIAG_SIZE AG_DIAG_TYPE AG_DIAG_NAME < <(
  /home/yhx/.espressif/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin/xtensa-esp32s3-elf-nm \
    -S -C /home/yhx/Desktop/openvela/nuttx/nuttx | awk '/g_face_diag_store$/ {print $1, $2, $3, $4}'
)
test "$AG_DIAG_SIZE" = 000003f8
/home/yhx/.espressif/tools/openocd-esp32/v0.12.0-esp32-20260424/openocd-esp32/bin/openocd \
  -f board/esp32s3-builtin.cfg \
  -c 'adapter speed 1000' \
  -c 'init' \
  -c 'halt' \
  -c "dump_image /tmp/agentguard-full-c-empty-1.bin 0x${AG_DIAG_ADDRESS} 0x${AG_DIAG_SIZE}" \
  -c 'resume' \
  -c 'shutdown'
```

Expected: the dump is exactly 1016 bytes, and the transaction reaches `resume`. If any command fails, immediately run this separate recovery transaction before continuing:

```bash
/home/yhx/.espressif/tools/openocd-esp32/v0.12.0-esp32-20260424/openocd-esp32/bin/openocd \
  -f board/esp32s3-builtin.cfg \
  -c 'adapter speed 1000' \
  -c 'init' \
  -c 'resume' \
  -c 'shutdown'
```

- [ ] **Step 3: Capture three empty-scene samples**

Ask the user to ensure no face, photograph, or face-like screen is visible. Wait for at least three completed inference generations between captures. Run the Step 2 transaction three times, changing the output suffix to `empty-1`, `empty-2`, and `empty-3`.

After each capture run:

```bash
stat -c '%s %n' /tmp/agentguard-full-c-empty-1.bin
sha256sum /tmp/agentguard-full-c-empty-1.bin
```

Expected: each file is 1016 bytes. Record all hashes; do not put the binary files in Git.

- [ ] **Step 4: Capture three single-face samples**

Ask the user to place one real face near the center at normal desk distance with no second face or face image visible. Wait for at least three completed inference generations between captures. Repeat the same resume-safe transaction for `face-1`, `face-2`, and `face-3`.

After each capture run:

```bash
stat -c '%s %n' /tmp/agentguard-full-c-face-1.bin
sha256sum /tmp/agentguard-full-c-face-1.bin
```

Expected: each file is 1016 bytes and the target resumes after every dump.

- [ ] **Step 5: Decode the confirmed target ABI fields**

Task 3 must have confirmed the 1016-byte store and exact offsets before this step. Run the same commands for each of the six files, changing only `AG_DIAG_DUMP`:

```bash
AG_DIAG_DUMP=/tmp/agentguard-full-c-empty-1.bin
od -An -j 28 -N 4 -t u4 "$AG_DIAG_DUMP"
od -An -j 60 -N 4 -t x4 "$AG_DIAG_DUMP"
od -An -j 548 -N 4 -t x4 "$AG_DIAG_DUMP"
od -An -j 628 -N 16 -t d4 "$AG_DIAG_DUMP"
od -An -j 656 -N 16 -t d4 "$AG_DIAG_DUMP"
od -An -j 672 -N 2 -t u2 "$AG_DIAG_DUMP"
od -An -j 676 -N 16 -t d4 "$AG_DIAG_DUMP"
od -An -j 692 -N 2 -t u2 "$AG_DIAG_DUMP"
od -An -j 1000 -N 6 -t u1 "$AG_DIAG_DUMP"
od -An -j 1012 -N 1 -t u1 "$AG_DIAG_DUMP"
```

Interpret the output in this fixed order:

```text
inference sequence
raw-frame full hash
live MSR preprocessed-input hash
resize x/y/inverse-x/inverse-y millionths
top candidate before clipping: left/top/right/bottom
top candidate before-clipping score, permille
top candidate after clipping: left/top/right/bottom
top candidate after-clipping score, permille
MSR candidates / MNP attempts / MNP recorded / MNP accepted / final faces / live-valid
snapshot-valid
```

Expected: both valid bytes are 1; sequences change between captures; raw and MSR input hashes demonstrate that current camera data reached preprocessing. Save the decoded values in the progress-document notes, not in another binary file.

- [ ] **Step 6: Apply the evidence decision table without tuning**

Choose `POSTPROCESS` only when all of these are true:

```text
- raw and preprocessed fingerprints vary across captures, proving live frames;
- empty and face samples show a repeatable difference in neural candidate coordinates,
  neural candidate scores, accepted-result count, or final boxes;
- the difference is present in at least two of three samples for each scene;
- a deterministic rule could separate scenes without using skin pixels or scene-specific coordinates.
```

Choose `REPLACE_MODEL` when any of these are true:

```text
- empty and face samples retain the same accepted count and effectively identical boxes/scores;
- all scenes saturate to the same false-positive pattern;
- only raw-frame hashes differ while neural decision fields do not;
- separation would require memorizing the test background, using skin color, or arbitrary threshold tuning.
```

Do not modify thresholds, model files, ESP-DL code, or product behavior in this task.

- [ ] **Step 7: Record and commit the evidence gate**

Append a dated section to `AGENTGUARD_PROGRESS.md` containing:

```markdown
## 2026-09-05 截止期收敛：全 C ESP-DL 真人对照

- Firmware SHA-256 and size
- Diagnostic symbol address, size, and decoded ABI source
- Three EMPTY sample hashes and decoded neural fields
- Three FACE sample hashes and decoded neural fields
- Core-resume verification after JTAG
- Decision: POSTPROCESS or REPLACE_MODEL
- Exact reason copied from the decision table
- Explicit statement that no threshold or behavior was changed
```

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  diff --check -- AGENTGUARD_PROGRESS.md
```

Then stage only the progress document and commit:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  add AGENTGUARD_PROGRESS.md
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  commit -m "docs: record full-C ESP-DL scene evidence"
```

- [ ] **Step 8: Stop at the branch boundary**

If the decision is `POSTPROCESS`, create a new implementation plan covering candidate normalization, overlap suppression, `0/1/2+` folding, temporal confirmation, removal of the skin fallback, `AI ERROR`, product HUD, and physical acceptance.

If the decision is `REPLACE_MODEL`, create a new bounded design and plan naming the exact official ESP-DL model/version, provenance, license, memory budget, integration file changes, rollback point, and empty/face acceptance probe.

Do not start either branch from this plan; their exact implementation depends on the evidence recorded in Step 7.

---

## Final Verification for This Plan

- [ ] `git diff --check` reports no whitespace errors in the committed plan work.
- [ ] `git archive HEAD` contains all files referenced by `app/agentguard/Makefile` and `src/espdl_model.S`.
- [ ] AgentGuard host tests pass from the extracted archive.
- [ ] PC-agent tests pass from the extracted archive.
- [ ] The workspace contract sees the AgentGuard manifest mapping in committed content.
- [ ] The official ESP32-S3-EYE target build succeeds and the image stays below `0x300000`.
- [ ] Six valid, resume-safe JTAG samples are recorded outside Git.
- [ ] `AGENTGUARD_PROGRESS.md` contains exactly one evidence-based branch decision.
- [ ] No product behavior, threshold, model, NuttX public source, secret, firmware, or JTAG binary is committed by this plan.
