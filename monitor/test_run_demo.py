import unittest
from pathlib import Path

from run_demo import build_commands


class RunDemoTest(unittest.TestCase):
    def test_builds_listener_and_dashboard_commands(self):
        listener, dashboard = build_commands(
            Path("/opt/agentguard/monitor"), "/usr/bin/python3",
            "/dev/ttyACM7", Path("/tmp/events.jsonl"), 9876)

        self.assertEqual(listener[-5:], [
            "--no-http", "--serial", "/dev/ttyACM7",
            "--log", "/tmp/events.jsonl"])
        self.assertEqual(dashboard[-4:], [
            "--log", "/tmp/events.jsonl", "--port", "9876"])


if __name__ == "__main__":
    unittest.main()
