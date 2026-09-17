import json
import tempfile
import unittest
from pathlib import Path

from event_stats import format_text, read_event_stats


class EventStatsTest(unittest.TestCase):
    def test_counts_product_events_and_tolerates_bad_lines(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events.jsonl"
            records = [
                {"event": "sedentary_alert"},
                {"event": "sedentary_alert"},
                {"event": "acknowledged"},
                {"event": "blur_screen"},
                {"event": "unblur_screen"},
            ]
            path.write_text(
                "\n".join(json.dumps(record) for record in records) +
                "\nnot-json\n{}\n", encoding="utf-8")

            stats = read_event_stats(path)

        self.assertEqual(stats["total_events"], 5)
        self.assertEqual(stats["sedentary_alerts"], 2)
        self.assertEqual(stats["acknowledgements"], 1)
        self.assertEqual(stats["privacy_triggers"], 1)
        self.assertEqual(stats["malformed_lines"], 2)
        self.assertIn("久坐提醒次数: 2", format_text(path, stats))


if __name__ == "__main__":
    unittest.main()
