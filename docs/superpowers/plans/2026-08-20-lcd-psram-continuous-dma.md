# LCD PSRAM Continuous DMA Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Send each 200×150 RGB565 camera preview from PSRAM as one continuous 80 MHz GDMA transaction at an effective 5–10 FPS.

**Architecture:** Remove the 15,360-byte static internal bounce buffer and allocate a 64-byte-aligned 64 KiB PSRAM transfer buffer whose logical capacity is 60,000 bytes. Extend the existing capacity-based copy path so a full preview becomes one `PUTAREA`, and configure ESP32-S3 SPI TX GDMA for 64-byte external-memory blocks exactly as the existing ESP32-S3 LCD_CAM driver does. Keep the NuttX driver change in a separate local commit that must receive official review before push.

**Tech Stack:** Apache NuttX/openvela C11, ESP32-S3 SPI2/GDMA, PSRAM, ST7789, Python source/configuration regression tests, host C unit tests.

**Spec:** `docs/superpowers/specs/2026-08-20-lcd-continuous-preview-dma-design.md`

## Global Constraints

- Preserve `CONFIG_LCD_ST7789_FREQUENCY=80000000`, preview geometry, RGB565 order, UI, camera, and face-detection behavior.
- Configure 60,000 logical DMA bytes and `AG_DISPLAY_REFRESH_US=125000`.
- The transfer allocation is 65,536 bytes, 64-byte aligned, and must classify as PSRAM before the display thread starts.
- Keep at least the currently measured 22,900-byte simple-boot internal DRAM link headroom; removing the static bounce buffer should improve it.
- The only openvela/NuttX source change is in `esp32s3_spi_dma_init()` and changes no public API.
- Keep the NuttX commit local until official review permits push.
- Keep `AGENTGUARD_PROGRESS.md` uncommitted and record both repositories' commits.
- Work in the current `dev-ai-contest-2026` workspace because required project
  sources are untracked and would be absent from a new worktree; stage only
  explicitly named task files.
- The user approved a TDD exception for the single NuttX GDMA register
  configuration. Validate it by target build, isolated diff review, and real
  PSRAM DMA hardware behavior; do not substitute a source-text change detector.

---

### Task 1: Establish the in-place baseline

**Files:**
- Inspect only: contest and NuttX repository status

**Interfaces:**
- Produces: passing baseline tests and an exact record of pre-existing changes.

- [ ] **Step 1: Verify repository boundaries**

Run status for both repositories and confirm the contest branch is
`dev-ai-contest-2026`, the NuttX SPI target file is clean, and the contest index
contains no staged changes.

- [ ] **Step 2: Run the complete host baseline**

Run `make -C app/agentguard/tests clean test`.

Expected: all existing tests pass before production changes.

### Task 2: Configure SPI TX GDMA for PSRAM in official NuttX source

**Files:**
- Modify in openvela/NuttX: `arch/xtensa/src/esp32s3/esp32s3_spi.c`

**Interfaces:**
- Consumes: existing `esp32s3_dma_set_ext_memblk()` and `ESP32S3_DMA_EXT_MEMBLK_64B` from the private ESP32-S3 DMA layer.
- Produces: SPI TX GDMA channels configured for 64-byte external-memory blocks; no public API or Kconfig change.

- [ ] **Step 1: Add the minimal driver configuration**

Immediately after the successful `esp32s3_dma_request()` failure check in
`esp32s3_spi_dma_init()`, add:

```c
  /* Configure aligned external-memory reads for PSRAM TX buffers. */

  esp32s3_dma_set_ext_memblk(priv->dma_channel, true,
                             ESP32S3_DMA_EXT_MEMBLK_64B);
```

- [ ] **Step 2: Verify the official-source diff is isolated**

Run:

```bash
git -C /home/yhx/Desktop/openvela/nuttx diff --check -- arch/xtensa/src/esp32s3/esp32s3_spi.c
git -C /home/yhx/Desktop/openvela/nuttx diff --stat -- arch/xtensa/src/esp32s3/esp32s3_spi.c
git -C /home/yhx/Desktop/openvela/nuttx status --short -- arch/xtensa/src/esp32s3/esp32s3_spi.c
```

Expected: exactly one modified official file and no whitespace error.

- [ ] **Step 3: Commit the official patch locally and do not push**

```bash
git -C /home/yhx/Desktop/openvela/nuttx add arch/xtensa/src/esp32s3/esp32s3_spi.c
git -C /home/yhx/Desktop/openvela/nuttx diff --cached --check
git -C /home/yhx/Desktop/openvela/nuttx commit -m "esp32s3/spi: configure PSRAM TX DMA block size"
```

Record the resulting commit. Do not run `git push`.

### Task 3: Add a testable PSRAM transfer-buffer boundary

**Files:**
- Create: `app/agentguard/include/agentguard/lcd_dma_buffer.h`
- Create: `app/agentguard/src/lcd_dma_buffer.c`
- Create: `app/agentguard/tests/test_lcd_dma_buffer.c`
- Modify: `app/agentguard/tests/Makefile`
- Modify: `app/agentguard/Makefile`
- Modify: `app/agentguard/CMakeLists.txt`

**Interfaces:**
- Produces: `ag_lcd_dma_buffer_init()` and `ag_lcd_dma_buffer_release()` using injected allocate, release, and external-address callbacks.

- [ ] **Step 1: Write failing lifecycle tests**

Define the public contract:

```c
struct ag_lcd_dma_buffer
{
  uint16_t *pixels;
  size_t pixel_capacity;
};

struct ag_lcd_dma_buffer_ops
{
  void *(*allocate)(void *context, size_t alignment, size_t bytes);
  void (*release)(void *context, void *memory);
  bool (*is_external)(void *context, const void *memory);
  void *context;
};

bool ag_lcd_dma_buffer_init(struct ag_lcd_dma_buffer *buffer,
                            const struct ag_lcd_dma_buffer_ops *ops,
                            size_t alignment, size_t allocation_bytes,
                            size_t usable_bytes);
void ag_lcd_dma_buffer_release(struct ag_lcd_dma_buffer *buffer,
                               const struct ag_lcd_dma_buffer_ops *ops);
```

In `test_lcd_dma_buffer.c`, use fake callbacks and assert:

- null callbacks, zero/non-power-of-two alignment, odd usable bytes, and
  `usable_bytes > allocation_bytes` fail without allocating;
- allocation failure leaves `{NULL, 0}`;
- a misaligned pointer is released once and rejected;
- an internal pointer is released once and rejected;
- an aligned external pointer succeeds with `pixel_capacity=usable_bytes/2`;
- release frees exactly once and clears both fields.

Add a Makefile target compiling the new test with `src/lcd_dma_buffer.c`, add it
to `TARGETS`, run it in `test`, and add the new source to both application build
lists.

- [ ] **Step 2: Run the focused test and verify RED**

Run `make -C app/agentguard/tests clean test_lcd_dma_buffer`.

Expected: build failure because the header and implementation do not exist.

- [ ] **Step 3: Implement the minimal lifecycle helper**

Validate all arguments before allocation. After allocation, require
`(uintptr_t)memory % alignment == 0` and `ops->is_external(...) == true`; call
`release` on either failure. On success assign the pointer and
`usable_bytes / sizeof(uint16_t)`. Release only a non-null pointer, then clear
the structure.

- [ ] **Step 4: Run the focused test and verify GREEN**

Run:

```bash
make -C app/agentguard/tests clean test_lcd_dma_buffer
app/agentguard/tests/test_lcd_dma_buffer
```

Expected: `AgentGuard LCD DMA buffer tests: PASS`.

- [ ] **Step 5: Commit the isolated buffer boundary**

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui add app/agentguard/include/agentguard/lcd_dma_buffer.h app/agentguard/src/lcd_dma_buffer.c app/agentguard/tests/test_lcd_dma_buffer.c app/agentguard/tests/Makefile app/agentguard/Makefile app/agentguard/CMakeLists.txt
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui diff --cached --check
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui commit -m "feat: validate LCD PSRAM DMA buffer"
```

### Task 4: Make bounce batching capacity-based

**Files:**
- Modify: `app/agentguard/include/agentguard/lcd_bounce.h`
- Modify: `app/agentguard/src/lcd_bounce.c`
- Modify: `app/agentguard/tests/test_lcd_bounce.c`

**Interfaces:**
- Changes the final bounce argument from `uint16_t bounce_rows` to `size_t bounce_pixel_capacity` and derives rows with `capacity / visible_width`.

- [ ] **Step 1: Convert tests to pixel capacities and add exact geometry cases**

Change existing calls so a three-pixel by two-row buffer passes `6`, and the
four-pixel primary buffer passes `4`. Add metadata-only submit support plus
static 240×240 source and 200×150 bounce arrays. Verify:

```c
/* Exact preview capacity: one submit. */
assert(ag_lcd_bounce_area(&ops, source240, 240, 240,
                          20, 45, 20, 45, 200, 150,
                          bounce200x150, 200 * 150, &timing) == 0);
assert(context.submit_count == 1);
assert(context.widths[0] == 200 && context.row_counts[0] == 150);

/* Same capacity at LCD width: 125 rows plus one row. */
assert(ag_lcd_bounce_area(&ops, source240, 240, 240,
                          0, 0, 0, 0, 240, 126,
                          bounce200x150, 200 * 150, &timing) == 0);
assert(context.submit_count == 2);
assert(context.row_counts[0] == 125 && context.row_counts[1] == 1);
```

Verify capacity `2` is rejected for a three-pixel-wide region.

- [ ] **Step 2: Run the focused test and verify RED**

Run `make -C app/agentguard/tests clean test_lcd_bounce && app/agentguard/tests/test_lcd_bounce`.

Expected: assertion failure because capacities are still interpreted as rows.

- [ ] **Step 3: Implement capacity-derived rows**

Add `<stddef.h>` to the header, change the parameter type/name, reject
`bounce_pixel_capacity < visible_width`, then calculate:

```c
size_t row_capacity = bounce_pixel_capacity / visible_width;
```

For each loop iteration use:

```c
uint16_t row_count = remaining < row_capacity
                       ? remaining
                       : (uint16_t)row_capacity;
```

Keep all existing coordinate, byte-swap, error, and timing behavior unchanged.

- [ ] **Step 4: Run the focused test and verify GREEN**

Run `make -C app/agentguard/tests clean test_lcd_bounce && app/agentguard/tests/test_lcd_bounce`.

Expected: `AgentGuard LCD bounce tests: PASS`.

- [ ] **Step 5: Commit the helper change**

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui add app/agentguard/include/agentguard/lcd_bounce.h app/agentguard/src/lcd_bounce.c app/agentguard/tests/test_lcd_bounce.c
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui diff --cached --check
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui commit -m "refactor: size LCD bounce batches by capacity"
```

### Task 5: Wire the PSRAM buffer and continuous preview into AgentGuard

**Files:**
- Modify: `app/agentguard/src/agentguard_main.c`
- Modify: `app/agentguard/tests/test_lcd_dma_config.py`
- Modify: `tools/apply_agentguard_config.sh`

**Interfaces:**
- Consumes: Tasks 2–4.
- Produces: 64 KiB aligned PSRAM allocation, 30,000 logical pixels, one 200×150 `PUTAREA`, and 125 ms pacing.

- [ ] **Step 1: Write failing configuration/source assertions**

In `test_lcd_dma_config.py`, expect DMA size `60000`, read
`agentguard_main.c`, and assert it contains:

```text
#define AG_LCD_DMA_ALLOCATION_BYTES (64 * 1024)
#define AG_DISPLAY_REFRESH_US 125000
```

Also assert the old declaration text `static uint16_t g_agentguard_lcd_bounce`
is absent.

- [ ] **Step 2: Run the configuration test and verify RED**

Run `python3 app/agentguard/tests/test_lcd_dma_config.py`.

Expected: generated DMA size remains `15360` and the old refresh/buffer source
policy fails.

- [ ] **Step 3: Wire allocation, validation, and cleanup**

In `agentguard_main.c`:

- include `agentguard/lcd_dma_buffer.h`;
- add `struct ag_lcd_dma_buffer lcd_dma` to `ag_display_worker`;
- define 64 KiB allocation, 60,000-byte logical capacity, 64-byte alignment,
  and 125,000 µs delay;
- remove `g_agentguard_lcd_bounce` and `AG_LCD_BOUNCE_ROWS`;
- provide allocation/release wrappers around `memalign()`/`free()` and an
  external-address callback accepting `[0x3c000000, 0x3e000000)`;
- initialize the DMA buffer before creating the display thread and release it
  on every startup failure after allocation;
- add the buffer pointer/capacity to `ag_lcd_submit_context` and pass them to
  `ag_lcd_bounce_area()`;
- assert configured DMA bytes are even and at least `200 * 150 * 2`, and the
  64 KiB allocation is at least the configured byte count.

In `apply_agentguard_config.sh`, set
`ESP32S3_SPI_DMA_BUFSIZE 60000` and explain the 15-descriptor full-preview
chain in the adjacent comment.

- [ ] **Step 4: Run focused tests and verify GREEN**

Run:

```bash
make -C app/agentguard/tests clean test_lcd_dma_buffer test_lcd_bounce
app/agentguard/tests/test_lcd_dma_buffer
app/agentguard/tests/test_lcd_bounce
python3 app/agentguard/tests/test_lcd_dma_config.py
```

Expected: the LCD DMA buffer, LCD bounce, and LCD DMA configuration tests all
print PASS.

- [ ] **Step 5: Commit the application experiment**

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui add app/agentguard/src/agentguard_main.c app/agentguard/tests/test_lcd_dma_config.py tools/apply_agentguard_config.sh
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui diff --cached --check
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui commit -m "perf: send LCD preview from PSRAM DMA"
```

### Task 6: Build, verify, flash, and record

**Files:**
- Modify generated configuration: `/home/yhx/Desktop/openvela/nuttx/.config`
- Modify but do not commit: `AGENTGUARD_PROGRESS.md`

**Interfaces:**
- Produces: tested firmware on `/dev/ttyACM0`, two-repository change record, and physical acceptance request.

- [ ] **Step 1: Run all host and workspace tests**

Run:

```bash
make -C app/agentguard/tests clean test
skills/develop-agentguard-openvela/scripts/check_workspace.sh /home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui /home/yhx/Desktop/openvela
```

Expected: all checks pass.

- [ ] **Step 2: Apply config and build the target**

Run:

```bash
tools/apply_agentguard_config.sh
rg -n '^CONFIG_(ESP32S3_SPI_DMA_BUFSIZE|LCD_ST7789_FREQUENCY)=' /home/yhx/Desktop/openvela/nuttx/.config
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx -j8
```

Expected values are `60000` and `80000000`; target build succeeds.

- [ ] **Step 3: Prove the internal-memory result and image validity**

Run:

```bash
source /home/yhx/Desktop/openvela/myenv/bin/activate
xtensa-esp32s3-elf-nm -S --size-sort /home/yhx/Desktop/openvela/nuttx/nuttx | rg 'g_agentguard_lcd_bounce|esp32s3_spi[23]_dma_(tx|rx)desc'
xtensa-esp32s3-elf-nm -n /home/yhx/Desktop/openvela/nuttx/nuttx | rg ' (_ebss|_sheap)$'
xtensa-esp32s3-elf-size -A /home/yhx/Desktop/openvela/nuttx/nuttx | rg 'dram0.bss|dram0.data'
sha256sum /home/yhx/Desktop/openvela/nuttx/nuttx.bin
python -m esptool --chip esp32s3 image-info /home/yhx/Desktop/openvela/nuttx/nuttx.bin
```

Require:

- no `g_agentguard_lcd_bounce` symbol;
- SPI2/SPI3 descriptor arrays reflect 15 descriptors;
- `_sheap` is no higher than the previous `0x3fcca68c` baseline;
- valid ESP32-S3 image information;
- recorded image byte size and SHA-256;
- clean `git diff --check` in both repositories.

- [ ] **Step 4: Review both repository diffs before flashing**

Contest commits may contain only the tests, buffer helper, bounce API, app
wiring, configuration, and documents described above. The NuttX commit may
contain only `arch/xtensa/src/esp32s3/esp32s3_spi.c`. Confirm no push occurred.

- [ ] **Step 5: Flash and verify**

Confirm `/dev/ttyACM0` is present and unused, then run:

```bash
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx -j8 flash ESPTOOL_PORT=/dev/ttyACM0 ESPTOOL_BINDIR=./
```

Require `Hash of data verified`; do not open serial afterward because it can
reset the board into simple-boot mode.

- [ ] **Step 6: Record the handoff and request physical acceptance**

Append to `AGENTGUARD_PROGRESS.md`: both commit IDs; explicit “NuttX commit is
local and requires official review before push”; TDD RED/GREEN evidence; all
test/build results; DMA bytes/descriptors; `_sheap`; image size/hash; flash
verification; and rollback commands based on reverting each isolated commit.

Ask the user for:

```text
扫描线：不可见 / 轻微 / 明显
D/S/M：常见值与最高值
闪烁：无 / 轻微 / 明显
颜色、几何、响应、稳定性：正常或具体异常
主观帧率：可接受 / 太低
```

Do not claim physical success before this feedback.
