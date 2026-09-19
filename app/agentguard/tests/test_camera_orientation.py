#!/usr/bin/env python3

from pathlib import Path


source = (Path(__file__).parent.parent / "src" / "agentguard_main.c").read_text()

rotate_call = "ag_ui_rotate_180_rgb565((uint16_t *)frame.m.userptr"
vision_call = "ag_vision_process_rgb565(&vision"

assert source.count(rotate_call) == 1, "camera frame must be rotated exactly once"
assert source.index(rotate_call) < source.index(vision_call), (
    "camera frame must be oriented before face inference"
)
assert "display_face.x = AG_WIDTH - display_face.x" not in source, (
    "already-oriented detection boxes must not be rotated a second time"
)

print("AgentGuard camera orientation test: PASS")
