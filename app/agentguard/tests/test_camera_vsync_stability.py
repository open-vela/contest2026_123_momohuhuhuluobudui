#!/usr/bin/env python3

from pathlib import Path


PATCH_RELATIVE = Path("tools/patches/nuttx-esp32s3-cam-vsync.patch")
CONFIG_SCRIPT_RELATIVE = Path("tools/apply_agentguard_config.sh")


repo_root = Path(__file__).resolve().parents[3]
patch_source = (repo_root / PATCH_RELATIVE).read_text(encoding="utf-8")
config_script = (repo_root / CONFIG_SCRIPT_RELATIVE).read_text(encoding="utf-8")
assert "-  up_udelay(10);" in patch_source
assert patch_source.count("-  esp32s3_gpio_matrix_in(") == 2
assert "CAM_V_SYNC_IDX, false" in patch_source
assert "CAM_V_SYNC_IDX, true" in patch_source
assert 'camera_patch="$repo_root/tools/patches/nuttx-esp32s3-cam-vsync.patch"' in config_script
assert 'git -C "$openvela_root/nuttx" apply' in config_script

print("AgentGuard camera VSYNC stability test: PASS")
