# LCD DMA RGB565 Byte-Order Correction Design

## Problem

After routing LCD writes through a 64-byte-aligned internal-RAM bounce buffer,
the random colored horizontal corruption disappeared.  The displayed image is
now structurally coherent, but its colors are dominated by blue, green, and
yellow and red is absent.

The ST7789 driver selects 16-bit SPI words for pixel transfers.  The ESP32-S3
polling block-transfer path swaps the two bytes in every 16-bit word before it
writes the SPI data registers.  Its DMA path does not perform that swap and
instead transmits the bytes in memory order.  The panel is configured for
big-endian RGB data because `CONFIG_LCD_ST7789_DATA_ENDIAN_LITTLE` is disabled.
Consequently, a host RGB565 red pixel `0xf800`, stored as bytes `00 f8`, reaches
the panel as `0x00f8` on the DMA path instead of `0xf800`.

This difference explains why the image geometry is now correct while the
color channels are not.

## Scope

This change corrects only RGB565 byte order at the AgentGuard internal-RAM DMA
bounce boundary.  It does not change:

- the 8-row, 3,840-byte bounce-buffer size;
- the SPI DMA threshold or descriptor settings;
- ST7789 global configuration;
- the ESP32-S3 SPI driver;
- frame scheduling or the 30 LCD area submissions per 240-row frame.

The visible top-to-bottom refresh and `D:50-200` timing are a separate problem
and will be investigated after correct colors are physically confirmed.

## Considered Approaches

### 1. Swap RGB565 words while filling the bounce buffer

This is the selected approach.  Each source RGB565 word is byte-swapped as it
is copied from the source frame into internal RAM.  On a little-endian CPU,
the swapped word's memory bytes are then in the big-endian order expected by
the panel when DMA transmits them unchanged.

This keeps the correction local to the one transfer path known to require it,
does not alter shared platform code, and gives a single-variable physical
test.

### 2. Configure the ST7789 for little-endian RGB data

This would make raw DMA order match the panel, but it globally changes panel
semantics.  Small or non-DMA 16-bit transfers would still pass through the
polling path's software byte swap and could become incorrect.

### 3. Add 16-bit byte swapping to the ESP32-S3 SPI DMA driver

This would make DMA mirror the polling path at the SPI abstraction boundary,
but it changes shared NuttX behavior for every 16-bit SPI DMA consumer.  It
also requires platform-wide validation outside the immediate AgentGuard fix.

## Data Flow

For each bounce chunk:

1. Read RGB565 source pixels from the cropped PSRAM frame.
2. Store `bswap16(source_pixel)` into the aligned internal-RAM bounce buffer.
3. Clean the exact bounce-buffer cache range.
4. Submit the chunk through `LCDDEVIO_PUTAREA`.
5. The ST7789 driver selects 16-bit SPI words, and ESP32-S3 DMA transmits the
   bounce bytes unchanged.
6. The panel receives the original source RGB565 value in big-endian order.

For example:

| Source RGB565 word | Source memory bytes | Bounce word | DMA bytes | Panel word |
| --- | --- | --- | --- | --- |
| `0xf800` (red) | `00 f8` | `0x00f8` | `f8 00` | `0xf800` |
| `0x07e0` (green) | `e0 07` | `0xe007` | `07 e0` | `0x07e0` |
| `0x001f` (blue) | `1f 00` | `0x1f00` | `00 1f` | `0x001f` |

## Component Contract

`ag_lcd_bounce_frame` remains responsible for preparing the physical LCD DMA
payload, not merely for generic memory copying.  Its submitted chunk data will
therefore be defined as byte-swapped RGB565 words suitable for the current
16-bit ESP32-S3 SPI DMA path.

No runtime switch is added.  AgentGuard has one LCD transport, and a switch
would permit an invalid combination that is not required by the product.

All existing dimension, crop, clock, and submission-error behavior remains
unchanged.  A failed submission still stops the frame and invalidates timing.

## Tests

The host bounce-controller test will first be changed to require DMA-ready
RGB565 values.  Before production code changes, the test must fail because the
current implementation copies words unchanged.

The test fixture will include values with distinguishable red, green, and blue
bits.  Assertions will verify both crop/chunk placement and exact swapped
words, including:

- `0xf800 -> 0x00f8`;
- `0x07e0 -> 0xe007`;
- `0x001f -> 0x1f00`;
- unchanged symmetric values where applicable.

After the minimal implementation, all AgentGuard host tests, configuration
checks, target build, ELF internal-RAM placement check, image validation, and
flash hash verification must pass.

## Physical Acceptance

The diagnostic firmware succeeds when:

- colored horizontal corruption remains absent;
- red content is visible again;
- normal UI and camera colors are restored rather than blue/green/yellow
  dominated;
- screen geometry remains coherent.

Top-to-bottom scanning is explicitly not an acceptance condition for this
byte-order change.  It remains the next optimization target after color
correctness is confirmed.
