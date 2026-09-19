#!/usr/bin/env python3
"""Reset an ESP32 USB-Serial/JTAG target and capture its console safely."""

import argparse
import os
import sys
import time

import serial


def open_port(device: str, baud: int) -> serial.Serial:
    port = serial.Serial()
    port.port = device
    port.baudrate = baud
    port.timeout = 0.1
    port.write_timeout = 1
    port.dtr = False
    port.rts = False
    port.open()
    return port


def reconnect(device: str, baud: int, deadline: float) -> serial.Serial:
    last_error = None
    while time.monotonic() < deadline:
        if os.path.exists(device):
            try:
                return open_port(device, baud)
            except (OSError, serial.SerialException) as error:
                last_error = error
        time.sleep(0.1)
    raise RuntimeError(f"unable to open {device}: {last_error}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--device", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=float, default=15.0)
    parser.add_argument("--reset", action="store_true")
    parser.add_argument("--command", action="append", default=[])
    parser.add_argument("--command-delay", type=float, default=1.0)
    args = parser.parse_args()

    deadline = time.monotonic() + args.duration
    port = reconnect(args.device, args.baud, deadline)
    if args.reset:
        port.dtr = False
        port.rts = True
        time.sleep(0.2)
        port.rts = False
        time.sleep(0.2)

    pending = list(args.command)
    next_command = time.monotonic() + args.command_delay
    while time.monotonic() < deadline:
        try:
            data = port.read(port.in_waiting or 1)
            if data:
                sys.stdout.buffer.write(data)
                sys.stdout.buffer.flush()
            if pending and time.monotonic() >= next_command:
                command = pending.pop(0)
                port.write(command.encode("utf-8") + b"\r")
                port.flush()
                next_command = time.monotonic() + args.command_delay
        except (OSError, serial.SerialException):
            try:
                port.close()
            except serial.SerialException:
                pass
            port = reconnect(args.device, args.baud, deadline)

    port.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
