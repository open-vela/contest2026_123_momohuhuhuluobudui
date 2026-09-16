#!/usr/bin/env python3
"""Authenticated local PC companion for AgentGuard."""

from __future__ import annotations

import argparse
import hmac
import json
import logging
import os
import platform
import queue
import select
import subprocess
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Callable

LOGGER = logging.getLogger("agentguard.pc")
MAX_BODY_BYTES = 4096
EVENTS = {"sedentary_alert", "posture_alert", "blur_screen",
          "unblur_screen", "lock_screen"}
SERIAL_PREFIX = "AGENTGUARD_EVENT "
MAX_SERIAL_FRAME_BYTES = MAX_BODY_BYTES


class SerialFrameDecoder:
    def __init__(self, max_frame_bytes: int = MAX_SERIAL_FRAME_BYTES) -> None:
        self._max_frame_bytes = max_frame_bytes
        self._pending = bytearray()
        self._discarding = False

    @property
    def buffered_bytes(self) -> int:
        return len(self._pending)

    def feed(self, data: bytes) -> list[str]:
        lines = []
        self._pending.extend(data)
        while True:
            newline = self._pending.find(b"\n")
            if newline < 0:
                if len(self._pending) > self._max_frame_bytes:
                    self._pending.clear()
                    self._discarding = True
                break

            raw = bytes(self._pending[:newline])
            del self._pending[:newline + 1]
            if self._discarding:
                self._discarding = False
                continue
            if len(raw) > self._max_frame_bytes:
                continue
            lines.append(raw.decode("utf-8", errors="replace").rstrip("\r"))
        return lines


def parse_serial_event(line: str) -> dict | None:
    if not line.startswith(SERIAL_PREFIX):
        return None
    try:
        payload = json.loads(line[len(SERIAL_PREFIX):])
    except (json.JSONDecodeError, RecursionError):
        return None
    if not isinstance(payload, dict):
        return None
    event = payload.get("event")
    if not isinstance(event, str) or event not in EVENTS:
        return None
    return payload


def dispatch_serial_line(line: str, dispatcher: "ActionDispatcher",
                         log_path: Path) -> bool:
    payload = parse_serial_event(line.strip())
    if payload is None:
        return False
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("a", encoding="utf-8") as stream:
        stream.write(json.dumps(payload, ensure_ascii=False) + "\n")
    result = dispatcher.dispatch(payload["event"], payload)
    LOGGER.info("serial event=%s result=%s", payload["event"], result)
    return True


def serial_event_loop(device: str, dispatcher: "ActionDispatcher",
                      log_path: Path) -> None:
    decoder = SerialFrameDecoder()
    while True:
        try:
            fd = os.open(device, os.O_RDONLY | os.O_NONBLOCK | os.O_NOCTTY)
            LOGGER.info("listening on serial %s", device)
            try:
                while True:
                    readable, _, _ = select.select([fd], [], [], 1.0)
                    if not readable:
                        continue
                    data = os.read(fd, 4096)
                    if not data:
                        raise OSError("serial disconnected")
                    for line in decoder.feed(data):
                        dispatch_serial_line(line, dispatcher, log_path)
            finally:
                os.close(fd)
        except OSError as exc:
            LOGGER.warning("serial unavailable (%s); retrying", exc)
            decoder = SerialFrameDecoder()
            threading.Event().wait(1.0)


class PrivacyShield:
    """Local opaque privacy overlay controlled through a thread-safe queue."""

    def __init__(self) -> None:
        self._commands: queue.Queue[bool] = queue.Queue()
        self._thread: threading.Thread | None = None

    def start(self) -> None:
        if self._thread is None:
            self._thread = threading.Thread(target=self._run, daemon=True)
            self._thread.start()

    def show(self) -> None:
        self.start()
        self._commands.put(True)

    def hide(self) -> None:
        self._commands.put(False)

    def _run(self) -> None:
        try:
            import tkinter as tk

            root = tk.Tk()
            root.withdraw()
            shield = tk.Toplevel(root)
            shield.withdraw()
            shield.configure(background="#111111")
            shield.attributes("-fullscreen", True)
            shield.attributes("-topmost", True)
            tk.Label(shield, text="AgentGuard 隐私保护\n检测到他人靠近",
                     fg="white", bg="#111111",
                     font=("sans", 28, "bold")).pack(expand=True)

            def poll() -> None:
                try:
                    while True:
                        visible = self._commands.get_nowait()
                        shield.deiconify() if visible else shield.withdraw()
                except queue.Empty:
                    pass
                root.after(100, poll)

            root.after(100, poll)
            root.mainloop()
        except Exception as exc:
            LOGGER.warning("privacy shield unavailable: %s", exc)


class ActionDispatcher:
    def __init__(self, allow_lock: bool = False,
                 shield: PrivacyShield | None = None,
                 runner: Callable = subprocess.run) -> None:
        self.allow_lock = allow_lock
        self.shield = shield or PrivacyShield()
        self.runner = runner

    def dispatch(self, event: str, payload: dict) -> str:
        if event == "blur_screen":
            self.shield.show()
            return "privacy shield shown"
        if event == "unblur_screen":
            self.shield.hide()
            return "privacy shield hidden"
        if event == "lock_screen":
            if not self.allow_lock:
                LOGGER.warning("lock request ignored; start with --allow-lock")
                return "lock disabled"
            self._lock_screen()
            return "screen locked"
        if event == "sedentary_alert":
            self._notify("AgentGuard 久坐提醒", "请起身活动并让肩颈放松")
            return "notification sent"
        if event == "posture_alert":
            self._notify("AgentGuard 坐姿提醒", "抬头、收下巴并放松肩部")
            return "notification sent"
        raise ValueError(f"unsupported event: {event}")

    def _notify(self, title: str, message: str) -> None:
        system = platform.system()
        if system == "Darwin":
            title = title.replace('"', "")
            message = message.replace('"', "")
            self.runner(["osascript", "-e",
                         f'display notification "{message}" with title "{title}"'],
                        check=False)
        elif system == "Windows":
            LOGGER.info("%s: %s", title, message)
        else:
            self.runner(["notify-send", "--urgency=critical",
                         "--expire-time=10000", title, message], check=False)

    def _lock_screen(self) -> None:
        system = platform.system()
        if system == "Darwin":
            command = ["pmset", "displaysleepnow"]
        elif system == "Windows":
            command = ["rundll32.exe", "user32.dll,LockWorkStation"]
        else:
            command = ["loginctl", "lock-session"]
        self.runner(command, check=False)


def make_handler(token: str, dispatcher: ActionDispatcher, log_path: Path):
    expected = f"Bearer {token}"

    class AgentGuardHandler(BaseHTTPRequestHandler):
        server_version = "AgentGuardPC/1.0"

        def _json_response(self, status: int, payload: dict) -> None:
            body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self) -> None:
            self._json_response(200, {"status": "ok"}) if self.path == "/health" \
                else self._json_response(404, {"error": "not found"})

        def do_POST(self) -> None:
            if self.path != "/event":
                self._json_response(404, {"error": "not found"})
                return
            supplied = self.headers.get("Authorization", "")
            if not hmac.compare_digest(supplied, expected):
                self._json_response(401, {"error": "unauthorized"})
                return
            try:
                length = int(self.headers.get("Content-Length", "0"))
            except ValueError:
                self._json_response(400, {"error": "invalid content length"})
                return
            if length <= 0 or length > MAX_BODY_BYTES:
                self._json_response(413, {"error": "invalid body size"})
                return
            try:
                payload = json.loads(self.rfile.read(length).decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError):
                self._json_response(400, {"error": "invalid JSON"})
                return
            event = payload.get("event") if isinstance(payload, dict) else None
            if event not in EVENTS:
                self._json_response(422, {"error": "unsupported event"})
                return
            log_path.parent.mkdir(parents=True, exist_ok=True)
            with log_path.open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(payload, ensure_ascii=False) + "\n")
            result = dispatcher.dispatch(event, payload)
            LOGGER.info("event=%s result=%s", event, result)
            self._json_response(200, {"status": "ok", "result": result})

        def log_message(self, fmt: str, *args) -> None:
            LOGGER.debug("http: " + fmt, *args)

    return AgentGuardHandler


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--allow-lan", action="store_true")
    parser.add_argument("--allow-lock", action="store_true")
    parser.add_argument("--log", type=Path,
                        default=Path("agentguard-events.jsonl"))
    parser.add_argument("--serial", help="USB serial device, e.g. /dev/ttyACM0")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    token = os.environ.get("AGENTGUARD_TOKEN", "")
    if len(token) < 16:
        raise SystemExit("set AGENTGUARD_TOKEN to a random value of 16+ characters")
    if args.host not in {"127.0.0.1", "::1", "localhost"} and not args.allow_lan:
        raise SystemExit("non-loopback binding requires --allow-lan")
    logging.basicConfig(level=logging.INFO,
                        format="%(asctime)s %(levelname)s %(message)s")
    dispatcher = ActionDispatcher(allow_lock=args.allow_lock)
    if args.serial:
        threading.Thread(target=serial_event_loop,
                         args=(args.serial, dispatcher, args.log),
                         daemon=True).start()
    server = ThreadingHTTPServer(
        (args.host, args.port), make_handler(token, dispatcher, args.log))
    LOGGER.info("listening on http://%s:%d (lock=%s)", args.host, args.port,
                args.allow_lock)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
