import os
import tempfile
import unittest
from pathlib import Path

from run_usb import build_launch, default_log_path


class RunUsbTest(unittest.TestCase):
    def test_default_log_uses_xdg_state_directory(self):
        path = default_log_path(
            {"XDG_STATE_HOME": "/tmp/agentguard-state"},
            Path("/home/demo"),
        )

        self.assertEqual(
            path,
            Path("/tmp/agentguard-state/agentguard/events.jsonl"),
        )

    def test_default_log_falls_back_to_home_state_directory(self):
        path = default_log_path({}, Path("/home/demo"))

        self.assertEqual(
            path,
            Path("/home/demo/.local/state/agentguard/events.jsonl"),
        )

    def test_build_launch_uses_listener_next_to_launcher(self):
        with tempfile.TemporaryDirectory() as directory:
            monitor_dir = Path(directory)
            listener = monitor_dir / "agentguard_pc.py"
            listener.touch()

            command, environment, generated = build_launch(
                monitor_dir=monitor_dir,
                python_executable="/usr/bin/python3",
                serial_device="/dev/ttyACM7",
                log_path=Path("/tmp/events.jsonl"),
                environment={},
            )

        self.assertEqual(
            command,
            [
                "/usr/bin/python3",
                str(listener),
                "--no-http",
                "--serial",
                "/dev/ttyACM7",
                "--log",
                "/tmp/events.jsonl",
            ],
        )
        self.assertGreaterEqual(len(environment["AGENTGUARD_TOKEN"]), 16)
        self.assertTrue(generated)

    def test_build_launch_preserves_configured_token(self):
        token = "configured-token-1234"
        with tempfile.TemporaryDirectory() as directory:
            monitor_dir = Path(directory)
            (monitor_dir / "agentguard_pc.py").touch()

            _, environment, generated = build_launch(
                monitor_dir=monitor_dir,
                python_executable="python3",
                serial_device="/dev/ttyACM0",
                log_path=Path("events.jsonl"),
                environment={"AGENTGUARD_TOKEN": token, "DISPLAY": ":0"},
            )

        self.assertEqual(environment["AGENTGUARD_TOKEN"], token)
        self.assertEqual(environment["DISPLAY"], ":0")
        self.assertFalse(generated)

    def test_build_launch_rejects_missing_listener(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(FileNotFoundError, "agentguard_pc.py"):
                build_launch(
                    monitor_dir=Path(directory),
                    python_executable="python3",
                    serial_device="/dev/ttyACM0",
                    log_path=Path("events.jsonl"),
                    environment=os.environ,
                )


if __name__ == "__main__":
    unittest.main()
