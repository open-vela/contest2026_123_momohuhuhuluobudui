#!/usr/bin/env python3
"""Capture a USB CDC console across re-enumeration without touching DTR/RTS."""

import argparse
import errno
import os
import select
import sys
import termios
import time


def open_raw(device: str, baud: int) -> int:
    speeds = {115200: termios.B115200}
    if baud not in speeds:
        raise ValueError(f"unsupported baud rate: {baud}")

    fd = os.open(device, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0
    attrs[4] = speeds[baud]
    attrs[5] = speeds[baud]
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 1
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    return fd


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--device", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=float, default=120.0)
    parser.add_argument("--command", action="append", default=[])
    parser.add_argument("--command-delay", type=float, default=1.0)
    args = parser.parse_args()

    deadline = time.monotonic() + args.duration
    fd = None
    saw_prompt = False
    pending = list(args.command)
    next_command = 0.0
    next_probe = time.monotonic()
    next_reopen = time.monotonic() + 1.0
    recent = bytearray()

    while time.monotonic() < deadline:
        if fd is None:
            try:
                fd = open_raw(args.device, args.baud)
            except (FileNotFoundError, PermissionError, OSError):
                time.sleep(0.05)
                continue

        try:
            readable, _, _ = select.select([fd], [], [], 0.1)
            if readable:
                data = os.read(fd, 4096)
                if not data:
                    raise OSError(errno.ENODEV, "serial device disconnected")
                sys.stdout.buffer.write(data)
                sys.stdout.buffer.flush()
                recent.extend(data)
                if len(recent) > 1024:
                    del recent[:-1024]
                if b"nsh>" in recent and not saw_prompt:
                    saw_prompt = True
                    next_command = time.monotonic() + args.command_delay

            if saw_prompt and pending and time.monotonic() >= next_command:
                os.write(fd, pending.pop(0).encode("utf-8") + b"\r")
                next_command = time.monotonic() + args.command_delay
            elif not saw_prompt and time.monotonic() >= next_probe:
                os.write(fd, b"\r")
                next_probe = time.monotonic() + 1.0
            if not saw_prompt and time.monotonic() >= next_reopen:
                os.close(fd)
                fd = None
                next_reopen = time.monotonic() + 1.0
        except OSError:
            if fd is not None:
                os.close(fd)
            fd = None
            saw_prompt = False
            recent.clear()
            next_probe = time.monotonic()
            next_reopen = time.monotonic() + 1.0
            time.sleep(0.05)

    if fd is not None:
        os.close(fd)
    return 0 if saw_prompt else 2


if __name__ == "__main__":
    raise SystemExit(main())
