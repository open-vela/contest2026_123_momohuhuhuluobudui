# ESP-DL Product Firmware Validation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the stale on-board ESP-DL diagnostic image with a product build from the current source, then prove continuous camera/display/inference operation on ESP32-S3-EYE without regressing AgentGuard's tested behavior.

**Architecture:** Keep the current `agentguard_main.c` hardware adapter, `vision.c` dispatch layer, and `vision_espdl.cpp` MSR+MNP implementation unchanged unless a reproducible failure identifies a specific defect. Treat source tests, cross-build provenance, ELF/image gates, flashing, and physical-board observations as separate evidence boundaries.

**Tech Stack:** openvela/NuttX, ESP32-S3-EYE, Xtensa GCC, ESP-DL v3.2.0, MSR+MNP S8 model, C/C++ host tests, Python `unittest`, esptool v5.3.1, USB Serial/JTAG.

## Global Constraints

- Keep `CONFIG_AGENTGUARD_ESP_DL=y`, `CONFIG_XTENSA_CP_INITSET=0x0009`, the uniprocessor configuration, BLE isolation, and the LittleFS partition at `0x300000`.
- Keep the 0.60/0.60 score thresholds, inference once every three frames, and the eight-face result limit.
- Do not change the health state machine, JSONL schema, PC HTTP protocol, Wi-Fi credentials, or shared token.
- Do not erase `/mnt/spif`; `nuttx.bin` must remain smaller than `0x300000`.
- Do not modify or restore unrelated openvela or contest-workspace changes.
- Do not claim formal model accuracy from live-board observations.

---

## File Map

- `app/agentguard/src/vision_espdl.cpp`: product ESP-DL lifecycle, throttling, and result conversion; expected to need no change.
- `app/agentguard/third_party/human_face_detect/human_face_detect.cpp`: MSR/MNP model wrappers; expected to need no change.
- `app/agentguard/third_party/esp-dl/dl/model/src/dl_model_base.cpp`: NuttX-compatible model minimization; expected to need no change.
- `app/agentguard/src/agentguard_main.c`: camera, LCD, state-machine, log, and network loop; expected to need no change.
- `tools/apply_agentguard_config.sh`: reproducible product Kconfig values; verification input only.
- `AGENTGUARD_PROGRESS.md`: append final evidence and remaining limitations.
- `README.md`: update only if the verified public status changes.

### Task 1: Establish a Reproducible Pre-Build Baseline

**Files:**
- Inspect: `app/agentguard/src/vision_espdl.cpp`
- Inspect: `app/agentguard/third_party/human_face_detect/human_face_detect.cpp`
- Inspect: `/home/yhx/Desktop/openvela/nuttx/.config`
- Inspect: `/home/yhx/Desktop/openvela/nuttx/nuttx`

**Interfaces:**
- Consumes: the approved design and current working tree.
- Produces: a saved baseline showing the source is product code while the old ELF is diagnostic code.

- [ ] **Step 1: Record protected working-tree changes**

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui status --short
repo status
```

Expected: existing contest and openvela changes are visible and retained; no cleanup or reset command is run.

- [ ] **Step 2: Verify current source contains no acceptance stop loop**

Run:

```bash
rg -n 'ag_acceptance|AG_ACCEPTANCE_STOP|espdl_hardware_acceptance_pass' \
  app/agentguard/src app/agentguard/third_party
```

Expected: exit status 1 with no matches.

- [ ] **Step 3: Verify the stale ELF still contains the diagnostic marker**

Run:

```bash
/home/yhx/Desktop/openvela/prebuilts/gcc/linux-x86_64/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-nm \
  -C /home/yhx/Desktop/openvela/nuttx/nuttx | \
  rg 'espdl_hardware_acceptance_pass|ag_acceptance_set_stage'
```

Expected: both diagnostic symbols are present before rebuilding.

- [ ] **Step 4: Verify the product configuration**

Run:

```bash
rg -n '^(CONFIG_AGENTGUARD_ESP_DL=y|CONFIG_XTENSA_CP_INITSET=0x0009|CONFIG_SMP_NCPUS=1|# CONFIG_SMP is not set|# CONFIG_ESP32S3_BLE is not set|CONFIG_ESP32S3_STORAGE_MTD_OFFSET=0x300000)' \
  /home/yhx/Desktop/openvela/nuttx/.config
```

Expected: all six settings are printed with the exact values above.

### Task 2: Run Source-Level Regression Tests

**Files:**
- Test: `app/agentguard/tests/test_core.c`
- Test: `app/agentguard/tests/test_vision.c`
- Test: `app/agentguard/tests/test_storage.c`
- Test: `app/agentguard/tests/test_display_ui.c`
- Test: `monitor/test_agentguard_pc.py`
- Test: `skills/develop-agentguard-openvela/scripts/check_workspace.sh`

**Interfaces:**
- Consumes: current AgentGuard C sources, monitor server, and reusable Skill.
- Produces: host-side regression evidence before firmware replacement.

- [ ] **Step 1: Rebuild and run all strict C tests**

Run:

```bash
make -C app/agentguard/tests clean test
```

Expected: core, vision, storage, and display UI test executables all print `PASS`; compilation uses `-Wall -Wextra -Werror -pedantic`.

- [ ] **Step 2: Run PC-agent unit tests**

Run:

```bash
python3 -m unittest discover -s monitor -p 'test_*.py' -v
```

Expected: four tests pass.

- [ ] **Step 3: Validate the reusable workspace Skill**

Run:

```bash
skills/develop-agentguard-openvela/scripts/check_workspace.sh \
  /home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  /home/yhx/Desktop/openvela
python3 /home/yhx/.codex/skills/.system/skill-creator/scripts/quick_validate.py \
  skills/develop-agentguard-openvela
```

Expected: workspace checks pass and `quick_validate.py` prints `Skill is valid!`.

### Task 3: Produce and Gate a Product Firmware Image

**Files:**
- Build input: `app/agentguard/Makefile`
- Build input: `app/agentguard/Make.defs`
- Build input: all files mapped above
- Generated: `/home/yhx/Desktop/openvela/nuttx/nuttx`
- Generated: `/home/yhx/Desktop/openvela/nuttx/nuttx.bin`

**Interfaces:**
- Consumes: passing Task 2 results and the verified `.config` from Task 1.
- Produces: a product ELF and flash image with no diagnostic acceptance loop.

- [ ] **Step 1: Remove generated build objects while preserving `.config`**

Run:

```bash
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx clean
```

Expected: generated objects and archives are removed; `/home/yhx/Desktop/openvela/nuttx/.config` remains present.

- [ ] **Step 2: Cross-build from current source**

Run:

```bash
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx \
  EXTRAFLAGS='-Wno-cpp -Wno-deprecated-declarations' -j8
```

Expected: output recompiles `vision_espdl.cpp`, `human_face_detect.cpp`, `dl_model_base.cpp`, and the AgentGuard adapter, then reports `LD: nuttx` and `Generated: nuttx.bin`.

- [ ] **Step 3: Enforce the diagnostic-symbol gate**

Run:

```bash
if /home/yhx/Desktop/openvela/prebuilts/gcc/linux-x86_64/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-nm \
  -C /home/yhx/Desktop/openvela/nuttx/nuttx | \
  rg -q 'espdl_hardware_acceptance_pass|ag_acceptance_set_stage|ag_acceptance_stop'; then
  echo 'ERROR: diagnostic acceptance code remains in product ELF' >&2
  exit 1
fi
/home/yhx/Desktop/openvela/prebuilts/gcc/linux-x86_64/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-nm \
  -C /home/yhx/Desktop/openvela/nuttx/nuttx | \
  rg 'agentguard_main|ag_vision_model_process_rgb565'
```

Expected: the first check prints nothing and succeeds; the second prints both product entry symbols.

- [ ] **Step 4: Enforce image size and format gates**

Run:

```bash
test "$(stat -c %s /home/yhx/Desktop/openvela/nuttx/nuttx.bin)" -lt $((0x300000))
source /home/yhx/Desktop/openvela/myenv/bin/activate
esptool --chip esp32s3 image-info /home/yhx/Desktop/openvela/nuttx/nuttx.bin
sha256sum /home/yhx/Desktop/openvela/nuttx/nuttx.bin
```

Expected: size comparison succeeds; image-info identifies ESP32-S3 and a valid checksum; SHA-256 is recorded for the flash report.

### Task 4: Flash Without Touching the LittleFS Partition

**Files:**
- Input: `/home/yhx/Desktop/openvela/nuttx/nuttx.bin`
- Device: `/dev/ttyACM0`
- Preserved: flash range `0x300000..0x3fffff`

**Interfaces:**
- Consumes: the gated product image from Task 3.
- Produces: a board containing the product image with esptool write verification.

- [ ] **Step 1: Resolve the exact USB target and check exclusivity**

Run:

```bash
lsusb
ls -l /dev/ttyACM0
fuser /dev/ttyACM0 2>/dev/null || true
```

Expected: USB ID `303a:1001`, `/dev/ttyACM0` owned by `root:dialout`, and no unexpected process holds the port.

- [ ] **Step 2: Flash the simple-boot product image**

Run:

```bash
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx -j8 flash \
  ESPTOOL_PORT=/dev/ttyACM0 ESPTOOL_BINDIR=./
```

Expected: esptool writes from `0x00000000`, reports `Hash of data verified`, and the written range ends below `0x300000`.

- [ ] **Step 3: Request one physical RESET**

Ask the user to short-press RESET after flashing. Do not treat the automatic USB reset as physical-boot evidence.

### Task 5: Validate Continuous Product Operation on Hardware

**Files:**
- Tool: `tools/hw_serial_probe.py`
- Runtime devices: `/dev/video0`, `/dev/lcd0`, `/dev/buttons`, `/dev/userleds`
- Runtime storage: `/mnt/spif/agentguard-events.jsonl`

**Interfaces:**
- Consumes: the physically reset product firmware from Task 4.
- Produces: serial, visual, process-liveness, and non-destructive storage evidence.

- [ ] **Step 1: Capture startup without issuing another reset**

Run after the user's physical RESET:

```bash
source /home/yhx/Desktop/openvela/myenv/bin/activate
python tools/hw_serial_probe.py --device /dev/ttyACM0 --duration 15
```

Expected: product startup reaches NuttX/AgentGuard without `EXCCAUSE_CP3_DISABLED`, `[BT] HCI`, `hpwork` panic, or a Saved PC in an acceptance loop. If opening USB forces a software reset and hides the physical boot, explicitly label this capture inconclusive and use the known physical-reset interaction for the next commands.

- [ ] **Step 2: Verify device initialization and process liveness**

Send through the serial probe after NSH is available:

```text
ls /dev
pidof agentguard
agentguard stats
```

Expected: camera/LCD/button/LED nodes exist; AgentGuard has one stable PID; statistics read existing LittleFS records with `malformed=0`.

- [ ] **Step 3: Obtain user-confirmed display evidence**

Ask the user to observe the LCD for at least 30 seconds and report whether the camera image and HUD continue changing. Expected: no frozen acceptance screen or static first frame.

- [ ] **Step 4: Exercise directional face-count behavior**

Ask the user to present, in order, no face, one face, and two faces while observing `FACE:n` and the primary box. Expected: counts and primary box change in the correct direction across conditions; exact accuracy is recorded but is not a completion gate for formal model quality.

- [ ] **Step 5: Recheck liveness after the visual exercise**

Run:

```text
pidof agentguard
agentguard stats
```

Expected: the PID matches Step 2, no crash is printed, and storage remains readable.

### Task 6: Record Evidence and Commit the Validated State

**Files:**
- Modify: `AGENTGUARD_PROGRESS.md`
- Modify if status changed: `README.md`

**Interfaces:**
- Consumes: exact command outputs and user observations from Tasks 1–5.
- Produces: an honest handoff record that distinguishes build, flash, runtime, visual, and accuracy evidence.

- [ ] **Step 1: Append the dated validation record**

Add a `2026-08-13 ESP-DL 产品固件真机验收` section to `AGENTGUARD_PROGRESS.md` containing:

- old Saved PC `0x421ba31f` and its acceptance-pass meaning;
- product ELF diagnostic-symbol gate result;
- image byte size and SHA-256;
- esptool write range and hash-verification result;
- physical RESET startup result;
- stable PID observation interval;
- LCD refresh and no-face/one-face/two-face observations;
- LittleFS statistics result;
- remaining accuracy/performance limitations.

- [ ] **Step 2: Update public status only to the level proven**

If Tasks 4–5 pass, update the ESP-DL row in `README.md` from “进行中/待替换” to wording that says the real model runs continuously on hardware, while retaining the explicit statement that formal accuracy and posture-angle validation remain unfinished. If hardware validation fails, leave the public status unchanged and document the exact blocker only in the progress file.

- [ ] **Step 3: Run final documentation and regression checks**

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui diff --check
make -C app/agentguard/tests clean test
python3 -m unittest discover -s monitor -p 'test_*.py' -v
```

Expected: `diff --check` is silent and all host tests pass.

- [ ] **Step 4: Inspect and stage only evidence-bearing project files**

First list the candidate paths:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui status --short
```

Stage `AGENTGUARD_PROGRESS.md`, `README.md` if changed by Step 2, and each exact
source file changed to fix a reproduced failure. Do not stage the broad
`app/agentguard` or `tools` directories because they contain pre-existing
untracked work. Example when validation needs no source correction:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui add \
  AGENTGUARD_PROGRESS.md README.md
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui diff --cached --check
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui diff --cached --stat
```

Expected: the staged diff contains only evidence updates and exact source
corrections made during this plan.

- [ ] **Step 5: Commit the inspected staged diff**

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui commit \
  -m "feat: validate ESP-DL product firmware"
```

Expected: the commit contains only the ESP-DL product source corrections, if
any, and the validated documentation updates.
