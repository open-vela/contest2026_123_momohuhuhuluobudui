# LCD DMA RGB565 Byte-Order Correction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore correct red, green, and blue output by preparing big-endian RGB565 bytes in the internal-RAM LCD DMA bounce buffer.

**Architecture:** Keep the existing 8-row internal-RAM bounce and 30 blocking LCD submissions. Change only the bounce copy operation so each host-endian RGB565 word is byte-swapped before the ESP32-S3 DMA path transmits its memory bytes unchanged; validate the transform through the real bounce-controller callback boundary before target build and flash.

**Tech Stack:** C11, host assertions with warnings as errors, NuttX LCD character API, ESP32-S3 SPI DMA, Xtensa ELF tools, NuttX cross-build, esptool.

## Global Constraints

- Correct only RGB565 byte order; do not modify chunk size, SPI DMA threshold, LCD frequency, scheduling, camera, inference, policy, network, or storage behavior.
- Keep `AG_LCD_BOUNCE_ROWS=8`, the 3,840-byte static buffer, and its 64-byte alignment.
- Do not modify outer NuttX ST7789 or ESP32-S3 SPI/DMA source files.
- Do not enable `CONFIG_LCD_ST7789_DATA_ENDIAN_LITTLE`.
- Preserve the existing invalid-input, submission-error, clock-error, crop, chunk, and timing behavior.
- Treat the visible top-to-bottom scan and `D:50-200` as a separate follow-up; do not mix scan optimizations into this correction.
- Preserve unrelated workspace changes and keep `.git.codex-hold` untouched.

---

### Task 1: Tested DMA-Ready RGB565 Bounce Payload

**Files:**
- Modify: `app/agentguard/tests/test_lcd_bounce.c`
- Modify: `app/agentguard/src/lcd_bounce.c`

**Interfaces:**
- Consumes: existing `ag_lcd_bounce_frame(...)` arguments and callbacks without signature changes.
- Produces: each `submit` callback receives `bswap16(source_pixel)` values in the same crop, row, and chunk order as before.
- Preserves: return values and `struct ag_lcd_timing_state` behavior.

- [ ] **Step 1: Change the real callback-boundary test to require DMA-ready words**

In `test_lcd_bounce.c`, change the existing crop expectations to explicit
byte-swapped words:

```c
static const uint16_t first_expected[6] =
  {0x0b00, 0x0c00, 0x0d00, 0x1500, 0x1600, 0x1700};
static const uint16_t second_expected[6] =
  {0x1f00, 0x2000, 0x2100, 0x2900, 0x2a00, 0x2b00};
static const uint16_t final_expected[3] =
  {0x1f00, 0x2000, 0x2100};
```

Add a one-row primary-color case using the existing real
`ag_lcd_bounce_frame` call and submit snapshot:

```c
static const uint16_t primary_source[4] =
  {0xf800, 0x07e0, 0x001f, 0xffff};
static const uint16_t primary_expected[4] =
  {0x00f8, 0xe007, 0x1f00, 0xffff};

test_context_reset(&context, 50, 51);
assert(ag_lcd_bounce_frame(&ops, primary_source, 4, 1, 0, 0, 4, 1,
                           bounce, 1, &timing) == 0);
assert(context.submit_count == 1);
assert(memcmp(context.snapshots[0], primary_expected,
              sizeof(primary_expected)) == 0);
```

The production change that makes this test pass is replacing exact word
copies with per-pixel byte swapping inside `ag_lcd_bounce_frame`.

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```bash
make -C app/agentguard/tests clean test_lcd_bounce
app/agentguard/tests/test_lcd_bounce
```

Expected: compilation succeeds, then the process aborts at the first snapshot
comparison because the current controller submits unswapped words.

- [ ] **Step 3: Implement the minimal byte-order correction**

In the existing row-copy loop in `lcd_bounce.c`, replace `memcpy` with a
column loop.  Do not change validation, chunking, callbacks, or timing:

```c
uint16_t column;

for (column = 0; column < visible_width; column++)
  {
    uint16_t pixel = source[source_offset + column];

    bounce[(size_t)row * visible_width + column] =
      (uint16_t)((pixel << 8) | (pixel >> 8));
  }
```

Remove `<string.h>` from `lcd_bounce.c` after `memcpy` is gone so the file has
no unused dependency.

- [ ] **Step 4: Verify focused GREEN**

Run:

```bash
make -C app/agentguard/tests clean test_lcd_bounce
app/agentguard/tests/test_lcd_bounce
```

Expected: `AgentGuard LCD bounce tests: PASS`.

- [ ] **Step 5: Run the complete host regression**

Run:

```bash
make -C app/agentguard/tests clean test
```

Expected: all twelve compiled tests, the ESP-DL no-stdio check, and the real
LCD DMA configuration check pass with no warnings or errors.

- [ ] **Step 6: Commit the isolated correction**

Stage only the two Task 1 files and commit:

```text
fix: correct RGB565 byte order for LCD DMA
```

---

### Task 2: Target Verification and Physical Color Test

**Files:**
- Verify: `app/agentguard/src/lcd_bounce.c`
- Verify: `/home/yhx/Desktop/openvela/nuttx/.config`
- Verify: `/home/yhx/Desktop/openvela/nuttx/nuttx`
- Verify and flash: `/home/yhx/Desktop/openvela/nuttx/nuttx.bin`

**Interfaces:**
- Consumes: Task 1 byte-swapped bounce payload.
- Produces: a flashed ESP32-S3-EYE diagnostic firmware whose LCD geometry is coherent and whose red, green, and blue channels are physically correct.

- [ ] **Step 1: Revalidate workspace and generated target configuration**

Run:

```bash
skills/develop-agentguard-openvela/scripts/check_workspace.sh \
  /home/yhx/Desktop/openvela/contest2026_123_momohuhuhuluobudui \
  /home/yhx/Desktop/openvela
tools/apply_agentguard_config.sh
rg -n "CONFIG_ESP32S3_SPI_DMA|CONFIG_ESP32S3_SPI_DMA_BUFSIZE|CONFIG_ESP32S3_SPI_DMATHRESHOLD|CONFIG_LCD_ST7789_DATA_ENDIAN_LITTLE" \
  /home/yhx/Desktop/openvela/nuttx/.config
```

Expected: workspace checks pass; DMA is enabled; buffer size is `2048`;
threshold is `64`; ST7789 little-endian data is not enabled.

- [ ] **Step 2: Build the target firmware**

Run:

```bash
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx -j8
```

Expected: `lcd_bounce.c` compiles, `nuttx` links, and `nuttx.bin` is generated
without errors.

- [ ] **Step 3: Recheck internal bounce placement and image integrity**

Run the activated Xtensa `nm` on `nuttx` and locate
`g_agentguard_lcd_bounce`.  Expected size is `0x0f00`, address is inside
internal DRAM `[0x3fc88000, 0x3fcd0000)`, and the address is divisible by 64.

Run:

```bash
sha256sum /home/yhx/Desktop/openvela/nuttx/nuttx.bin
esptool.py --chip esp32s3 image_info /home/yhx/Desktop/openvela/nuttx/nuttx.bin
```

Expected: a SHA-256 is emitted and esptool reports a valid ESP32-S3 image and
valid checksum.

- [ ] **Step 4: Flash and verify the device write**

Run:

```bash
source /home/yhx/Desktop/openvela/myenv/bin/activate
make -C /home/yhx/Desktop/openvela/nuttx -j8 flash \
  ESPTOOL_PORT=/dev/ttyACM0 ESPTOOL_BINDIR=./
```

Expected: esptool reports `Hash of data verified` and resets the device.
Recompute `nuttx.bin` SHA-256 after the flash target's rebuild.

- [ ] **Step 5: Obtain physical single-variable evidence**

Ask the user to report:

1. whether colored horizontal corruption remains absent;
2. whether red content has returned;
3. whether camera and UI colors now look normal;
4. whether geometry remains coherent;
5. current `C`, `Q`, `L`, and `D` ranges.

Do not claim the color issue fixed until these observations pass.  Record the
top-to-bottom scan unchanged as evidence for the separate full-frame transfer
optimization, not as failure of this byte-order task.
