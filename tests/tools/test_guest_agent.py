"""tools/guest/guest_agent.py against a socket that answers each request line
with scripted bytes: the answers a real agent gives, and every way one can be
wrong."""
import base64
import json
import os
import pathlib
import socket
import sys
import tempfile
import threading
import time
import unittest

from tool_run import ROOT

sys.path.insert(0, str(ROOT / "tools" / "guest"))
from guest_agent import PULL_CHUNK, AgentError, GuestAgent, Qmp, wait_for_agent  # noqa: E402


class ScriptedPeer:
    """Accepts one connection, sends `greeting`, then answers the Nth request
    line with replies[N] - bytes, or a callable of the request - and closes
    once the script runs out."""

    def __init__(self, path, replies, greeting=b""):
        self.server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.server.bind(path)
        self.server.listen(1)
        self.replies = list(replies)
        self.greeting = greeting
        self.requests = []
        threading.Thread(target=self.serve, daemon=True).start()

    def serve(self):
        connection, _ = self.server.accept()
        connection.sendall(self.greeting)
        buffer = b""
        while self.replies:
            data = connection.recv(65536)
            if not data:
                break
            buffer += data
            while b"\n" in buffer and self.replies:
                line, buffer = buffer.split(b"\n", 1)
                request = json.loads(line.lstrip(b"\xff"))
                self.requests.append(request)
                reply = self.replies.pop(0)
                connection.sendall(reply(request) if callable(reply) else reply)
        connection.close()
        self.server.close()


def answer(value):
    return json.dumps({"return": value}).encode() + b"\n"


def synced(request):
    return b"stale junk\xff" + answer(request["arguments"]["id"] + 1) + b"\xff" + answer(request["arguments"]["id"])


class GuestAgentClient(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.mkdtemp()
        self.path = os.path.join(self.directory, "agent.sock")

    def tearDown(self):
        for name in os.listdir(self.directory):
            os.unlink(os.path.join(self.directory, name))
        os.rmdir(self.directory)

    def test_sync_skips_stale_answers_and_garbage_then_commands_answer(self):
        peer = ScriptedPeer(self.path, [synced, b"not json\n[3]\n" + answer({})])
        agent = GuestAgent(self.path, 2.0)
        agent.sync()
        self.assertEqual(agent.execute("guest-ping"), {})
        self.assertEqual(peer.requests[1], {"execute": "guest-ping"})
        agent.close()

    def test_runs_a_process_until_it_exits_and_decodes_its_output(self):
        output = base64.b64encode(b"hello").decode()
        peer = ScriptedPeer(self.path, [answer({"pid": 7}), answer({"exited": False}),
                                        answer({"exited": True, "exitcode": 3, "out-data": output})])
        agent = GuestAgent(self.path, 2.0)
        self.assertEqual(agent.run("C:\\av\\avtest.exe", ["-x"], 5.0, poll=0.01), (3, b"hello", b""))
        self.assertEqual(peer.requests[0]["arguments"], {"path": "C:\\av\\avtest.exe", "arg": ["-x"],
                                                         "capture-output": True})
        agent.close()

    def test_process_answers_that_are_not_what_was_asked_raise(self):
        cases = [
            ([answer(5)], "guest-exec answered 5"),
            ([answer({"pid": 1}), answer("gone")], "guest-exec-status answered gone"),
            ([answer({"pid": 1}), answer({"exited": True, "out-data": "@@@"})], "undecodable process output"),
            ([answer({"pid": 1}), answer({"exited": False}), answer({"exited": False})], "still running after"),
        ]
        for index, (replies, message) in enumerate(cases):
            path = os.path.join(self.directory, f"case{index}.sock")
            ScriptedPeer(path, replies)
            agent = GuestAgent(path, 2.0)
            with self.assertRaises(AgentError) as raised:
                agent.run("program", [], 0.0, poll=0.01)
            self.assertIn(message, str(raised.exception))
            agent.close()

    def test_pushes_in_chunks_and_a_short_write_raises_but_still_closes(self):
        data = bytes(range(256)) * 200
        chunks = [answer({"count": 48 * 1024}), answer({"count": len(data) - 48 * 1024})]
        peer = ScriptedPeer(self.path, [answer(9), *chunks, answer({})])
        agent = GuestAgent(self.path, 2.0)
        agent.push(data, "C:\\av\\file")
        written = b"".join(base64.b64decode(r["arguments"]["buf-b64"]) for r in peer.requests[1:3])
        self.assertEqual(written, data)
        self.assertEqual(peer.requests[-1], {"execute": "guest-file-close", "arguments": {"handle": 9}})
        agent.close()
        short = os.path.join(self.directory, "short.sock")
        peer = ScriptedPeer(short, [answer(9), answer({"count": 1}), answer({})])
        agent = GuestAgent(short, 2.0)
        with self.assertRaises(AgentError) as raised:
            agent.push(b"abc", "C:\\av\\file")
        self.assertIn("short write", str(raised.exception))
        self.assertEqual(peer.requests[-1]["execute"], "guest-file-close")
        agent.close()

    def test_errors_non_answers_silence_and_a_closed_connection_raise(self):
        cases = [
            (b'{"error": {"class": "GenericError", "desc": "denied"}}\n', "guest agent error: denied"),
            (b'{"error": "plain"}\n', "guest agent error: plain"),
            (b'{"event": "odd"}\n', "not an answer"),
            (None, "connection closed"),
        ]
        for index, (reply, message) in enumerate(cases):
            path = os.path.join(self.directory, f"error{index}.sock")
            ScriptedPeer(path, [reply] if reply is not None else [])
            agent = GuestAgent(path, 0.5)
            with self.assertRaises(AgentError) as raised:
                agent.execute("guest-ping")
            self.assertIn(message, str(raised.exception))
            agent.close()
        silent = os.path.join(self.directory, "silent.sock")
        ScriptedPeer(silent, [b"", b""])
        agent = GuestAgent(silent, 0.2)
        with self.assertRaises(AgentError) as raised:
            agent.execute("guest-ping")
        self.assertIn("timed out", str(raised.exception))
        with self.assertRaises(AgentError):
            agent.sync()
        agent.close()
        with self.assertRaises(AgentError) as raised:
            GuestAgent(os.path.join(self.directory, "absent.sock"), 0.2)
        self.assertIn("cannot connect", str(raised.exception))

    def test_a_send_to_a_closed_peer_raises(self):
        ScriptedPeer(self.path, [])
        agent = GuestAgent(self.path, 0.5)
        time.sleep(0.1)
        with self.assertRaises(AgentError):
            for _ in range(50):
                agent.notify("guest-shutdown", {"mode": "reboot"})
                time.sleep(0.01)
        agent.close()

    def test_waiting_for_the_agent_retries_until_it_answers_or_the_deadline(self):
        with self.assertRaises(AgentError) as raised:
            wait_for_agent(self.path, time.monotonic() + 0.3, 0.2, pause=0.05)
        self.assertIn("never answered: None", str(raised.exception))
        ScriptedPeer(self.path, [b"\xff" + answer(-1)])
        with self.assertRaises(AgentError) as raised:
            wait_for_agent(self.path, time.monotonic() + 0.6, 0.2, pause=0.05)
        self.assertIn("never answered", str(raised.exception))
        os.unlink(self.path)
        ScriptedPeer(self.path, [synced, answer({})])
        agent = wait_for_agent(self.path, time.monotonic() + 5.0, 1.5, pause=0.05)
        self.assertEqual(agent.timeout, 1.5)
        agent.close()

    def test_waiting_for_the_agent_stops_as_soon_as_it_is_abandoned(self):
        reasons = iter([None, "the guest panicked while booting"])
        started = time.monotonic()
        with self.assertRaises(AgentError) as raised:
            wait_for_agent(self.path, started + 5.0, 0.2, pause=0.05, abandoned=lambda: next(reasons))
        self.assertIn("the guest panicked while booting", str(raised.exception))
        self.assertLess(time.monotonic() - started, 1.0)

    def test_pulls_until_end_of_file_into_a_local_file_and_closes(self):
        pieces = [answer({"count": 3, "buf-b64": base64.b64encode(b"abc").decode(), "eof": False}),
                  answer({"count": 2, "buf-b64": base64.b64encode(b"de").decode(), "eof": True})]
        peer = ScriptedPeer(self.path, [answer(5), *pieces, answer({})])
        agent = GuestAgent(self.path, 2.0)
        local = os.path.join(self.directory, "pulled.bin")
        self.assertEqual(agent.pull("C:\\Windows\\MEMORY.DMP", local), 5)
        self.assertEqual(pathlib.Path(local).read_bytes(), b"abcde")
        self.assertEqual(peer.requests[0]["arguments"], {"path": "C:\\Windows\\MEMORY.DMP", "mode": "rb"})
        self.assertEqual(peer.requests[1]["arguments"], {"handle": 5, "count": PULL_CHUNK})
        self.assertEqual(peer.requests[-1], {"execute": "guest-file-close", "arguments": {"handle": 5}})
        agent.close()
        os.unlink(local)

    def test_a_pull_that_reads_nothing_stops_and_a_bad_read_raises_but_still_closes(self):
        ScriptedPeer(self.path, [answer(5), answer({"count": 0, "buf-b64": "", "eof": False}), answer({})])
        agent = GuestAgent(self.path, 2.0)
        local = os.path.join(self.directory, "empty.bin")
        self.assertEqual(agent.pull("C:\\empty", local, chunk=16), 0)
        agent.close()
        os.unlink(local)
        for index, reply in enumerate([answer("nope"), answer({"count": 1, "buf-b64": "@@@"})]):
            path = os.path.join(self.directory, f"bad{index}.sock")
            peer = ScriptedPeer(path, [answer(5), reply, answer({})])
            agent = GuestAgent(path, 2.0)
            with self.assertRaises(AgentError) as raised:
                agent.pull("C:\\bad", os.path.join(self.directory, "bad.bin"))
            self.assertIn("answered nope" if index == 0 else "undecodable", str(raised.exception))
            self.assertEqual(peer.requests[-1]["execute"], "guest-file-close")
            agent.close()
        os.unlink(os.path.join(self.directory, "bad.bin"))


class QmpClient(unittest.TestCase):
    def setUp(self):
        self.directory = pathlib.Path(tempfile.mkdtemp())
        self.path = str(self.directory / "qmp.sock")

    def tearDown(self):
        for entry in self.directory.iterdir():
            entry.unlink()
        self.directory.rmdir()

    def test_negotiates_then_keeps_events_seen_while_waiting_for_answers(self):
        greeting = json.dumps({"QMP": {"version": {}}}).encode() + b"\n"
        event = json.dumps({"event": "RESET"}).encode() + b"\n"
        stop = json.dumps({"event": "STOP"}).encode() + b"\n"
        ScriptedPeer(self.path, [event + answer({}), stop + answer({}), answer({}) + stop + event], greeting=greeting)
        qmp = Qmp(self.path, 1.0)
        qmp.wait_event("RESET", 1.0)   # seen during negotiation
        self.assertEqual(qmp.execute("cont"), {})
        self.assertEqual([message["event"] for message in qmp.pending], ["STOP"])
        qmp.forget_events("STOP")
        qmp.execute("query-status")
        self.assertEqual(qmp.wait_event("RESET", 1.0), {"event": "RESET"})   # read past a STOP
        self.assertEqual([message["event"] for message in qmp.pending], ["STOP"])
        with self.assertRaises(AgentError):
            qmp.wait_event("SHUTDOWN", 0.2)
        qmp.close()

    def test_drains_queued_events_until_quiet_then_takes_them_by_name(self):
        greeting = json.dumps({"QMP": {"version": {}}}).encode() + b"\n"
        panic = {"event": "GUEST_PANICKED", "data": {"action": "run"}}
        events = b"".join(json.dumps(event).encode() + b"\n" for event in (panic, {"event": "RESET"}))
        ScriptedPeer(self.path, [answer({}) + events + b"garbage\n", b""], greeting=greeting)
        qmp = Qmp(self.path, 1.0)
        qmp.drain(0.2)
        self.assertTrue(qmp.seen("GUEST_PANICKED"))
        self.assertEqual(qmp.take_events("GUEST_PANICKED"), [panic])
        self.assertFalse(qmp.seen("GUEST_PANICKED"))
        self.assertEqual(qmp.take_events("GUEST_PANICKED"), [])
        self.assertEqual(qmp.wait_event("RESET", 0.1), {"event": "RESET"})
        qmp.close()

    def test_draining_a_qemu_that_went_away_raises(self):
        greeting = json.dumps({"QMP": {"version": {}}}).encode() + b"\n"
        ScriptedPeer(self.path, [answer({})], greeting=greeting)
        qmp = Qmp(self.path, 1.0)
        with self.assertRaises(AgentError) as raised:
            qmp.drain(1.0)
        self.assertIn("connection closed", str(raised.exception))
        qmp.close()

    def test_refuses_a_peer_that_is_not_qmp(self):
        ScriptedPeer(self.path, [], greeting=b'{"hello": 1}\n')
        with self.assertRaises(AgentError) as raised:
            Qmp(self.path, 1.0)
        self.assertIn("not a QMP greeting", str(raised.exception))


if __name__ == "__main__":
    unittest.main()
