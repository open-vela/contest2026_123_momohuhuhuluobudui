# AgentGuard openvela application

## Layers

- `src/core.c`: deterministic policy state machine; no hardware dependencies.
- `src/vision_espdl.cpp`: ESP-DL inference adapter producing face boxes/counts.
- `src/agentguard_main.c`: NuttX V4L2, LCD, button, LED, JSONL and PC HTTP
  adapter.
- `src/storage.c`: bounded JSONL retention and behavior-event statistics.
- `tests/`: host tests built with warnings treated as errors.

The ESP-DL algorithm layer is kept close to upstream. A narrow compatibility
layer adapts image input, allocation and runtime calls to NuttX/openvela so the
health/privacy policy remains independent from the detector.

## Host verification

```bash
cd tests
make clean test
```

## NuttX entry point

Enable `CONFIG_LVX_USE_DEMO_CONTEST2026_123_AGENTGUARD`; the NSH command is
`agentguard`. Run it in the background with `agentguard &`.

On ESP32-S3-EYE, AgentGuard shows a live center crop of the 320x240 camera
frame on the 240x240 ST7789 LCD. ESP-DL detections are mapped to LCD rectangles
and the HUD reports the current face count. No owner identity is inferred.

The preview includes a compact RGB565 status HUD. It shows the estimated face
count, posture score, seated time, privacy state, calibration state, health
alerts and the BOOT-button acknowledgement prompt. Red means no person, green
means one-person monitoring, magenta means multi-person/privacy, and orange
means an active health alert.

Short BOOT press acknowledges an active reminder. Hold BOOT for 3--7 seconds
and release to toggle privacy; hold for at least 8 seconds and release to
toggle the 20-second Demo mode. `agentguard command` is a diagnostic text
parser only; this submission does not claim microphone KWS.

## Local behavior history

The repository configuration script enables the official ESP32-S3 SPI-flash
LittleFS mount at `/mnt/spif`. AgentGuard appends behavior events to
`/mnt/spif/agentguard-events.jsonl`, compacts records older than seven days
when wall-clock time is valid, and always enforces a configurable byte cap.
An unset clock therefore degrades to bounded retention instead of pretending
that calendar-day retention is available.

Use these NSH commands after the filesystem has mounted:

```text
nsh> agentguard stats
nsh> agentguard prune
```

The first boot with this storage configuration can format the dedicated
`0x300000..0x3fffff` flash region. It must not contain user data, and the final
firmware image must remain below the `0x300000` partition boundary.
