#!/usr/bin/env python3
"""Start the AgentGuard USB companion with safe demo defaults."""

from __future__ import annotations

import argparse
import os
import secrets
import sys
from collections.abc import Mapping
from pathlib import Path


def default_log_path(environment: Mapping[str, str], home: Path) -> Path:
    state_home = environment.get("XDG_STATE_HOME")
    base = Path(state_home).expanduser() if state_home else home / ".local/state"
    return base / "agentguard/events.jsonl"


def build_launch(
    *, monitor_dir: Path, python_executable: str, serial_device: str,
    log_path: Path, environment: Mapping[str, str],
) -> tuple[list[str], dict[str, str], bool]:
    listener = monitor_dir / "agentguard_pc.py"
    if not listener.is_file():
        raise FileNotFoundError(f"listener not found: {listener}")

    child_environment = dict(environment)
    generated = len(child_environment.get("AGENTGUARD_TOKEN", "")) < 16
    if generated:
        child_environment["AGENTGUARD_TOKEN"] = secrets.token_urlsafe(24)

    command = [
        python_executable,
        str(listener),
        "--no-http",
        "--serial",
        serial_device,
        "--log",
        str(log_path),
    ]
    return command, child_environment, generated


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", default="/dev/ttyACM0",
                        help="USB serial device (default: /dev/ttyACM0)")
    parser.add_argument("--log", type=Path,
                        help="JSONL event log (default: user state directory)")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    monitor_dir = Path(__file__).resolve().parent
    log_path = args.log or default_log_path(os.environ, Path.home())
    log_path = log_path.expanduser().resolve()
    log_path.parent.mkdir(parents=True, exist_ok=True)

    command, environment, generated = build_launch(
        monitor_dir=monitor_dir,
        python_executable=sys.executable,
        serial_device=args.serial,
        log_path=log_path,
        environment=os.environ,
    )

    print("AgentGuard USB 监听器", flush=True)
    print(f"  串口: {args.serial}", flush=True)
    print(f"  日志: {log_path}", flush=True)
    if generated:
        print("  令牌: 已自动生成本次运行的本地令牌", flush=True)
    if not Path(args.serial).exists():
        print("  提示: 串口暂不可用，监听器将持续等待并自动重连", flush=True)
    print("按 Ctrl+C 停止监听。", flush=True)

    os.execvpe(command[0], command, environment)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
