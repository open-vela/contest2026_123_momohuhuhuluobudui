# ESP-DL TIE Convolution Self-Test Design

## Context

The face detector produces stable but incorrect results when the ESP32-S3 TIE
convolution kernels are enabled: the embedded reference and live frames both
report a score of 2 and no face. Running convolution in C changes the result,
which isolates the fault to the accelerated convolution path. Moving ESP-DL
internal allocations from PSRAM into a private internal-memory pool did not
change the TIE result, so activation tensor placement is not the cause.

The remaining leading possibilities are:

1. TIE vector loads do not read model weights correctly from Flash-mapped
   read-only memory.
2. The NuttX build does not satisfy the TIE kernel ABI or instruction
   assumptions.
3. The standalone kernel works, and the defect instead lies in model-specific
   arguments, layout, or scheduling.

## Goal

Add a deterministic, one-shot startup diagnostic that distinguishes these
possibilities without changing face-detection thresholds, model behavior, or
the camera/display pipeline.

## Diagnostic Structure

The diagnostic uses one small, known int8 convolution case whose values avoid
ambiguous saturation and rounding:

- one aligned input vector in internal writable memory;
- one aligned filter copy in internal writable memory;
- an identical `const` aligned filter in Flash-mapped read-only memory;
- two independent aligned output buffers;
- a scalar reference calculation performed by ordinary C code.

The code constructs the same ESP-DL `ArgsType<int8_t>` for both accelerated
runs, changing only the filter pointer. It calls the smallest suitable exported
TIE convolution kernel directly. Before enabling the call, compile-time layout
checks verify every `ArgsType` field offset consumed by the assembly routine.

The diagnostic runs once during AgentGuard startup, before camera registration
or model inference starts. It prints one compact serial record containing:

- input, RAM-filter, Flash-filter, argument, and output addresses;
- expected scalar output;
- RAM-filter TIE output;
- Flash-filter TIE output;
- a classification code.

The board's NuttX application `stderr` is not routed to the available USB CDC
console, and USB-JTAG memory inspection is unavailable in the development
environment. The diagnostic therefore also exposes a read-only result query.
The display worker reads this query independently of camera frames and carries
the two pass bits in its existing UI status. The LCD footer appends `K:xy` to
the normal model timing line and displays it on the camera-error path as well.
`x` is the RAM-filter pass bit and `y` is the Flash-filter pass bit. Before a
result is available the footer shows `K:--`; after the one-shot run it shows
exactly one of `K:10`, `K:00`, `K:11`, or `K:01`.

The camera-error primary line uses the stable numeric form `CAM:<phase>` rather
than a changing textual phase name. This prevents partial LCD refreshes from
visually combining labels such as `CAM WORK QUEUED` and `V4L2 CALLBACK` and
allows the exact camera boundary to be reported alongside `K:xy`. These display
changes do not alter the camera viewport, inference schedule, detector
thresholds, or face result.

## Classification

The result is classified deterministically:

- `RAM_PASS_FLASH_FAIL`: Flash/DROM filter access is the failing boundary.
- `RAM_FAIL_FLASH_FAIL`: the problem is below model integration, most likely
  kernel ABI, instruction execution, or argument construction.
- `RAM_PASS_FLASH_PASS`: the primitive works; the next diagnostic must compare
  C and TIE at the first divergent model layer.
- `RAM_FAIL_FLASH_PASS`: an unexpected memory-region dependency; preserve the
  raw values and investigate before modifying the runtime.

## Isolation and Failure Handling

The self-test is compiled as a small AgentGuard diagnostic unit with one public
function called from AgentGuard startup and one read-only status query. It
owns no long-lived heap memory and does not modify model tensors. All buffers
use static aligned storage so stack size and allocation failure cannot affect
the result. LCD rendering remains outside the self-test module and consumes
only the queried pass bits through the display worker. A missing camera frame
cannot prevent either the self-test from running or its result from reaching
the LCD.

If required kernel symbols or ABI assumptions are unavailable, the build must
fail rather than silently skip the test. A mismatch is diagnostic evidence,
not a boot failure: it is logged, and the existing application continues so
camera and LCD behavior remain observable.

## Verification

Host-side tests validate the scalar fixture, classification table, exact
one-shot behavior, unavailable-state handling, and LCD `K:xy` formatting
without executing Xtensa instructions. Target verification then consists of:

1. building with both ordinary and depthwise TIE convolution enabled;
2. confirming the self-test object and TIE symbol are linked;
3. flashing the board;
4. resetting and capturing the startup serial record automatically;
5. confirming `K:xy` appears even when the camera does not produce a frame;
6. reading the single `K:xy` and `CAM:<phase>` result pair from the LCD.

No runtime fix will be implemented until this evidence identifies the failing
boundary.
