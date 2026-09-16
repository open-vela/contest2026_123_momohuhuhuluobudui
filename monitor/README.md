# AgentGuard PC agent

`agentguard_pc.py` accepts only `/event` requests carrying the configured
Bearer token. It supports desktop notifications, an opaque privacy shield and
optional OS screen locking.

```bash
export AGENTGUARD_TOKEN='a-random-value-with-at-least-16-characters'
python3 agentguard_pc.py --host 0.0.0.0 --allow-lan
```

For the submission demo, USB serial avoids Wi-Fi setup:

```bash
cd monitor
python3 run_usb.py
```

The launcher finds `agentguard_pc.py` relative to itself, generates a temporary
local token when `AGENTGUARD_TOKEN` is unset, and stores events in
`~/.local/state/agentguard/events.jsonl` (or `$XDG_STATE_HOME/agentguard`). Use
`python3 run_usb.py --serial /dev/ttyACM1` for a different device. If the device
is temporarily absent, leave the launcher running: it waits and reconnects
automatically. It uses USB-only mode, so an occupied HTTP port cannot stop the
demo. Press Ctrl+C to stop it.

The equivalent manual command remains available when custom HTTP options are
needed:

```bash
export AGENTGUARD_TOKEN='a-random-value-with-at-least-16-characters'
python3 agentguard_pc.py --serial /dev/ttyACM0 --log /tmp/events.jsonl
```

Keep this process running, enable `DEMO` with a three-second BOOT hold, and
remain in view for 20 seconds. A `sedentary_alert` produces a desktop
notification and one JSONL record. Serial input accepts only lines beginning
with `AGENTGUARD_EVENT ` and only the same five allowlisted actions as HTTP.
The device sends these records on a best-effort, non-blocking basis so a busy
console can never stall camera inference. The Linux notification is marked
critical and remains visible for 10 seconds. Opening the port uses the Python
standard library and does not issue explicit DTR/RTS control operations;
USB passthrough environments can still briefly re-enumerate the device.

Screen locking is intentionally disabled unless `--allow-lock` is passed.
Use `--log PATH` to select the received JSONL event log.

Run tests with:

```bash
python3 -m unittest -v test_agentguard_pc.py test_run_usb.py
```

The old `sit_reminder_monitor.py` is retained as an early prototype and should
not be used for the final privacy-sensitive workflow because it has no request
authentication or event allowlist.
