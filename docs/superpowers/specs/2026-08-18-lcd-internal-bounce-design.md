# LCD Internal DMA Bounce Design

**Date:** 2026-08-18

**Status:** Approved

## Context

Enabling ESP32-S3 SPI DMA removed the polling path but made the physical LCD
show one row of colored lines after another instead of a coherent camera
image. The flashed image passed build, image checksum, write hash, and all host
tests, so the failure is specific to the DMA data source used at runtime.

AgentGuard allocates each 320x240 RGB565 display buffer with
`memalign(32, 153600)`. The target configuration enables
`CONFIG_ESP32S3_SPIRAM_COMMON_HEAP` and adds PSRAM as the second common-heap
region, so these large allocations can reside in external RAM.

The ESP32-S3 SPI DMA lower half passes its TX pointer directly to
`esp32s3_dma_setup()` but never calls `esp32s3_dma_set_ext_memblk()` for its DMA
channel. In contrast, the ESP32-S3 native LCD DMA driver explicitly enables a
64-byte external-memory block before attaching its framebuffer. Cache cleaning
can make CPU writes visible, but it cannot grant a DMA channel external-memory
access. The row-colored corruption is therefore consistent with SPI DMA
reading the PSRAM-backed draw buffer incorrectly.

## Goal

Keep the performance benefit of SPI DMA while ensuring every DMA source buffer
is in internal DRAM, without modifying the NuttX SPI or ST7789 drivers.

Physical success means:

- the camera image, colors, footer, and overlays are coherent;
- no row-colored corruption remains;
- no obvious top-to-bottom or bottom-to-top half-second repaint remains;
- the complete chunked display operation normally reports `D < 100 ms`;
- camera timing remains in its previous broad range.

## Chosen Approach

Add one static, 64-byte-aligned internal-DRAM bounce buffer containing eight
visible LCD rows:

```text
240 pixels × 8 rows × 2 bytes = 3840 bytes
```

The static buffer is linked into internal `.bss` rather than allocated from the
PSRAM-capable common heap. A 3840-byte chunk is below the ESP32-S3 DMA maximum
descriptor payload of 4095 bytes, so each LCD submission fits one configured
SPI DMA descriptor and never splits at an odd byte boundary.

Each 240-row frame is submitted as 30 chunks. For each chunk, the display
worker copies the centered 240-pixel crop from the 320-pixel-stride draw buffer
into the contiguous bounce buffer, then uses the existing coherent transfer
boundary to clean and submit only those internal bytes.

## Alternatives Considered

### Disable SPI DMA

This restores the known coherent display but also restores approximately 1,800
polling transactions and the visible half-second repaint. It is useful only as
an emergency rollback, not as the product fix.

### Modify the ESP32-S3 SPI lower half

The SPI driver could configure its private DMA channel for external-memory TX,
matching the native LCD driver. That modifies the read-only outer NuttX
checkout, changes behavior for every SPI DMA user, and requires broader driver
validation. It is outside the contest repository's ownership boundary.

### Allocate a complete internal 240x240 buffer

A 115200-byte internal buffer would permit one LCD call, but it consumes too
much scarce internal DRAM alongside camera, networking, task stacks, and
ESP-DL. The eight-row buffer provides deterministic DMA-safe memory at modest
cost.

## Components and Data Flow

### Pure crop helper

A small host-testable C helper copies up to eight rows from a strided source
crop into a contiguous destination. It validates pointers and dimensions,
returns the number of rows copied, and handles the final partial chunk without
reading beyond the frame.

The helper has no NuttX or hardware dependencies. Literal host fixtures cover
the horizontal crop, source stride, multiple chunks, final partial rows, and
invalid inputs.

### Internal bounce storage

The product source owns one named static array aligned to 64 bytes. It is used
only by the single display worker, so no mutex or second buffer is required.
The target ELF symbol table must place the array inside the ESP32-S3 internal
DRAM address range; this build-artifact check guards against a future linker or
attribute regression.

### Chunked LCD submission

For output rows `0, 8, 16, ... 232`:

1. Copy up to eight centered visible rows into the contiguous bounce buffer.
2. Set `row_start` and `row_end` to the current output chunk.
3. Set `col_start = 0`, `col_end = 239`, and `stride = 480` bytes.
4. Point `area.data` at the bounce buffer.
5. Clean and submit exactly `copied_rows × 480` bytes through
   `ag_lcd_transfer_run()`.

Any failed copy or LCD submission stops that frame. An LCD submission failure
retains the existing error message and closes the LCD device.

### Display duration

`D` changes from one full-area ioctl duration to the end-to-end duration of the
complete chunked LCD update. The measurement begins immediately before the
first chunk copy and ends after the final successful LCD submission. It
therefore represents the user-visible display update cost, including the
necessary internal copies and 30 driver calls.

The previous completed frame's valid `D` remains the value rendered on the
current frame. A failed clock read, backward clock, copy failure, or LCD
submission invalidates the sample.

## Error Handling

- Invalid crop dimensions or pointers return zero copied rows and invalidate
  the frame's display timing.
- The final chunk may contain fewer than eight rows and submits only its copied
  byte count.
- Integer bounds are checked before calculating row offsets or byte counts.
- A failed LCD ioctl closes the display through the current error path.
- A failed or backward whole-frame clock pair leaves `D` invalid even if all
  chunks were submitted.
- DMA remains enabled because every submitted source now has a verified
  internal-DRAM lifetime and alignment.

## Testing

Implementation follows test-driven development.

Host tests cover:

- exact 240-pixel crop copying from a wider source stride;
- the correct source rows for successive chunks;
- a final chunk containing fewer than eight rows;
- null pointers, zero dimensions, source crops outside the stride, and row
  requests beyond the available frame;
- existing transfer ordering, failure invalidation, timing formatting, and
  real configuration-script behavior.

Target verification covers:

- all host tests and source behavior tests;
- SPI DMA configuration remains `y/2048/64`;
- a successful full NuttX build and valid ESP32-S3 image checksum;
- the named bounce symbol is 64-byte aligned and lies in internal DRAM;
- esptool reports `Hash of data verified` during flashing.

Physical validation records `C/Q/L/D`, confirms coherent pixels and overlays,
and confirms the colored-line failure and visible half-second scan are both
gone.

## Scope Boundaries

This change does not modify outer NuttX driver sources, DMA channel setup,
PSRAM heap configuration, LCD clock, display resolution, rotation, camera
mode, V4L2 buffers, inference cadence, model behavior, policy, networking, or
storage. It does not allocate a full internal framebuffer.
