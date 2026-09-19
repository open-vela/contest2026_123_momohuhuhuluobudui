---
name: develop-agentguard-openvela
description: Build, debug, test, and document AgentGuard or similar edge-AI health/privacy applications for the ESP32-S3-EYE on openvela/NuttX. Use for camera, LCD, button, LED, Wi-Fi, local logging, PC-agent integration, contest manifest wiring, cross-compilation, flashing, or hardware acceptance work in this repository.
---

# Develop AgentGuard on openvela

Work only in the contest repository. Treat the surrounding openvela checkout,
including `nuttx/`, `apps/`, `packages/`, and `vendor/`, as read-only dependencies.

## Start every task

1. Run `scripts/check_workspace.sh <contest-repo> <openvela-root>`.
2. Inspect `git status --short`; preserve unrelated and pre-existing changes.
3. Read `references/contest-layout.md` when changing directories, manifests,
   configuration, or submission documentation.
4. Read `references/esp32s3-eye.md` when touching hardware, building, flashing,
   or diagnosing a peripheral.
5. State which layer is changing: portable decision core, inference, NuttX
   adapter, PC agent, or documentation.

## Implement in verification gates

Follow these gates in order. Do not call a later gate complete when an earlier
gate is failing.

1. **Portable core**: express timing and policy as deterministic state
   transitions driven by monotonic timestamps. Avoid camera, network, or file
   calls in this layer. Add host tests for thresholds, acknowledgements,
   privacy transitions, and timer resets.
2. **Inference adapter**: return observations (`face_count`, bounding box,
   posture score, gesture) without performing policy actions. Test synthetic
   images or recorded fixtures. Clearly distinguish a heuristic detector from
   a trained face/pose model in code and docs.
3. **NuttX hardware adapter**: use device nodes exposed by the official BSP;
   do not use ESP-IDF `app_main`, `esp_camera_*`, or FreeRTOS-only APIs.
4. **PC protocol**: authenticate every state-changing request, reject unknown
   events, cap request sizes, bind locally by default, and keep destructive
   actions disabled unless explicitly enabled by the operator.
5. **Persistence**: append structured JSONL events and tolerate an absent or
   read-only filesystem. Keep a bounded retention policy before claiming
   seven-day memory.
6. **Cross-build**: enable the application from the official
   `esp32s3-eye/configs/openvela` config and build through `build.sh`.
7. **Hardware acceptance**: verify `/dev/video0`, `/dev/lcd0`,
   `/dev/audio/pcm_in0`, `/dev/buttons`, `/dev/userleds`, and Wi-Fi separately
   before validating AgentGuard end to end.

## Preserve evidence

- Record exact test and build commands in the final handoff.
- Update the root README with reproducible build, flash, configuration, and
  acceptance steps.
- Mark unavailable hardware/model validation as pending; never convert a host
  test or successful compile into a claim of real-world detection accuracy.
- Keep AI collaboration logs in `logs/` according to the contest instructions.

## Definition of done

Require all applicable items: host tests pass with warnings-as-errors; the
manifest maps the application; the official BSP cross-build succeeds; PC-agent
tests cover authorization and action dispatch; no secrets are committed;
documentation identifies heuristic/model limitations; and the user receives a
short checklist for the next physical-board step.
