# LCD SPI DMA Design

**Date:** 2026-08-18

**Status:** Approved

## Context

AgentGuard's live camera screen does not hold a completely static image for
half a second. The displayed content can move, but each replacement frame is
visibly painted from one edge of the LCD to the other over roughly 0.5
seconds. The direction appears as either bottom-to-top or top-to-bottom
depending on the changed content.

The on-device timing diagnostics report:

```text
C:70-160 Q:20-100 L:60-110 AI:60
```

These measurements show that camera completion, dequeue, the successful main
loop, and inference all progress substantially faster than the visible LCD
scan. The bottleneck is therefore after frame publication, in the LCD transfer
path rather than in camera capture or AI inference.

Static inspection explains the gap. The ESP32-S3-EYE ST7789 is configured for
40 MHz SPI, but `CONFIG_ESP32S3_SPI_DMA` is disabled. AgentGuard submits a
240x240 RGB565 visible area from a 320-pixel-stride draw buffer. ST7789 sends
the area one 480-byte row at a time. Without DMA, the ESP32-S3 SPI lower half
splits every row into 64-byte polling transactions, resulting in approximately
1,800 transactions for one 115,200-byte frame. The payload's theoretical wire
time is about 23 ms; per-transaction polling overhead accounts for the much
longer visible scan.

## Goal

Use the existing ESP32-S3 SPI DMA path to remove the visible full-screen scan
while preserving AgentGuard's current camera, inference, rotation, overlay,
and display geometry behavior.

Success on the physical board means:

- no obvious top-to-bottom or bottom-to-top repaint;
- live motion is substantially smoother;
- a full `LCDDEVIO_PUTAREA` normally completes in less than 100 ms;
- the displayed image and overlays remain coherent and correctly colored;
- camera and ESP-DL timing do not regress materially.

## Chosen Approach

Enable SPI2 DMA through AgentGuard's reproducible Kconfig script, explicitly
retain the current 2,048-byte DMA buffer size and 64-byte DMA threshold, and
clean the draw buffer's data-cache range immediately before submitting it to
the LCD driver.

The existing row-stride display path remains unchanged. Each visible row is
480 bytes, above the DMA threshold; row starts are separated by 640 bytes and
both values have suitable cache-line alignment. This lets the existing ST7789
driver use DMA without introducing another full-frame buffer or changing the
driver outside the contest repository.

The most recent LCD write duration is exposed as `D` in the diagnostic footer.
During this focused diagnosis, the footer becomes:

```text
C:92 Q:40 L:94 D:31
```

`D` measures only the blocking `LCDDEVIO_PUTAREA` call. Because the current
frame has already been rendered before that call finishes, the value displayed
on frame N is the duration measured for frame N-1. The first frame does not
claim a valid `D` value.

## Alternatives Considered

### Enable DMA without cache maintenance

This is the smallest configuration change, but the large draw buffer can be
allocated from cached external RAM. The SPI DMA lower half does not clean the
transmit range. Omitting cache maintenance could therefore display stale or
partially updated pixels and would make a successful-looking test unreliable.

### Raise the SPI clock

The LCD already runs at 40 MHz. Raising the clock cannot remove the roughly
1,800 polling transactions and introduces panel and signal-integrity risk. It
does not address the measured type of overhead.

### Add a contiguous 240x240 bounce or crop buffer

A contiguous buffer would let ST7789 submit the area as a single logical
transfer, but it costs another 115,200 bytes plus a frame copy and adds
ownership complexity. It remains a fallback only if row-at-a-time DMA still
misses the target.

## Components and Data Flow

### Reproducible target configuration

`tools/apply_agentguard_config.sh` enables:

```text
CONFIG_ESP32S3_SPI_DMA=y
CONFIG_ESP32S3_SPI_DMA_BUFSIZE=2048
CONFIG_ESP32S3_SPI_DMATHRESHOLD=64
```

The script remains the source of truth; generated NuttX `.config` changes are
not committed in place.

### Cache coherency

Immediately after composing the complete draw frame and before
`LCDDEVIO_PUTAREA`, the display path calls `up_clean_dcache()` for the full
draw buffer range. Cleaning the whole 320x240 RGB565 allocation is deliberate:
it is simple, bounded, and includes every strided source row and UI overlay.
It occurs after the final CPU write and before the blocking DMA read.

The current 32-byte-aligned allocation is retained. The clean range ends at
the allocation size, following the NuttX cache API's half-open range usage.

### LCD duration state

The display worker owns the last valid LCD write duration. For each refresh it:

1. copies the latest published frame and status under the existing mutex;
2. adds the previous valid `D` value to its local status copy;
3. renders the camera, overlays, and diagnostic footer;
4. cleans the completed draw buffer cache range;
5. records monotonic time immediately before and after `LCDDEVIO_PUTAREA`;
6. stores the nonnegative elapsed milliseconds for the next refresh.

The main camera thread does not own or synchronize `D`; this avoids feeding a
display-side measurement backward through frame-publication state.

## Error Handling and Fallback

- A monotonic clock failure or backward timestamp invalidates that `D` sample
  without affecting the LCD write.
- An LCD ioctl failure follows the existing display error path and does not
  publish a duration as valid.
- Durations above the UI limit render as `9999+`, consistent with the existing
  timing diagnostics.
- If DMA causes corruption or a target fault, the change is not accepted as a
  performance success. The first fallback is a small internal-RAM DMA bounce
  strategy, not an unsafe removal of cache maintenance.
- If DMA is correct but still takes at least 100 ms, the next optimization is
  a contiguous 240x240 transfer buffer so ST7789 no longer submits 240 logical
  row transfers.

## Testing

Implementation follows test-driven development.

Host/source-contract tests cover:

- the reproducible configuration script enables SPI DMA and pins its buffer
  size and threshold;
- a valid prior LCD duration appears as `D` in the timing footer;
- an unavailable first sample does not claim a valid LCD duration;
- the existing duration clamp renders values above the limit as `9999+`;
- camera metrics remain intact while `D` replaces the now-known `AI` value in
  the focused footer;
- the target display source cleans the draw-buffer cache after rendering and
  before the LCD ioctl.

After focused tests pass, all AgentGuard host tests and source-contract tests
must pass. The generated target configuration must contain the three intended
SPI DMA values, the full NuttX target build must succeed, and changed files
must pass `git diff --check`.

The built image is then flashed with hash verification. Physical validation
records the displayed `D` range and confirms that the edge-to-edge scan is no
longer visible, colors and overlays are coherent, and `C`, `Q`, and `L` remain
within their previous operating range.

## Scope Boundaries

This change does not modify the NuttX ST7789 or ESP32-S3 SPI driver sources,
LCD clock frequency, visible resolution, frame rotation, camera mode, V4L2
buffers, inference cadence, model thresholds, policy logic, network behavior,
or persistent storage. A contiguous display buffer is explicitly deferred
until measurements show it is necessary.
