import json
import tempfile
import unittest
from pathlib import Path

from tools.import_codex_logs import convert_session, discover_sessions


def write_session(path: Path, meta: dict, events: list[dict]) -> None:
    rows = [
        {"timestamp": "2026-09-20T00:00:00Z", "type": "session_meta",
         "payload": meta},
        *events,
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "".join(json.dumps(row, ensure_ascii=False) + "\n" for row in rows),
        encoding="utf-8",
    )


class DiscoverSessionsTest(unittest.TestCase):
    def test_keeps_contest_roots_and_real_reviewers_only(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            contest = root / "contest"
            contest.mkdir()
            sessions = root / "sessions"
            write_session(sessions / "root.jsonl", {
                "id": "root", "timestamp": "2026-09-20T00:00:00Z",
                "cwd": str(contest), "source": "cli",
            }, [])
            write_session(sessions / "reviewer.jsonl", {
                "id": "reviewer", "timestamp": "2026-09-20T00:01:00Z",
                "cwd": str(contest), "parent_thread_id": "root",
                "source": {"subagent": {"thread_spawn": {"agent_path": "/root/review"}}},
            }, [])
            write_session(sessions / "guardian.jsonl", {
                "id": "guardian", "timestamp": "2026-09-20T00:02:00Z",
                "cwd": str(contest), "parent_thread_id": "root",
                "source": {"subagent": {"other": "guardian"}},
            }, [])
            write_session(sessions / "private.jsonl", {
                "id": "private", "timestamp": "2026-09-20T00:03:00Z",
                "cwd": str(root / "other"), "source": "cli",
            }, [])

            found = discover_sessions(sessions, contest)

            self.assertEqual([item.session_id for item in found], ["root", "reviewer"])


class ConvertSessionTest(unittest.TestCase):
    def test_converts_messages_pairs_tools_and_redacts_private_values(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "session.jsonl"
            write_session(path, {
                "id": "session-1", "timestamp": "2026-09-20T00:00:00Z",
                "cwd": "/workspace/contest", "source": "cli",
            }, [
                {"timestamp": "2026-09-20T00:00:01Z", "type": "response_item",
                 "payload": {"type": "message", "role": "developer",
                             "content": [{"type": "input_text", "text": "hidden"}]}},
                {"timestamp": "2026-09-20T00:00:02Z", "type": "response_item",
                 "payload": {"type": "message", "role": "user", "content": [
                     {"type": "input_text", "text":
                      "电话18600789808 邮箱yan20060509@163.com 北京工业大学"},
                     {"type": "image", "data": "secret-image"},
                 ]}},
                {"timestamp": "2026-09-20T00:00:03Z", "type": "response_item",
                 "payload": {"type": "custom_tool_call", "call_id": "call-1",
                             "name": "exec", "input":
                             "AGENTGUARD_TOKEN='agentguard-usb-local-demo'"}},
                {"timestamp": "2026-09-20T00:00:04Z", "type": "response_item",
                 "payload": {"type": "custom_tool_call_output", "call_id": "call-1",
                             "output": [{"type": "input_text", "text":
                             "ok agentguard-usb-local-demo 闫贺翔 "
                             "\\\"encrypted_content\\\":\\\"opaque-secret\\\" "
                             "gAAAAA0123456789abcdefghijklmnopqrstuvwxyz_ABCD"}]}},
                {"timestamp": "2026-09-20T00:00:05Z", "type": "response_item",
                 "payload": {"type": "reasoning", "encrypted_content": "private"}},
                {"timestamp": "2026-09-20T00:00:06Z", "type": "response_item",
                 "payload": {"type": "message", "role": "assistant", "content": [
                     {"type": "output_text", "text": "完成"},
                 ]}},
                {"timestamp": "2026-09-20T00:00:07Z", "type": "token_usage_record",
                 "payload": {"thread_token_usage": {"input_tokens": 100,
                             "output_tokens": 20, "total_tokens": 120}}},
            ])

            converted = convert_session(path, "contest2026_123_test", "yhx06")

            self.assertEqual([event["role"] for event in converted.events],
                             ["user", "tool", "assistant"])
            self.assertEqual([event["seq"] for event in converted.events], [0, 1, 2])
            self.assertEqual(converted.events[1]["tool_name"], "exec")
            self.assertTrue(converted.events[1]["output"].startswith("ok "))
            rendered = json.dumps(converted.events, ensure_ascii=False)
            for secret in ("18600789808", "yan20060509@163.com",
                           "北京工业大学", "agentguard-usb-local-demo",
                           "闫贺翔", "opaque-secret", "gAAAAA0123456789",
                           "secret-image",
                           "hidden", "private"):
                self.assertNotIn(secret, rendered)
            self.assertGreaterEqual(converted.redacted_count, 4)
            self.assertEqual(converted.tokens_in, 100)
            self.assertEqual(converted.tokens_out, 20)
            self.assertEqual(converted.tokens_total, 120)


if __name__ == "__main__":
    unittest.main()
