import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from dashboard import build_payload, parse_args


class DashboardTest(unittest.TestCase):
    def test_rejects_non_loopback_host(self):
        with patch.object(sys, "argv", ["dashboard.py", "--host", "0.0.0.0"]):
            with self.assertRaises(SystemExit):
                parse_args()

    def test_missing_log_returns_empty_dashboard(self):
        with tempfile.TemporaryDirectory() as directory:
            payload = build_payload(Path(directory) / "missing.jsonl")

        self.assertEqual(payload["stats"]["total_events"], 0)
        self.assertEqual(payload["recent"], [])

    def test_recent_events_are_newest_first_and_limited(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events.jsonl"
            records = [
                {"event": "acknowledged",
                 "received_at": f"2026-09-18T00:00:{index:02d}Z"}
                for index in range(12)
            ]
            path.write_text("\n".join(json.dumps(item) for item in records),
                            encoding="utf-8")
            payload = build_payload(path)

        self.assertEqual(len(payload["recent"]), 10)
        self.assertEqual(payload["recent"][0]["received_at"],
                         "2026-09-18T00:00:11Z")
        self.assertEqual(payload["recent"][0]["label"], "用户确认")


if __name__ == "__main__":
    unittest.main()
