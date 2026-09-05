#!/usr/bin/env bash
set -euo pipefail

contest_repo="${1:-.}"
openvela_root="${2:-$(cd "${contest_repo}/.." && pwd)}"
manifest="${contest_repo}/contest2026_123_momohuhuhuluobudui.xml"
bsp="${openvela_root}/vendor/espressif/boards/esp32s3/esp32s3-eye/configs/openvela/defconfig"
failed=0

check_file() {
  if [[ -e "$1" ]]; then
    printf 'OK   %s\n' "$2"
  else
    printf 'MISS %s: %s\n' "$2" "$1"
    failed=1
  fi
}

check_file "$manifest" "contest manifest"
check_file "$bsp" "official ESP32-S3-EYE defconfig"
check_file "${contest_repo}/app/agentguard/Kconfig" "AgentGuard Kconfig"
check_file "${contest_repo}/app/agentguard/Make.defs" "AgentGuard Make.defs"

if [[ -f "$manifest" ]] &&
   grep -q 'src="app/agentguard"' "$manifest"; then
  printf 'OK   AgentGuard manifest mapping\n'
else
  printf 'MISS AgentGuard manifest mapping\n'
  failed=1
fi

if [[ -L "${openvela_root}/packages/demos/contest2026_123_agentguard" ]]; then
  printf 'OK   repo-created package link\n'
else
  printf 'INFO package link is absent; run repo sync after manifest changes\n'
fi

exit "$failed"
