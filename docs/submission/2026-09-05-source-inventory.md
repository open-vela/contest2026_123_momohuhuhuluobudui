# AgentGuard Submission Source Inventory

## Required application inputs

- `app/agentguard/src`, `include`, `compat`: owned application and compatibility code.
- `app/agentguard/third_party/esp-dl`: minimal vendored ESP-DL source subset used by the NuttX Makefile; generated `.o` files are excluded.
- `app/agentguard/third_party/human_face_detect/human_face_detect.espdl`: 191,248-byte embedded MSR+MNP model.
- `app/agentguard/third_party/human_face_detect/human_face_rgb565be.bin`: 153,600-byte diagnostic reference frame.
- Both third-party license files are retained next to their sources.

## Required integration inputs

- `contest2026_123_momohuhuhuluobudui.xml`: links `app/agentguard` into `packages/demos/contest2026_123_agentguard`.
- `tools/apply_agentguard_config.sh`: applies the reproducible ESP32-S3-EYE product configuration.
- `monitor`: authenticated PC event receiver and tests.
- `skills/develop-agentguard-openvela`: project development and evidence rules.

## Excluded generated and local inputs

Object files, dependency markers, test executables, Python caches, firmware images, JTAG dumps, the standalone root `third_party` checkout, private tokens, Wi-Fi credentials, and local submission templates are not source inputs and are excluded.
