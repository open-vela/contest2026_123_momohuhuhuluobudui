# LCD Internal DMA Bounce Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace direct SPI DMA reads from the PSRAM-capable draw buffer with 30 coherent eight-row transfers from a verified internal-DRAM bounce buffer.

**Architecture:** A host-testable `lcd_bounce` controller validates the crop, copies each strided source chunk into caller-owned contiguous storage, submits it through a callback, and measures the complete chunked frame duration. The NuttX adapter owns one 64-byte-aligned 3840-byte static buffer, maps each callback to the existing coherent LCD transfer boundary, and leaves SPI DMA enabled. The target ELF symbol table verifies the bounce storage is internal before flashing.

**Tech Stack:** C11, callback-based hardware boundary tests, NuttX LCD character API, ESP32-S3 SPI DMA, Xtensa ELF tools, host Make tests with warnings as errors, NuttX cross-build, esptool.

## Global Constraints

- Do not modify outer NuttX ST7789, SPI, DMA, allocator, or linker sources.
- Keep `CONFIG_ESP32S3_SPI_DMA=y`, buffer size `2048`, threshold `64`, and LCD clock `40 MHz`.
- Submit DMA only from a static, 64-byte-aligned internal-DRAM buffer.
- Use exactly eight rows: `240 × 8 × 2 = 3840` bytes, below the 4095-byte DMA descriptor maximum.
- Keep the 320x240 PSRAM-capable draw buffers, centered 240-pixel crop, rotation, camera mode, inference, policy, network, and storage behavior unchanged.
- Make `D` represent the end-to-end duration of all chunk copies and LCD submissions for one complete displayed frame.
- Invalidate `D` on an invalid crop, copy failure, failed LCD submission, failed clock read, or backward clock.
- Preserve unrelated workspace changes and keep `.git.codex-hold` untouched.

---

### Task 1: Tested Chunked Bounce Controller

**Files:**
- Create: `app/agentguard/include/agentguard/lcd_bounce.h`
- Create: `app/agentguard/src/lcd_bounce.c`
- Create: `app/agentguard/tests/test_lcd_bounce.c`
- Modify: `app/agentguard/tests/Makefile`
- Modify: `app/agentguard/Makefile`

**Interfaces:**
- Produces: `struct ag_lcd_bounce_ops` with `read_ms`, `submit`, and opaque `context` members.
- Produces: `int ag_lcd_bounce_frame(const struct ag_lcd_bounce_ops *ops, const uint16_t *source, uint16_t source_stride, uint16_t source_height, uint16_t source_x, uint16_t source_y, uint16_t visible_width, uint16_t visible_height, uint16_t *bounce, uint16_t bounce_rows, struct ag_lcd_timing_state *timing)`.
- Callback: `int submit(void *context, const uint16_t *pixels, uint16_t first_row, uint16_t row_count, uint16_t width)`.
- Returns: `0` after all chunks submit; the first negative submit result unchanged; `-1` for invalid inputs.

- [ ] **Step 1: Write the failing real-data controller test**

Create a 6x5 source fixture whose literal values are:

```c
static const uint16_t source[30] =
{
   0,  1,  2,  3,  4,  5,
  10, 11, 12, 13, 14, 15,
  20, 21, 22, 23, 24, 25,
  30, 31, 32, 33, 34, 35,
  40, 41, 42, 43, 44, 45
};
```

Use a fake submit callback that records metadata and copies the submitted
pixels into two independent snapshots. Call the wished-for API with stride 6,
source crop `(x=1, y=1)`, visible size `3x4`, and a two-row bounce buffer.
With fake clocks `100` and `135`, assert:

```c
assert(result == 0);
assert(context.submit_count == 2);
assert(context.first_rows[0] == 0 && context.row_counts[0] == 2);
assert(context.first_rows[1] == 2 && context.row_counts[1] == 2);
assert(context.widths[0] == 3 && context.widths[1] == 3);
assert(memcmp(context.snapshots[0],
              (uint16_t[]){11, 12, 13, 21, 22, 23}, 12) == 0);
assert(memcmp(context.snapshots[1],
              (uint16_t[]){31, 32, 33, 41, 42, 43}, 12) == 0);
assert(timing.valid && timing.write_ms == 35);
```

Run a `3x3` crop with two bounce rows and assert chunk row counts `2, 1` and
the last snapshot contains only the final three pixels. Make the second submit
return `-7` and assert the function returns `-7`, stops after two calls, and
invalidates timing. Add invalid-input cases for null pointers, zero width,
zero bounce rows, `source_x + visible_width > source_stride`, and
`source_y + visible_height > source_height`; each returns `-1`, performs no
submit, and invalidates timing.

Add `test_lcd_bounce` to `TARGETS`, compile it from `lcd_bounce.c` and
`lcd_timing.c`, and execute it after `test_lcd_transfer`. Add
`src/lcd_bounce.c` to product `CSRCS`.

- [ ] **Step 2: Run the focused test to verify RED**

Run: `make -C app/agentguard/tests test_lcd_bounce`

Expected: FAIL because `agentguard/lcd_bounce.h` and its source do not exist.

- [ ] **Step 3: Implement the minimal controller**

Validate all callbacks, pointers, nonzero dimensions, and crop bounds using
subtraction comparisons so 16-bit addition cannot wrap:

```c
visible_width > source_stride - source_x
visible_height > source_height - source_y
```

Read the whole-frame start clock, then loop `first_row` from zero to
`visible_height`. For each iteration, choose
`row_count = min(bounce_rows, visible_height - first_row)`, copy each row with:

```c
memcpy(bounce + row * visible_width,
       source + (source_y + first_row + row) * source_stride + source_x,
       visible_width * sizeof(*bounce));
```

Call `submit()` with the contiguous chunk. On a negative result, invalidate
and return it immediately. After the final chunk, read the finish clock and
update the existing timing state only when both reads succeeded; otherwise
invalidate timing while returning `0` because the hardware submissions did
succeed.

- [ ] **Step 4: Verify GREEN and regression**

Run: `make -C app/agentguard/tests clean test_lcd_bounce`

Expected: build succeeds.

Run: `app/agentguard/tests/test_lcd_bounce`

Expected: `AgentGuard LCD bounce tests: PASS`.

Run: `make -C app/agentguard/tests test`

Expected: all twelve compiled tests plus no-stdio and real DMA configuration
behavior tests pass.

- [ ] **Step 5: Commit the controller**

Stage only the five Task 1 files and commit:

```text
feat: add chunked LCD bounce controller
```

---

### Task 2: Internal Buffer and NuttX Display Adapter

**Files:**
- Modify: `app/agentguard/src/agentguard_main.c`

**Interfaces:**
- Consumes: Task 1 `ag_lcd_bounce_frame()`.
- Reuses: `ag_lcd_transfer_run()` for each internal chunk's cache clean and blocking LCD ioctl.
- Produces: local ELF symbol `g_agentguard_lcd_bounce`, exactly 3840 bytes and aligned to 64 bytes.

- [ ] **Step 1: Confirm all tested boundaries are GREEN**

Run: `make -C app/agentguard/tests clean test`

Expected: twelve compiled tests and both Python behavior tests pass. The main
integration below contains only hardware adapters into those tested
boundaries; physical behavior and ELF placement are verified in Task 3.

- [ ] **Step 2: Add fixed internal storage and constants**

Add:

```c
#define AG_LCD_VISIBLE_WIDTH 240
#define AG_LCD_BOUNCE_ROWS 8
#define AG_LCD_BOUNCE_PIXELS \
  (AG_LCD_VISIBLE_WIDTH * AG_LCD_BOUNCE_ROWS)

static uint16_t g_agentguard_lcd_bounce[AG_LCD_BOUNCE_PIXELS]
  __attribute__((aligned(64)));
```

Include `agentguard/lcd_bounce.h`. Do not allocate this buffer with `malloc`,
`memalign`, or the common heap.

- [ ] **Step 3: Adapt each chunk to the existing coherent transfer**

Add a bounce submit callback that receives pixels, first row, row count, and
width. It fills the existing `lcddev_area_s` as:

```c
area.row_start = first_row;
area.row_end = first_row + row_count - 1;
area.col_start = 0;
area.col_end = width - 1;
area.stride = width * sizeof(*pixels);
area.data = (uint8_t *)pixels;
```

Create/reset a temporary per-chunk timing state, construct the current
`ag_lcd_transfer_ops`, and call:

```c
ag_lcd_transfer_run(&transfer_ops, (uintptr_t)pixels,
                    row_count * width * sizeof(*pixels), &chunk_timing);
```

The outer bounce controller owns the user-visible whole-frame `D`; the
temporary chunk timing is not published.

- [ ] **Step 4: Replace direct PSRAM submission**

In `ag_display_frame()`, retain the current visible-size and centered-source
calculations, cap visible width at `AG_LCD_VISIBLE_WIDTH`, construct
`ag_lcd_bounce_ops`, and replace the single direct transfer with:

```c
result = ag_lcd_bounce_frame(&bounce_ops, pixels, width, height,
                             source_x, source_y,
                             visible_width, visible_height,
                             g_agentguard_lcd_bounce,
                             AG_LCD_BOUNCE_ROWS, lcd_timing);
```

On a negative result, preserve the existing `LCD preview stopped` message and
close behavior. The display worker's previous-frame `D` publication remains
unchanged.

- [ ] **Step 5: Run host regression and changed-file hygiene**

Run: `make -C app/agentguard/tests clean test`

Expected: all twelve compiled tests and both Python tests pass.

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  diff --check -- app/agentguard/src/agentguard_main.c
```

Expected: no output.

- [ ] **Step 6: Commit the NuttX adapter**

Stage only `app/agentguard/src/agentguard_main.c` and commit:

```text
fix: bounce LCD DMA through internal RAM
```

---

### Task 3: Target Placement, Build, Flash, and Physical Validation

**Files:**
- Verify only: `/home/yhx/Desktop/openvela/nuttx/.config`
- Verify only: `/home/yhx/Desktop/openvela/nuttx/nuttx`
- Verify only: `/home/yhx/Desktop/openvela/nuttx/nuttx.bin`

**Interfaces:**
- Consumes: committed controller and NuttX adapter.
- Produces: verified internal symbol placement, hash-verified firmware, and physical acceptance evidence.

- [ ] **Step 1: Reapply and verify product configuration**

Run:

```bash
skills/develop-agentguard-openvela/scripts/check_workspace.sh \
  /home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  /home/yhx/Desktop/openvela
tools/apply_agentguard_config.sh \
  /home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  /home/yhx/Desktop/openvela
rg -n '^CONFIG_ESP32S3_SPI_DMA=y$|^CONFIG_ESP32S3_SPI_DMA_BUFSIZE=2048$|^CONFIG_ESP32S3_SPI_DMATHRESHOLD=64$' \
  /home/yhx/Desktop/openvela/nuttx/.config
```

Expected: workspace checks pass, configuration application succeeds, and the
three exact DMA values are printed.

- [ ] **Step 2: Run fresh host and target builds**

Run: `make -C app/agentguard/tests clean test`

Expected: every host and behavior test passes.

Activate `/home/yhx/Desktop/openvela/myenv/bin/activate`, then run:

```bash
make -C /home/yhx/Desktop/openvela/nuttx -j8
```

Expected: exit 0 and fresh `nuttx`/`nuttx.bin` outputs.

- [ ] **Step 3: Verify bounce symbol size, alignment, and address**

Run:

```bash
xtensa-esp32s3-elf-nm -S --radix=x \
  /home/yhx/Desktop/openvela/nuttx/nuttx | \
  rg 'g_agentguard_lcd_bounce$'
```

Expected: symbol size is `00000f00`; its address is divisible by `0x40` and is
within the simple-boot internal DRAM range `[0x3fc88000, 0x3fcd0000)`. Reject
the image if the symbol is absent, in the external `0x3c...` range, incorrectly
sized, or misaligned.

- [ ] **Step 4: Verify the final image and repository hygiene**

Run:

```bash
sha256sum /home/yhx/Desktop/openvela/nuttx/nuttx.bin
/home/yhx/Desktop/openvela/myenv/bin/esptool --chip esp32s3 image-info \
  /home/yhx/Desktop/openvela/nuttx/nuttx.bin
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  diff --check
```

Expected: record the SHA-256; esptool reports an ESP32-S3 image with a valid
checksum; the diff check produces no output.

- [ ] **Step 5: Flash with write verification**

With `/dev/ttyACM0` present and free, activate the Python environment and run:

```bash
make -C /home/yhx/Desktop/openvela/nuttx -j8 flash \
  ESPTOOL_PORT=/dev/ttyACM0 ESPTOOL_BINDIR=./
```

Expected: esptool reports `Hash of data verified` and resets the board.

- [ ] **Step 6: Obtain physical acceptance evidence**

Record `C/Q/L/D` ranges and confirm:

```text
camera pixels, colors, footer, and overlays are coherent
row-colored corruption is gone
no obvious half-second edge-to-edge repaint remains
D is normally below 100 ms
C/Q/L remain in their previous broad range
```

If pixels remain corrupt, stop and capture the exact pattern rather than
changing DMA or buffer size without a new diagnosis. If pixels are coherent
but `D >= 100 ms`, record the range before considering a different chunk size.

- [ ] **Step 7: Record final repository state**

Run:

```bash
git --git-dir=/home/yhx/Desktop/openvela/.repo/projects/contest2026_123_momohuhuhuluobudui.git \
  --work-tree=/home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  status --short
```

Confirm the feature files are committed and only pre-existing unrelated
changes/build artifacts remain.
