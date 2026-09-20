#!/usr/bin/env python3
"""Backfill contest-safe Codex sessions into the official log schema."""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


SCHEMA_VERSION = "1.0"
TOOL = "codex"

REDACTIONS = (
    (re.compile(r"(?:\\?[\"'])encrypted_content(?:\\?[\"'])\s*:\s*"
                r"(?:\\?[\"'])[^\"'\r\n]*(?:\\?[\"'])"),
     '"encrypted_content":"[ENCRYPTED_REDACTED]"'),
    (re.compile(r"gAAAAA[A-Za-z0-9_-]{32,}"), "[ENCRYPTED_REDACTED]"),
    (re.compile(r"(?<!\d)1[3-9]\d{9}(?!\d)"), "[PHONE_REDACTED]"),
    (re.compile(r"[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}"),
     "[EMAIL_REDACTED]"),
    (re.compile(r"北京工业大学"), "[SCHOOL_REDACTED]"),
    (re.compile(r"闫贺翔"), "[NAME_REDACTED]"),
    (re.compile(r"agentguard-usb-local-demo"), "[TOKEN_REDACTED]"),
    (re.compile(r"sk-[A-Za-z0-9_-]{20,}"), "sk-***REDACTED***"),
    (re.compile(r"gh[opsu]_[A-Za-z0-9]{20,}"), "gh*_***REDACTED***"),
    (re.compile(r"Bearer\s+[A-Za-z0-9._\-+/=]+"),
     "Bearer ***REDACTED***"),
    (re.compile(r"(AGENTGUARD_TOKEN\s*=\s*['\"]?)[^'\"\s;]+"),
     r"\1***REDACTED***"),
)


@dataclass(frozen=True)
class SessionSource:
    path: Path
    session_id: str
    started_at: str
    cwd: str
    reviewer: bool


@dataclass
class ConvertedSession:
    session_id: str
    started_at: str
    last_event_at: str
    cwd: str
    model: str
    events: list[dict[str, Any]]
    tokens_in: int
    tokens_out: int
    tokens_total: int
    redacted_count: int


def _load_rows(path: Path):
    with path.open(encoding="utf-8") as stream:
        for line in stream:
            try:
                row = json.loads(line)
            except json.JSONDecodeError:
                continue
            if isinstance(row, dict):
                yield row


def _session_meta(path: Path) -> dict[str, Any] | None:
    for row in _load_rows(path):
        if row.get("type") == "session_meta":
            payload = row.get("payload")
            return payload if isinstance(payload, dict) else None
    return None


def _inside(path: str, root: Path) -> bool:
    try:
        resolved = Path(path).resolve()
        resolved_root = root.resolve()
        return resolved == resolved_root or resolved_root in resolved.parents
    except (OSError, RuntimeError, ValueError):
        return False


def _is_reviewer(source: Any) -> bool:
    if not isinstance(source, dict):
        return False
    subagent = source.get("subagent")
    return isinstance(subagent, dict) and isinstance(
        subagent.get("thread_spawn"), dict
    )


def _is_guardian(source: Any) -> bool:
    if not isinstance(source, dict):
        return False
    subagent = source.get("subagent")
    return isinstance(subagent, dict) and subagent.get("other") == "guardian"


def discover_sessions(session_root: Path, contest_root: Path) -> list[SessionSource]:
    selected = []
    for path in session_root.rglob("*.jsonl"):
        meta = _session_meta(path)
        if not meta or not _inside(str(meta.get("cwd", "")), contest_root):
            continue
        source = meta.get("source")
        if _is_guardian(source):
            continue
        parent = str(meta.get("parent_thread_id") or "")
        reviewer = _is_reviewer(source)
        if parent and not reviewer:
            continue
        session_id = str(meta.get("id") or meta.get("session_id") or "")
        started_at = str(meta.get("timestamp") or "")
        if session_id and started_at:
            selected.append(SessionSource(
                path=path,
                session_id=session_id,
                started_at=started_at,
                cwd=str(meta.get("cwd") or ""),
                reviewer=reviewer,
            ))
    return sorted(selected, key=lambda item: (item.started_at, item.session_id))


def _extract_text(value: Any) -> str:
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        chunks = []
        for item in value:
            if not isinstance(item, dict):
                continue
            if item.get("type") not in ("input_text", "output_text", "text"):
                continue
            text = item.get("text")
            if isinstance(text, str) and text:
                chunks.append(text)
        return "\n".join(chunks)
    if isinstance(value, dict):
        text = value.get("text")
        return text if isinstance(text, str) else ""
    return ""


def _parse_arguments(value: Any) -> Any:
    if not isinstance(value, str):
        return value
    try:
        return json.loads(value)
    except json.JSONDecodeError:
        return value


def _redact(value: Any) -> tuple[Any, int]:
    if isinstance(value, str):
        count = 0
        result = value
        for pattern, replacement in REDACTIONS:
            result, found = pattern.subn(replacement, result)
            count += found
        return result, count
    if isinstance(value, list):
        result = []
        count = 0
        for item in value:
            redacted, found = _redact(item)
            result.append(redacted)
            count += found
        return result, count
    if isinstance(value, dict):
        result = {}
        count = 0
        for key, item in value.items():
            redacted, found = _redact(item)
            result[key] = redacted
            count += found
        return result, count
    return value, 0


def convert_session(path: Path, team_id: str,
                    github_login: str) -> ConvertedSession:
    meta = _session_meta(path)
    if not meta:
        raise ValueError(f"session_meta missing: {path}")
    session_id = str(meta.get("id") or meta.get("session_id") or "")
    started_at = str(meta.get("timestamp") or "")
    cwd = str(meta.get("cwd") or "")
    if not session_id or not started_at:
        raise ValueError(f"invalid session_meta: {path}")

    events: list[dict[str, Any]] = []
    calls: dict[str, int] = {}
    last_ts = started_at
    model = ""
    tokens_in = tokens_out = tokens_total = 0

    for row in _load_rows(path):
        timestamp = str(row.get("timestamp") or last_ts)
        last_ts = max(last_ts, timestamp)
        row_type = row.get("type")
        payload = row.get("payload")
        if not isinstance(payload, dict):
            continue

        if row_type == "turn_context":
            current_model = payload.get("model")
            if isinstance(current_model, str) and current_model:
                model = current_model
            continue
        if row_type == "token_usage_record":
            usage = payload.get("thread_token_usage")
            if isinstance(usage, dict):
                tokens_in = int(usage.get("input_tokens") or 0)
                tokens_out = int(usage.get("output_tokens") or 0)
                tokens_total = int(usage.get("total_tokens") or
                                   (tokens_in + tokens_out))
            continue
        if row_type != "response_item":
            continue

        item_type = payload.get("type")
        if item_type == "message":
            role = payload.get("role")
            if role not in ("user", "assistant"):
                continue
            text = _extract_text(payload.get("content"))
            if not text:
                continue
            event = {"ts": timestamp, "role": role, "text": text}
            if role == "assistant" and model:
                event["model"] = model
            events.append(event)
            continue

        if item_type in ("custom_tool_call", "function_call"):
            call_id = str(payload.get("call_id") or payload.get("id") or
                          f"call-{len(events)}")
            raw_input = payload.get("input") if item_type == "custom_tool_call" \
                else payload.get("arguments")
            event = {
                "ts": timestamp,
                "role": "tool",
                "tool_name": str(payload.get("name") or "unknown"),
                "tool_call_id": call_id,
                "input": _parse_arguments(raw_input),
                "output": None,
            }
            calls[call_id] = len(events)
            events.append(event)
            continue

        if item_type in ("custom_tool_call_output", "function_call_output"):
            call_id = str(payload.get("call_id") or payload.get("id") or "")
            output = _extract_text(payload.get("output"))
            if call_id in calls:
                events[calls[call_id]]["output"] = output
            else:
                events.append({
                    "ts": timestamp,
                    "role": "tool",
                    "tool_name": "<result>",
                    "tool_call_id": call_id or f"result-{len(events)}",
                    "input": None,
                    "output": output,
                })

    output_events = []
    redacted_total = 0
    for seq, event in enumerate(events):
        redacted, found = _redact(event)
        redacted_total += found
        output = {
            "schema_version": SCHEMA_VERSION,
            "session_id": session_id,
            "team_id": team_id,
            "github_login": github_login,
            "tool": TOOL,
            "seq": seq,
            "cwd": cwd,
            **redacted,
        }
        if found:
            output["redacted_count"] = found
        output_events.append(output)

    return ConvertedSession(
        session_id=session_id,
        started_at=started_at,
        last_event_at=last_ts,
        cwd=cwd,
        model=model,
        events=output_events,
        tokens_in=tokens_in,
        tokens_out=tokens_out,
        tokens_total=tokens_total,
        redacted_count=redacted_total,
    )


def _iso_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def write_sessions(converted: list[ConvertedSession], repo_root: Path,
                   team_id: str, github_login: str) -> None:
    member_dir = repo_root / "logs" / github_login
    manifest_path = member_dir / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    sessions = [entry for entry in manifest.get("sessions", [])
                if entry.get("tool") != TOOL]

    for item in converted:
        if not item.events:
            continue
        date = item.started_at[:10]
        rel_path = f"logs/{github_login}/{date}/{TOOL}__{item.session_id}.jsonl"
        output = repo_root / rel_path
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(
            "".join(json.dumps(event, ensure_ascii=False) + "\n"
                    for event in item.events),
            encoding="utf-8",
        )
        entry = {
            "session_id": item.session_id,
            "tool": TOOL,
            "started_at": item.started_at,
            "last_event_at": item.last_event_at,
            "event_count": len(item.events),
            "file_path": rel_path,
            "collection_mode": "cli",
            "health": "ok",
            "tokens_total": item.tokens_total,
            "tokens_in_total": item.tokens_in,
            "tokens_out_total": item.tokens_out,
            "redacted_count_total": item.redacted_count,
        }
        if item.model:
            entry["model"] = item.model
        sessions.append(entry)

    manifest["schema_version"] = SCHEMA_VERSION
    manifest["team_id"] = team_id
    manifest["github_login"] = github_login
    manifest["generator"] = "agentguard-codex-backfill@1.0"
    manifest["updated_at"] = _iso_now()
    manifest["sessions"] = sorted(
        sessions, key=lambda entry: (entry.get("started_at", ""),
                                     entry.get("session_id", ""))
    )
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--session-root", type=Path,
                        default=Path.home() / ".codex" / "sessions")
    parser.add_argument("--contest-root", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--team-id", required=True)
    parser.add_argument("--github-login", required=True)
    parser.add_argument("--confirm", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    sources = discover_sessions(args.session_root, args.contest_root)
    converted = [convert_session(source.path, args.team_id, args.github_login)
                 for source in sources]
    nonempty = [item for item in converted if item.events]
    print(f"selected_sessions={len(sources)} nonempty_sessions={len(nonempty)}")
    print(f"events={sum(len(item.events) for item in nonempty)}")
    print(f"redactions={sum(item.redacted_count for item in nonempty)}")
    print(f"tokens_total={sum(item.tokens_total for item in nonempty)}")
    for item in nonempty:
        print(f"  {item.started_at[:10]} {item.session_id} "
              f"events={len(item.events)} redactions={item.redacted_count}")
    if not args.confirm:
        print("preview only; re-run with --confirm to write logs")
        return 0
    write_sessions(nonempty, args.repo_root, args.team_id, args.github_login)
    print(f"written_to={args.repo_root / 'logs' / args.github_login}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
