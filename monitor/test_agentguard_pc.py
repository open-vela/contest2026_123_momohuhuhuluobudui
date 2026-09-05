import http.client
import json
import tempfile
import threading
import unittest
from http.server import ThreadingHTTPServer
from pathlib import Path

from agentguard_pc import ActionDispatcher, make_handler


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


if __name__ == "__main__":
    unittest.main()
