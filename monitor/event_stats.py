#!/usr/bin/env python3
"""Summarize AgentGuard desktop JSONL events."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path


def default_log_path() -> Path:
    state_home = os.environ.get("XDG_STATE_HOME")
    base = Path(state_home).expanduser() if state_home else \
        Path.home() / ".local/state"
    return base / "agentguard/events.jsonl"


def read_event_stats(path: Path) -> dict[str, int]:
    stats = {
        "total_events": 0,
        "sedentary_alerts": 0,
        "acknowledgements": 0,
        "privacy_triggers": 0,
        "malformed_lines": 0,
    }
    with path.open("r", encoding="utf-8") as stream:
        for line in stream:
            if not line.strip():
                continue
            try:
                payload = json.loads(line)
            except (json.JSONDecodeError, RecursionError):
                stats["malformed_lines"] += 1
                continue
            if not isinstance(payload, dict) or not isinstance(
                    payload.get("event"), str):
                stats["malformed_lines"] += 1
                continue

            event = payload["event"]
            stats["total_events"] += 1
            if event == "sedentary_alert":
                stats["sedentary_alerts"] += 1
            elif event == "acknowledged":
                stats["acknowledgements"] += 1
            elif event == "blur_screen":
                stats["privacy_triggers"] += 1
    return stats


def format_text(path: Path, stats: dict[str, int]) -> str:
    return "\n".join([
        "AgentGuard 事件统计",
        f"  日志: {path}",
        f"  有效事件: {stats['total_events']}",
        f"  久坐提醒次数: {stats['sedentary_alerts']}",
        f"  用户确认次数: {stats['acknowledgements']}",
        f"  隐私遮罩触发次数: {stats['privacy_triggers']}",
        f"  损坏记录: {stats['malformed_lines']}",
    ])


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", nargs="?", type=Path,
                        default=default_log_path(),
                        help="JSONL log path (default: AgentGuard state log)")
    parser.add_argument("--json", action="store_true",
                        help="print machine-readable JSON")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        stats = read_event_stats(args.log)
    except OSError as exc:
        raise SystemExit(f"无法读取日志 {args.log}: {exc}") from exc
    if args.json:
        print(json.dumps(stats, ensure_ascii=False, sort_keys=True))
    else:
        print(format_text(args.log, stats))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
