import http.client
import json
import select
import signal
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from http.server import ThreadingHTTPServer
from pathlib import Path

from agentguard_pc import (ActionDispatcher, SerialFrameDecoder,
                           dispatch_serial_line, make_handler,
                           parse_args, parse_serial_event)


class FakeShield:
    def __init__(self):
        self.visible = False

    def show(self):
        self.visible = True

    def hide(self):
        self.visible = False


class AgentGuardServerTest(unittest.TestCase):
    def setUp(self):
        self.tempdir = tempfile.TemporaryDirectory()
        self.shield = FakeShield()
        self.commands = []

        def runner(command, **kwargs):
            self.commands.append(command)

        dispatcher = ActionDispatcher(False, self.shield, runner)
        handler = make_handler("0123456789abcdef", dispatcher,
                               Path(self.tempdir.name) / "events.jsonl")
        self.server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
        self.thread = threading.Thread(target=self.server.serve_forever)
        self.thread.start()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.tempdir.cleanup()

    def request(self, event, token="0123456789abcdef"):
        body = json.dumps({"event": event})
        connection = http.client.HTTPConnection(*self.server.server_address)
        connection.request("POST", "/event", body,
                           {"Authorization": f"Bearer {token}",
                            "Content-Type": "application/json"})
        response = connection.getresponse()
        payload = response.read()
        connection.close()
        return response.status, payload

    def test_rejects_bad_token(self):
        self.assertEqual(self.request("blur_screen", "wrong")[0], 401)
        self.assertFalse(self.shield.visible)

    def test_privacy_shield_transitions(self):
        self.assertEqual(self.request("blur_screen")[0], 200)
        self.assertTrue(self.shield.visible)
        self.assertEqual(self.request("unblur_screen")[0], 200)
        self.assertFalse(self.shield.visible)

    def test_lock_is_disabled_by_default(self):
        status, payload = self.request("lock_screen")
        self.assertEqual(status, 200)
        self.assertIn(b"lock disabled", payload)
        self.assertEqual(self.commands, [])

    def test_rejects_unknown_event(self):
        self.assertEqual(self.request("run_arbitrary_command")[0], 422)

    def test_acknowledgement_is_logged_without_os_action(self):
        self.assertEqual(self.request("acknowledged")[0], 200)
        self.assertEqual(self.commands, [])

    def test_serial_event_dispatches_and_logs(self):
        path = Path(self.tempdir.name) / "serial.jsonl"
        dispatcher = ActionDispatcher(False, self.shield,
                                      lambda command, **kwargs: self.commands.append(command))
        line = 'AGENTGUARD_EVENT {"event":"sedentary_alert","monotonic_ms":20}'
        self.assertEqual(parse_serial_event(line)["event"], "sedentary_alert")
        self.assertTrue(dispatch_serial_line(line, dispatcher, path))
        record = json.loads(path.read_text(encoding="utf-8"))
        self.assertEqual(record["event"], "sedentary_alert")
        self.assertEqual(record["monotonic_ms"], 20)
        self.assertRegex(record["received_at"], r"Z$")
        self.assertEqual(
            self.commands[-1],
            ["notify-send", "--urgency=critical", "--expire-time=10000",
             "AgentGuard 久坐提醒", "请起身活动并让肩颈放松"])
        self.assertFalse(dispatch_serial_line("noise", dispatcher, path))
        self.assertFalse(dispatch_serial_line(
            'AGENTGUARD_EVENT {"event":"unknown"}', dispatcher, path))

    def test_serial_decoder_drops_oversized_frame_and_recovers(self):
        decoder = SerialFrameDecoder(max_frame_bytes=32)
        self.assertEqual(decoder.feed(b"x" * 33), [])
        self.assertEqual(decoder.buffered_bytes, 0)
        self.assertEqual(decoder.feed(
            b"discarded\nAGENTGUARD_EVENT {}\n"),
            ["AGENTGUARD_EVENT {}"])

    def test_serial_parser_rejects_non_string_and_deep_events(self):
        self.assertIsNone(parse_serial_event(
            'AGENTGUARD_EVENT {"event":[]}'))
        self.assertIsNone(parse_serial_event(
            'AGENTGUARD_EVENT {"event":{}}'))
        deep_json = "[" * 1200 + "]" * 1200
        self.assertIsNone(parse_serial_event(
            "AGENTGUARD_EVENT " + deep_json))


class AgentGuardArgumentTest(unittest.TestCase):
    def test_no_http_mode_is_available_for_usb_only_demo(self):
        args = parse_args(["--no-http", "--serial", "/dev/ttyACM7"])

        self.assertTrue(args.no_http)
        self.assertEqual(args.serial, "/dev/ttyACM7")

    def test_no_http_mode_exits_cleanly_on_ctrl_c(self):
        with tempfile.TemporaryDirectory() as directory:
            process = subprocess.Popen(
                [
                    sys.executable,
                    str(Path(__file__).with_name("agentguard_pc.py")),
                    "--no-http",
                    "--serial",
                    str(Path(directory) / "missing-device"),
                    "--log",
                    str(Path(directory) / "events.jsonl"),
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            stderr_lines = []
            deadline = time.monotonic() + 2
            while time.monotonic() < deadline:
                readable, _, _ = select.select(
                    [process.stderr], [], [], deadline - time.monotonic())
                if not readable:
                    break
                line = process.stderr.readline()
                stderr_lines.append(line)
                if "serial unavailable" in line:
                    break

            ready = any("serial unavailable" in line
                        for line in stderr_lines)
            if ready:
                process.send_signal(signal.SIGINT)
            else:
                process.terminate()
            _, remaining_stderr = process.communicate(timeout=2)
            stderr = "".join(stderr_lines) + remaining_stderr

        self.assertTrue(ready, stderr)
        self.assertEqual(process.returncode, 0, stderr)
        self.assertNotIn("Traceback", stderr)


if __name__ == "__main__":
    unittest.main()
