import csv
import json
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path

from event_stats import export_csv, format_text, read_event_stats


class EventStatsTest(unittest.TestCase):
    def test_counts_total_today_recent_and_bad_lines(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events.jsonl"
            records = [
                {"event": "sedentary_alert",
                 "received_at": "2026-09-18T01:00:00Z"},
                {"event": "sedentary_alert",
                 "received_at": "2026-09-17T01:00:00Z"},
                {"event": "acknowledged",
                 "received_at": "2026-09-18T02:00:00Z"},
                {"event": "blur_screen",
                 "received_at": "2026-09-18T03:00:00Z"},
                {"event": "unblur_screen",
                 "received_at": "2026-09-18T04:00:00Z"},
            ]
            path.write_text(
                "\n".join(json.dumps(record) for record in records) +
                "\nnot-json\n{}\n", encoding="utf-8")

            stats = read_event_stats(
                path, datetime(2026, 9, 18, 12, tzinfo=timezone.utc))

        self.assertEqual(stats["total_events"], 5)
        self.assertEqual(stats["sedentary_alerts"], 2)
        self.assertEqual(stats["acknowledgements"], 1)
        self.assertEqual(stats["privacy_triggers"], 1)
        self.assertEqual(stats["today_total"], 4)
        self.assertEqual(stats["today_sedentary_alerts"], 1)
        self.assertEqual(stats["last_event"], "unblur_screen")
        self.assertEqual(stats["malformed_lines"], 2)
        self.assertIn("累计: 久坐 2 / 确认 1 / 隐私 1",
                      format_text(path, stats))

    def test_old_records_remain_countable_without_timestamp(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events.jsonl"
            path.write_text('{"event":"acknowledged"}\n',
                            encoding="utf-8")
            stats = read_event_stats(path)

        self.assertEqual(stats["acknowledgements"], 1)
        self.assertEqual(stats["today_total"], 0)
        self.assertEqual(stats["last_event"], "acknowledged")

    def test_csv_export_is_spreadsheet_friendly(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "events.jsonl"
            destination = Path(directory) / "events.csv"
            source.write_text(
                '{"event":"blur_screen","received_at":"2026-09-18T03:00:00Z"}\n',
                encoding="utf-8")

            self.assertEqual(export_csv(source, destination), 1)
            with destination.open(encoding="utf-8-sig", newline="") as stream:
                rows = list(csv.reader(stream))

        self.assertEqual(rows[0], ["received_at", "event", "event_label"])
        self.assertEqual(rows[1][1:], ["blur_screen", "隐私遮罩"])


if __name__ == "__main__":
    unittest.main()
