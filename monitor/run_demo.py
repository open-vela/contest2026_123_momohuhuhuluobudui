#!/usr/bin/env python3
"""Run the USB companion and live dashboard with one command."""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
import webbrowser
from pathlib import Path

from event_stats import default_log_path


def build_commands(monitor_dir: Path, python_executable: str,
                   serial_device: str, log_path: Path,
                   port: int) -> tuple[list[str], list[str]]:
    listener = [python_executable, str(monitor_dir / "agentguard_pc.py"),
                "--no-http", "--serial", serial_device,
                "--log", str(log_path)]
    dashboard = [python_executable, str(monitor_dir / "dashboard.py"),
                 "--log", str(log_path), "--port", str(port)]
    return listener, dashboard


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", default="/dev/ttyACM0")
    parser.add_argument("--log", type=Path, default=default_log_path())
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--no-browser", action="store_true")
    return parser.parse_args()


def stop_process(process: subprocess.Popen) -> None:
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def main() -> int:
    args = parse_args()
    monitor_dir = Path(__file__).resolve().parent
    log_path = args.log.expanduser().resolve()
    log_path.parent.mkdir(parents=True, exist_ok=True)
    listener_command, dashboard_command = build_commands(
        monitor_dir, sys.executable, args.serial, log_path, args.port)
    url = f"http://127.0.0.1:{args.port}"

    print("AgentGuard 一键演示", flush=True)
    print(f"  串口: {args.serial}", flush=True)
    print(f"  日志: {log_path}", flush=True)
    print(f"  面板: {url}", flush=True)
    print("按 Ctrl+C 同时停止监听器和面板。", flush=True)

    dashboard = subprocess.Popen(dashboard_command)
    listener = subprocess.Popen(listener_command)
    try:
        time.sleep(0.6)
        if dashboard.poll() is not None:
            return dashboard.returncode or 1
        if not args.no_browser:
            webbrowser.open(url)
        while True:
            if listener.poll() is not None:
                return listener.returncode or 1
            if dashboard.poll() is not None:
                return dashboard.returncode or 1
            time.sleep(0.5)
    except KeyboardInterrupt:
        return 0
    finally:
        stop_process(listener)
        stop_process(dashboard)


if __name__ == "__main__":
    raise SystemExit(main())
