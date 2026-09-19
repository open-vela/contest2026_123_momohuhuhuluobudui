#!/usr/bin/env python3
"""Summarize or export AgentGuard desktop JSONL events."""

from __future__ import annotations

import argparse
import csv
import json
import os
import sys
from datetime import datetime
from pathlib import Path

COUNTED_EVENTS = {
    "sedentary_alert": "sedentary_alerts",
    "acknowledged": "acknowledgements",
    "blur_screen": "privacy_triggers",
}
EVENT_LABELS = {
    "sedentary_alert": "久坐提醒",
    "acknowledged": "用户确认",
    "blur_screen": "隐私遮罩",
    "unblur_screen": "隐私恢复",
    "posture_alert": "坐姿提醒",
    "lock_screen": "锁定屏幕",
}


def default_log_path() -> Path:
    state_home = os.environ.get("XDG_STATE_HOME")
    base = Path(state_home).expanduser() if state_home else \
        Path.home() / ".local/state"
    return base / "agentguard/events.jsonl"


def parse_timestamp(value: object) -> datetime | None:
    if not isinstance(value, str) or not value:
        return None
    try:
        return datetime.fromisoformat(value.replace("Z", "+00:00")).astimezone()
    except ValueError:
        return None


def load_events(path: Path) -> tuple[list[dict], int]:
    events = []
    malformed = 0
    with path.open("r", encoding="utf-8") as stream:
        for line in stream:
            if not line.strip():
                continue
            try:
                payload = json.loads(line)
            except (json.JSONDecodeError, RecursionError):
                malformed += 1
                continue
            if not isinstance(payload, dict) or not isinstance(
                    payload.get("event"), str):
                malformed += 1
                continue
            events.append(payload)
    return events, malformed


def summarize_events(events: list[dict], malformed: int = 0,
                     now: datetime | None = None) -> dict:
    now = (now or datetime.now().astimezone()).astimezone()
    stats = {
        "total_events": len(events),
        "sedentary_alerts": 0,
        "acknowledgements": 0,
        "privacy_triggers": 0,
        "today_total": 0,
        "today_sedentary_alerts": 0,
        "today_acknowledgements": 0,
        "today_privacy_triggers": 0,
        "malformed_lines": malformed,
        "last_event": None,
        "last_event_at": None,
    }
    latest: tuple[datetime, str] | None = None
    for payload in events:
        event = payload["event"]
        counter = COUNTED_EVENTS.get(event)
        if counter:
            stats[counter] += 1
        timestamp = parse_timestamp(payload.get("received_at") or
                                    payload.get("timestamp"))
        if timestamp is not None:
            if timestamp.date() == now.date():
                stats["today_total"] += 1
                if counter:
                    stats[f"today_{counter}"] += 1
            if latest is None or timestamp > latest[0]:
                latest = (timestamp, event)
    if latest:
        stats["last_event"] = latest[1]
        stats["last_event_at"] = latest[0].isoformat(timespec="seconds")
    elif events:
        stats["last_event"] = events[-1]["event"]
    return stats


def read_event_stats(path: Path, now: datetime | None = None) -> dict:
    events, malformed = load_events(path)
    return summarize_events(events, malformed, now)


def format_text(path: Path, stats: dict) -> str:
    last_event = stats["last_event"]
    last_label = EVENT_LABELS.get(last_event, last_event) if last_event else "无"
    if stats["last_event_at"]:
        last_label += f"（{stats['last_event_at']}）"
    return "\n".join([
        "AgentGuard 事件统计",
        f"  日志: {path}",
        f"  累计: 久坐 {stats['sedentary_alerts']} / "
        f"确认 {stats['acknowledgements']} / "
        f"隐私 {stats['privacy_triggers']}",
        f"  今日: 久坐 {stats['today_sedentary_alerts']} / "
        f"确认 {stats['today_acknowledgements']} / "
        f"隐私 {stats['today_privacy_triggers']}",
        f"  有效事件: {stats['total_events']}（今日 {stats['today_total']}）",
        f"  最近事件: {last_label}",
        f"  损坏记录: {stats['malformed_lines']}",
    ])


def export_csv(path: Path, destination: Path) -> int:
    events, _ = load_events(path)
    with destination.open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["received_at", "event", "event_label"])
        for payload in events:
            event = payload["event"]
            writer.writerow([
                payload.get("received_at") or payload.get("timestamp") or "",
                event,
                EVENT_LABELS.get(event, event),
            ])
    return len(events)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", nargs="?", type=Path,
                        default=default_log_path(),
                        help="JSONL log path (default: AgentGuard state log)")
    output = parser.add_mutually_exclusive_group()
    output.add_argument("--json", action="store_true",
                        help="print machine-readable JSON")
    output.add_argument("--csv", type=Path, metavar="PATH",
                        help="export all valid events to CSV")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        if args.csv:
            count = export_csv(args.log, args.csv)
            print(f"已导出 {count} 条事件到 {args.csv}")
            return 0
        stats = read_event_stats(args.log)
    except OSError as exc:
        print(f"无法读取日志 {args.log}: {exc}", file=sys.stderr)
        return 1
    if args.json:
        print(json.dumps(stats, ensure_ascii=False, sort_keys=True))
    else:
        print(format_text(args.log, stats))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
