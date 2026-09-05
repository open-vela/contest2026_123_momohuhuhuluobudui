# AgentGuard openvela application

## Layers

- `src/core.c`: deterministic policy state machine; no hardware dependencies.
- `src/vision.c`: RGB565 bring-up heuristic producing face count and posture score.
- `src/agentguard_main.c`: NuttX V4L2, LCD, button, LED, JSONL and PC HTTP
  adapter.
- `src/storage.c`: bounded JSONL retention and behavior-event statistics.
- `tests/`: host tests built with warnings treated as errors.

The vision implementation is deliberately isolated so that ESP-WHO or another
measured model can replace it without changing the health/privacy policy.

## Host verification

```bash
cd tests
make clean test
```

## NuttX entry point

Enable `CONFIG_LVX_USE_DEMO_CONTEST2026_123_AGENTGUARD`; the NSH command is
`agentguard`. Run it in the background with `agentguard &`.

On ESP32-S3-EYE, AgentGuard shows a live center crop of the 320x240 camera
frame on the 240x240 ST7789 LCD. A green rectangle marks the region selected
by the current skin-color heuristic. It is a bring-up aid, not evidence of
production-grade face detection accuracy.

The preview includes a compact RGB565 status HUD. It shows the estimated face
count, posture score, seated time, privacy state, calibration state, health
alerts and the BOOT-button acknowledgement prompt. Red means no person, green
means one-person monitoring, yellow means calibration, magenta means
multi-person/privacy, and orange means an active health alert. The candidate
box uses the same state color.

The BOOT button acknowledges an active reminder. Supported textual command
phrases can be checked with `agentguard command "开启隐私模式"`; wiring the
parser to ESP-SR is a later inference-adapter step.

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
