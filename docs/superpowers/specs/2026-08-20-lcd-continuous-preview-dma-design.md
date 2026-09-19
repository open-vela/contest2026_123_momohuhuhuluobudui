# LCD Continuous Preview DMA Design

## Goal

Make the 200×150 camera preview's moving scan line visually unnoticeable,
while accepting a reduced display rate of 5–10 FPS. Preserve the existing
80 MHz LCD clock, RGB565 colors, UI layout, camera pipeline, and face-detection
behavior.

## Evidence and hypothesis

The 80 MHz change improved the scan line, but increasing the application DMA
bounce batch from 16 to 32 LCD rows did not: the observed values remained
`D:10-20`, `S:0-10`, and `M:0-10`, and the scan line remained obvious.

The current 200×150 preview is copied and byte-swapped into an internal bounce
buffer, then submitted in about five synchronous `LCDDEVIO_PUTAREA` calls. Each
call programs a new ST7789 address window and waits for its SPI DMA operation.
The selected hypothesis is that these independent transactions and their gaps
make the moving boundary visible. The minimum test is one continuous preview
transaction, without changing the LCD clock or visual content.

The board does not connect the ST7789 TE output, so this change cannot provide
strict vertical synchronization. Its acceptance criterion is instead that the
scan line becomes visually unnoticeable at 5–10 FPS.

## Architecture

Set `CONFIG_ESP32S3_SPI_DMA_BUFSIZE` to 60,000 bytes, exactly the logical storage
needed for 200×150 RGB565 pixels. The ESP32-S3 SPI driver already sizes its GDMA
descriptor arrays from this configuration. At the current 4,095-byte descriptor
limit, the driver will create 15 descriptors and chain the complete preview in
one SPI transaction before waiting once for completion.

The original proposal placed those 60,000 bytes in static internal DRAM. A
pre-implementation boundary check disproved its feasibility: the simple-boot
DRAM region ends at `0x3fcd0000`, while the current image has
`_sheap=0x3fcca68c`, leaving only 22,900 bytes. Growing the existing 15,360-byte
bounce buffer to 60,000 bytes would require another 44,640 bytes and cannot
link safely.

Instead, remove the static internal bounce buffer and allocate a 64-byte-aligned
64 KiB transfer allocation from the common heap during display-worker startup.
At the measured internal boundary this allocation is necessarily served by the
configured PSRAM region. Use only the first 30,000 pixels as the logical DMA
capacity, and verify that the returned address is in the ESP32-S3 external RAM
range before enabling the display worker. Allocation or classification failure
uses the existing startup retry path and never falls back to a smaller,
multi-transaction preview.

Change the AgentGuard bounce helper to accept buffer capacity in pixels rather
than a fixed row count. For each submitted region, it calculates
`capacity / visible_width` rows per batch. Therefore the 200-pixel preview uses
all 30,000 pixels and is submitted once, while wider 240-pixel header or footer
updates safely use at most 125 rows. A capacity smaller than one row is an
error.

One NuttX official source file changes. In
`arch/xtensa/src/esp32s3/esp32s3_spi.c`, `esp32s3_spi_dma_init()` configures the
TX GDMA external-memory block size to 64 bytes immediately after requesting the
channel:

```c
esp32s3_dma_set_ext_memblk(priv->dma_channel, true,
                           ESP32S3_DMA_EXT_MEMBLK_64B);
```

This matches the existing PSRAM framebuffer setup in `esp32s3_lcd.c`, changes
no public interface, and leaves internal-RAM transfers unchanged because the
setting applies to external-memory accesses. This official-source modification
must be an isolated NuttX repository commit with its own build and hardware
evidence. It must not be pushed until openvela's official review accepts it.

## Data flow and pacing

For every display iteration:

1. The display worker snapshots the latest camera frame and UI state.
2. It scales and renders the frame into the existing screen buffer.
3. The preview region is copied into the aligned PSRAM transfer buffer, with
   the existing RGB565 byte swap.
4. One `LCDDEVIO_PUTAREA` call programs the 200×150 window and sends a chained,
   continuous GDMA transaction.
5. Dirty header and footer regions are submitted afterward using the same
   capacity-based helper.
6. The worker sleeps 125 ms after the completed iteration. Rendering and DMA
   time make the resulting rate slightly below 8 FPS, within the accepted
   5–10 FPS range.

The existing LCD write and submit duration statistics remain active. No phase
locking or open-loop vertical synchronization is reintroduced.

## Memory and failure handling

The 15,360-byte static internal bounce buffer is removed. Its replacement is a
64 KiB runtime allocation in PSRAM, of which 60,000 bytes are DMA-visible pixel
data. The GDMA descriptor arrays grow from four to 15 entries per enabled SPI
controller, but this small increase is outweighed by removing the static
bounce allocation. The target link must succeed, and the final map must confirm
that the old bounce symbol is absent, `.dram0.bss` and `_sheap` do not regress,
and the runtime transfer pointer is external and 64-byte aligned.

Compile-time assertions require an even configured byte count and enough logical
capacity for the complete preview. Runtime validation rejects allocation
failure, a non-PSRAM or misaligned transfer pointer, zero capacity, capacity
smaller than one visible row, invalid dimensions, and multiplication or
coordinate bounds already covered by the helper. LCD submission errors retain
the current behavior: invalidate timing data, close the display, and let the
application retry its normal recovery path.

If the target fails to link, the PSRAM allocation fails, or hardware rejects
the PSRAM-backed SPI transfer, implementation stops and reports the measured
boundary. It must not move unrelated stacks or functional state merely to force
this experiment to fit.

## Testing and verification

Host tests will first fail under the old row-count interface, then verify:

- a buffer with exactly `width × height` pixels produces one full-area submit;
- capacity is converted to rows independently for 200- and 240-pixel regions;
- a final partial batch is handled correctly;
- capacity smaller than one row is rejected without submission;
- RGB565 byte swapping, coordinates, error propagation, and timing semantics
  remain unchanged;
- the display worker rejects a missing, non-external, or misaligned transfer
  buffer and frees it on startup failure;
- the configuration script produces a 60,000-byte SPI DMA size and a 125 ms
  refresh interval.

After all AgentGuard host tests, workspace checks, `git diff --check`, and the
target build pass, inspect the image and link map, flash through `/dev/ttyACM0`,
and require esptool's hash verification. Physical acceptance records:

- scan line: invisible, slight, or obvious;
- D/S/M common and maximum values;
- flicker, color, geometry, response, and sustained stability;
- effective display rate within 5–10 FPS.

The existing `NO PERSON` result is a separate ESP-DL numerical issue and is not
an acceptance failure for this display-only experiment.

## Change and review records

Application code, tests, and reproducible configuration changes belong in the
contest repository commits. The `esp32s3_spi_dma_init()` external-memory block
configuration belongs in one separate openvela/NuttX commit. Its change record
must state the affected file and function, absence of public API changes,
PSRAM-TX compatibility effect, build and hardware evidence, and rollback by
reverting that commit. It remains local until official review permits a push.

The uncommitted `AGENTGUARD_PROGRESS.md` handoff log records both repositories'
commit IDs, the official-review requirement, firmware size and hash, memory
measurements, flash result, and physical feedback.
