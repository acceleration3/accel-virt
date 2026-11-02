import base64
import json
import os
import pathlib
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest

from tool_run import ROOT

sys.path.insert(0, str(ROOT / "tools" / "mcp"))
import av_inspect  # noqa: E402
import trace_format  # noqa: E402

SERVER = ROOT / "tools" / "mcp" / "av_inspect.py"
FAKE_QEMU = ROOT / "tests" / "tools" / "fake_guest" / "fake_qemu.py"
ACTIVE = 1 << 63
MS = 10_000


def header(slots, module=b"avvk", process=4242, crash=None, **overrides):
    fields = {"magic": b"AVTRACE\0", "version": 1, "header": 96, "slots": slots, "slot_bytes": 824, "frames": 32,
              "frame_bytes": 24, "module": module.ljust(16, b"\0")}
    fields.update(overrides)
    code, address, thread, at = crash or (0, 0, 0, 0)
    data = (fields["magic"] + struct.pack("<IIIIII", fields["version"], fields["header"], fields["slots"],
                                          fields["slot_bytes"], fields["frames"], fields["frame_bytes"])
            + fields["module"] + struct.pack("<II", process, thread) + struct.pack("<QQ", 5, 10_000_000)
            + struct.pack("<II", code, 0) + struct.pack("<QQ", address, at))
    assert len(data) == 96
    return data


def slot(owner=0, depth=0, thread=0, entries=0, last_left=0, left_at=0, waiting=0, holding=0, frames=()):
    data = struct.pack("<QIIQIIQQQ", owner, depth, thread, entries, last_left, 0, left_at, waiting, holding)
    for name, level, entered in frames:
        data += struct.pack("<IHHQQ", name, level, 0, entered, 0x7FF600000000 + name)
    return data.ljust(824, b"\0")


def snapshot(kind, clock, payload):
    return b"AVSNAP1\0" + struct.pack("<QII", clock, kind, len(payload)) + payload


RUN = trace_format.fnv1a("IcdRoot::destroy_instance")
SEND = trace_format.fnv1a("KmtTransport::send_sync")
WAIT = trace_format.fnv1a("GdiKmt::wait")
NAMES = {RUN: "IcdRoot::destroy_instance", SEND: "KmtTransport::send_sync", WAIT: "GdiKmt::wait"}


def busy_table():
    return (header(4)
            + slot(owner=0x11 | ACTIVE, depth=2, thread=11, entries=40, waiting=0x5000,
                   frames=[(RUN, 0, 100 * MS), (SEND, 0, 3_000 * MS)])
            + slot(owner=0x12 | ACTIVE, depth=1, thread=12, entries=3, holding=0x5000,
                   frames=[(WAIT, 0, 9_000 * MS)])
            + slot()
            + slot(owner=0x13, thread=13, entries=9, last_left=RUN, left_at=8_000 * MS))


class TraceFormat(unittest.TestCase):
    def test_fnv1a_is_the_published_function(self):
        self.assertEqual(trace_format.fnv1a(""), 0x811C9DC5)
        self.assertEqual(trace_format.fnv1a("a"), 0xE40C292C)
        self.assertEqual(trace_format.fnv1a("foobar"), 0xBF9CF968)

    def test_a_table_reads_back_every_documented_field(self):
        table = trace_format.parse_table(header(2, crash=(0xC0000005, 0x140001000, 12, 77))
                                         + slot(owner=0x11 | ACTIVE, depth=34, thread=11, entries=40,
                                                waiting=0x5000, holding=0x6000,
                                                frames=[(RUN, 2, 100)] * 32)
                                         + slot())
        self.assertEqual((table["module"], table["process"], table["created"], table["ticks_per_second"]),
                         ("avvk", 4242, 5, 10_000_000))
        self.assertEqual(table["crash"], {"code": 0xC0000005, "address": 0x140001000, "thread": 12, "at": 77})
        self.assertEqual(len(table["slots"]), 1)
        read = table["slots"][0]
        self.assertEqual((read["key"], read["active"], read["depth"], read["thread"], read["entries"],
                          read["waiting_on"], read["holding"]), (0x11, True, 34, 11, 40, 0x5000, 0x6000))
        self.assertEqual(len(read["frames"]), 32)
        self.assertEqual(read["frames"][31], {"name_id": RUN, "level": 2, "entered": 100,
                                              "text": 0x7FF600000000 + RUN})
        self.assertNotIn("crash", trace_format.parse_table(header(0)))

    def test_every_broken_header_field_is_refused_by_name(self):
        cases = {"magic": header(1, magic=b"AVTRACX\0"), "version": header(1, version=2),
                 "header bytes": header(1, header=97), "slot bytes": header(1, slot_bytes=823),
                 "frames per slot": header(1, frames=31), "frame bytes": header(1, frame_bytes=25),
                 "slot count": header(257)}
        for field, data in cases.items():
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, f"^{field}:"):
                trace_format.parse_table(data + slot() * 2)
        with self.assertRaisesRegex(ValueError, "^slot count: 2 slots do not fit"):
            trace_format.parse_table(header(2) + slot() + slot()[:-1])
        with self.assertRaisesRegex(ValueError, "^module:"):
            trace_format.parse_table(header(1, module=b"m" * 16) + slot())
        with self.assertRaisesRegex(ValueError, "^header:"):
            trace_format.parse_table(header(1)[:95])
        self.assertEqual(trace_format.parse_table(header(1, module=b"fifteen-letters") + slot())["module"],
                         "fifteen-letters")

    def test_snapshots_are_refused_unless_whole(self):
        self.assertEqual(trace_format.parse_snapshot(snapshot(2, 99, b"a/0 k=v") + b"tail"),
                         ("state", 99, b"a/0 k=v"))
        self.assertEqual(trace_format.parse_snapshot(snapshot(3, 0, b"no answer"))[0], "error")
        for broken, reason in ((snapshot(1, 0, b"x")[:-1], "length"), (b"AVSNAP2\0" + snapshot(1, 0, b"")[8:], "magic"),
                               (snapshot(4, 0, b""), "kind"), (b"AVSNAP1\0", "shorter")):
            with self.subTest(reason=reason), self.assertRaisesRegex(ValueError, reason):
                trace_format.parse_snapshot(broken)

    def test_a_state_dump_is_one_entry_per_object(self):
        entries = trace_format.parse_state("connection/0 luid=0x00A1 adapter=3\r\n\n"
                                           "connection/0/handle/12 kind=device lost= \nadapter/1")
        self.assertEqual(entries, [
            {"path": "connection/0", "kinds": ["connection"], "fields": {"luid": "0x00A1", "adapter": "3"}},
            {"path": "connection/0/handle/12", "kinds": ["connection", "handle"],
             "fields": {"kind": "device", "lost": ""}},
            {"path": "adapter/1", "kinds": ["adapter"], "fields": {}}])

    def test_a_reader_names_frames_times_them_and_finds_who_holds_a_lock(self):
        described = trace_format.describe_threads(trace_format.parse_table(busy_table()), NAMES,
                                                  now=10_000 * MS, stall_seconds=5)
        waiting, holding, idle = described["threads"]
        self.assertEqual([frame["name"] for frame in waiting["frames"]],
                         ["IcdRoot::destroy_instance", "KmtTransport::send_sync"])
        self.assertEqual([frame["age_ms"] for frame in waiting["frames"]], [9_900, 7_000])
        self.assertEqual((waiting["waiting_on"], waiting["lock_held_by"], waiting["stalled"]), ("0x5000", [12], True))
        self.assertEqual((holding["holding"], holding["stalled"], holding["frames"][0]["age_ms"]),
                         ("0x5000", False, 1_000))
        self.assertNotIn("waiting_on", holding)
        self.assertEqual((idle["active"], idle["last_left"], idle["left_ms_ago"], idle["stalled"]),
                         (False, "IcdRoot::destroy_instance", 2_000, False))
        self.assertEqual((waiting["state"], holding["state"], idle["state"]), ("blocked", "running", "idle"))
        self.assertEqual(described["module"], "avvk")

    def test_a_thread_parked_on_something_no_thread_holds_is_waiting_not_stalled(self):
        parked = trace_format.parse_table(header(2)
                                          + slot(owner=0x5 | ACTIVE, depth=1, thread=5, waiting=0x7000,
                                                 frames=[(RUN, 0, 1_000 * MS)])
                                          + slot(owner=0x6 | ACTIVE, depth=1, thread=6,
                                                 frames=[(RUN, 0, 1_000 * MS)]))
        waiting, busy = trace_format.describe_threads(parked, NAMES, now=10_000 * MS, stall_seconds=5)["threads"]
        self.assertEqual((waiting["state"], waiting["stalled"], waiting["lock_held_by"]), ("waiting", False, []))
        self.assertEqual((busy["state"], busy["stalled"]), ("stalled", True))

    def test_a_stall_is_judged_at_the_threshold_and_unknown_names_and_times_are_shown(self):
        table = trace_format.parse_table(header(1, crash=(11, 0x10, 5, 1_000 * MS))
                                         + slot(owner=0x5 | ACTIVE, depth=40, thread=5,
                                                frames=[(0xABCDEF01, 15, 5_000 * MS), (RUN, 0, 0)]))
        described = trace_format.describe_threads(table, NAMES, now=10_000 * MS, stall_seconds=5)
        thread = described["threads"][0]
        self.assertEqual(thread["frames"][0]["name"], "#abcdef01")
        self.assertEqual(thread["frames"][0]["level"], 15)
        self.assertIsNone(thread["frames"][1]["age_ms"])
        self.assertFalse(thread["stalled"])
        self.assertEqual((len(thread["frames"]), thread["unstored_frames"]), (32, 8))
        self.assertEqual(described["crash"], {"code": "0xb", "address": "0x10", "thread": 5, "ms_ago": 9_000})
        exactly = trace_format.parse_table(header(1) + slot(owner=0x5 | ACTIVE, depth=1, thread=5,
                                                            frames=[(RUN, 0, 5_000 * MS)]))
        self.assertTrue(trace_format.describe_threads(exactly, NAMES, 10_000 * MS, 5)["threads"][0]["stalled"])
        self.assertFalse(trace_format.describe_threads(exactly, NAMES, 10_000 * MS - 1, 5)["threads"][0]["stalled"])
        self.assertIsNone(trace_format.describe_threads(exactly, NAMES, 1, 5)["threads"][0]["frames"][0]["age_ms"])

    def test_without_a_clock_ages_run_from_the_newest_stamp(self):
        described = trace_format.describe_threads(trace_format.parse_table(busy_table()), NAMES)
        self.assertEqual([frame["age_ms"] for frame in described["threads"][0]["frames"]], [8_900, 6_000])
        self.assertEqual(trace_format.describe_threads(trace_format.parse_table(header(0)), NAMES)["threads"], [])


class ServerLoop(unittest.TestCase):
    def setUp(self):
        self.scratch = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)
        self.shm = self.scratch / "shm"
        self.shm.mkdir()
        self.root = self.scratch / "root"
        (self.root / "src" / "icd").mkdir(parents=True)
        (self.root / "src" / "icd" / "root.cpp").write_text(
            'void IcdRoot::destroy_instance()\n{\n    AV_SCOPE("IcdRoot::destroy_instance");\n}\n'
            'void GdiKmt::wait()\n{\n    AV_SCOPE("GdiKmt::wait");\n}\n')
        (self.root / "build-guest-x").mkdir()
        (self.root / "build-guest-x" / "avgpu.log").write_text("avgpu: DriverEntry\navgpu: vsync 1\n"
                                                                "avgpu: a\n")
        environment = dict(os.environ, AV_INSPECT_SHM=str(self.shm), AV_INSPECT_ROOT=str(self.root))
        self.server = subprocess.Popen([sys.executable, "-B", str(SERVER)], stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, text=True, env=environment)
        self.addCleanup(self.stop)
        self.next_id = 0

    def stop(self):
        self.server.stdin.close()
        self.server.wait(timeout=10)
        self.server.stdout.close()

    def request(self, method, params=None, notify=False):
        message = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            message["params"] = params
        if not notify:
            self.next_id += 1
            message["id"] = self.next_id
        self.server.stdin.write(json.dumps(message) + "\n")
        self.server.stdin.flush()
        if notify:
            return None
        answer = json.loads(self.server.stdout.readline())
        self.assertEqual((answer["jsonrpc"], answer["id"]), ("2.0", self.next_id))
        return answer

    def call(self, name, **arguments):
        result = self.request("tools/call", {"name": name, "arguments": arguments})["result"]
        text = result["content"][0]["text"]
        if result["isError"]:
            return True, text
        try:
            return False, json.loads(text)
        except ValueError:
            return False, text

    def test_initialize_lists_every_tool_with_a_schema(self):
        answer = self.request("initialize", {"protocolVersion": "2025-03-26", "capabilities": {},
                                             "clientInfo": {"name": "test", "version": "0"}})
        self.assertEqual(answer["result"]["protocolVersion"], "2025-03-26")
        self.assertEqual(answer["result"]["capabilities"], {"tools": {}})
        self.assertEqual(answer["result"]["serverInfo"]["name"], "av-inspect")
        self.assertIsNone(self.request("notifications/initialized", notify=True))
        self.assertEqual(self.request("ping")["result"], {})
        tools = {tool["name"]: tool for tool in self.request("tools/list")["result"]["tools"]}
        self.assertEqual(set(tools), {
            "runs", "processes", "threads", "app_state", "contexts", "handles", "kernel_threads", "kernel_state",
            "kernel_objects", "kernel_trace_from_memory", "crashes", "crash", "icd_logs", "driver_log", "verdict",
            "guest_powershell", "host_threads", "host_state", "scope_names", "scope_name"})
        inspector = av_inspect.Inspector()
        for name, tool in tools.items():
            with self.subTest(tool=name):
                self.assertEqual(tool["inputSchema"]["type"], "object")
                self.assertTrue(tool["description"])
                self.assertTrue(set(tool["inputSchema"]["required"]) <= set(tool["inputSchema"]["properties"]))
                self.assertTrue(callable(getattr(inspector, name)))
        default = self.request("initialize", {})["result"]["protocolVersion"]
        self.assertEqual(default, "2025-06-18")

    def test_protocol_errors_are_json_rpc_errors_and_tool_failures_are_results(self):
        self.server.stdin.write("{not json\n\n")
        self.server.stdin.flush()
        self.assertEqual(json.loads(self.server.stdout.readline())["error"]["code"], -32700)
        self.server.stdin.write(json.dumps({"id": 9, "method": "ping"}) + "\n")
        self.server.stdin.flush()
        self.assertEqual(json.loads(self.server.stdout.readline())["error"]["code"], -32600)
        self.assertEqual(self.request("resources/list")["error"]["code"], -32601)
        self.assertEqual(self.request("tools/call", {"name": "nonexistent"})["error"]["code"], -32602)
        self.assertEqual(self.request("tools/call", {"name": "host_threads", "arguments": {}})["error"]["code"],
                         -32602)
        self.assertEqual(self.request("tools/call", {"name": "verdict", "arguments": {"work": "x", "extra": 1}})
                         ["error"]["code"], -32602)
        self.assertEqual(self.request("tools/call", {"name": "verdict", "arguments": [1]})["error"]["code"], -32602)
        failed, text = self.call("verdict", work="no-such-run")
        self.assertTrue(failed)
        self.assertIn("no work directory", text)
        failed, text = self.call("driver_log", work="build-guest-x", grep="(")
        self.assertTrue(failed)
        self.assertIn("driver_log failed: ", text)

    def test_scope_names_come_from_the_sources_when_the_tool_is_absent(self):
        failed, names = self.call("scope_names")
        self.assertFalse(failed)
        self.assertEqual(names, [{"id": f"{WAIT:#010x}", "name": "GdiKmt::wait"},
                                 {"id": f"{RUN:#010x}", "name": "IcdRoot::destroy_instance"}])
        self.assertEqual(self.call("scope_names", pattern="Kmt")[1], [names[0]])
        self.assertEqual(self.call("scope_name", id=f"{RUN:#x}"), (False, "IcdRoot::destroy_instance"))
        self.assertEqual(self.call("scope_name", id=WAIT), (False, "GdiKmt::wait"))
        self.assertTrue(self.call("scope_name", id=1)[0])
        self.assertTrue(self.call("scope_name", id="zz")[0])

    def test_host_threads_read_a_table_in_shared_memory(self):
        now = time.clock_gettime_ns(time.CLOCK_MONOTONIC) // 100
        table = (header(2, module=b"device", process=os.getpid())
                 + slot(owner=7 | ACTIVE, depth=1, thread=7, holding=0x9,
                        frames=[(RUN, 0, now - 60 * 1_000 * MS)])
                 + slot(owner=8 | ACTIVE, depth=1, thread=8, waiting=0x9, frames=[(WAIT, 0, now)]))
        (self.shm / f"av-trace-device-{os.getpid()}").write_bytes(table)
        failed, described = self.call("host_threads", pid=os.getpid())
        self.assertFalse(failed, described)
        self.assertTrue(described["alive"])
        first, second = described["threads"]
        self.assertEqual(first["frames"][0]["name"], "IcdRoot::destroy_instance")
        self.assertTrue(first["stalled"])
        self.assertGreaterEqual(first["frames"][0]["age_ms"], 60_000)
        self.assertFalse(second["stalled"])
        self.assertEqual(second["lock_held_by"], [7])
        failed, text = self.call("host_threads", pid=os.getpid(), module="executor")
        self.assertTrue(failed)
        self.assertIn("cannot read", text)
        self.assertIn("one of device, executor", self.call("host_threads", pid=1, module="avvk")[1])
        (self.shm / "av-trace-executor-3").write_bytes(header(1, magic=b"XXXXXXXX") + slot())
        failed, text = self.call("host_threads", pid=3, module="executor")
        self.assertTrue(failed)
        self.assertIn("malformed trace table: magic", text)
        failed, runs = self.call("runs")
        self.assertFalse(failed)
        self.assertEqual(runs["guest_runs"], [{"work": "build-guest-x", "qemu_pids": [],
                                               "sockets": {"qmp.sock": False, "agent.sock": False,
                                                           "agent-relay.sock": False}}])
        self.assertIn({"module": "device", "pid": os.getpid(), "alive": True, "state_socket": False},
                      runs["host"])

    def test_host_state_asks_the_device_socket_and_reads_to_the_end(self):
        path = self.shm / "av-state-device-77.sock"
        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        server.bind(str(path))
        server.listen(1)
        asked = []

        def answer():
            connection, _ = server.accept()
            with connection:
                asked.append(connection.recv(64))
                for part in (b"device/0 contexts=2\n", b"device/0/context/1 lost=true"):
                    connection.sendall(part)
                    time.sleep(0.05)
        worker = threading.Thread(target=answer)
        worker.start()
        failed, entries = self.call("host_state", pid=77)
        worker.join()
        server.close()
        self.assertFalse(failed, entries)
        self.assertEqual(asked, [b"state\n"])
        self.assertEqual([entry["path"] for entry in entries], ["device/0", "device/0/context/1"])
        self.assertEqual(entries[1]["fields"], {"lost": "true"})
        failed, text = self.call("host_state", pid=78)
        self.assertTrue(failed)
        self.assertIn("av-state-device-78.sock", text)

    def serve_once(self, name, answer_bytes):
        path = self.shm / name
        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        server.bind(str(path))
        server.listen(1)

        def answer():
            connection, _ = server.accept()
            with connection:
                connection.recv(64)
                connection.sendall(answer_bytes)
        worker = threading.Thread(target=answer)
        worker.start()
        self.addCleanup(server.close)
        return worker

    def test_host_state_asks_the_executor_s_socket_and_reports_a_refusal_as_one(self):
        worker = self.serve_once("av-state-executor-77.sock", b"executor/0 live=1\nexecutor/0/session/3 api=vulkan\n")
        failed, entries = self.call("host_state", pid=77, module="executor")
        worker.join()
        self.assertFalse(failed, entries)
        self.assertEqual([entry["path"] for entry in entries], ["executor/0", "executor/0/session/3"])
        worker = self.serve_once("av-state-device-79.sock", b"error the main loop did not answer within 2 s\n")
        failed, text = self.call("host_state", pid=79)
        worker.join()
        self.assertTrue(failed)
        self.assertIn("the main loop did not answer within 2 s", text)
        failed, text = self.call("host_state", pid=77, module="kernel")
        self.assertTrue(failed)

    def test_the_run_s_logs_and_verdict_are_read_from_its_directory(self):
        self.assertEqual(self.call("driver_log", work="build-guest-x", lines=2),
                         (False, "avgpu: vsync 1\navgpu: a"))
        self.assertEqual(self.call("driver_log", work="build-guest-x", grep="Entry"), (False, "avgpu: DriverEntry"))
        self.assertTrue(self.call("verdict", work="build-guest-x")[0])
        (self.root / "build-guest-x" / "verdict.txt").write_text("boot ok\n")
        self.assertEqual(self.call("verdict", work="build-guest-x"), (False, {"verdict.txt": "boot ok\n"}))
        (self.root / "build-guest-x" / "panic.txt").write_text("bugcheck\n")
        self.assertEqual(set(self.call("verdict", work=str(self.root / "build-guest-x"))[1]),
                         {"verdict.txt", "panic.txt"})
        (self.root / "build-guest-x" / "avgpu.log").unlink()
        self.assertTrue(self.call("driver_log", work="build-guest-x")[0])
        failed, text = self.call("threads", work="build-guest-x", pid=1, module="avvk")
        self.assertTrue(failed)
        self.assertIn("no agent socket", text)
        failed, text = self.call("kernel_trace_from_memory", work="build-guest-x")
        self.assertTrue(failed)
        self.assertIn("names no trace table", text)


OS_THREADS = "40|Wait|UserRequest|15\r\n41|Running||3\r\n99|Wait|Suspended|0\r\n"
TABLE_ADDRESS = 0x1F000000


class GuestTools(unittest.TestCase):
    def setUp(self):
        self.scratch = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)
        self.work = self.scratch / "work"
        self.work.mkdir()
        (self.scratch / "disk.qcow2").write_bytes(b"disk")
        self.table = (header(2)
                      + slot(owner=40 | ACTIVE, depth=1, thread=40, waiting=0x77, frames=[(RUN, 0, 10 * MS)])
                      + slot(owner=41 | ACTIVE, depth=1, thread=41, holding=0x77, frames=[(WAIT, 0, 90 * MS)]))
        self.kernel = header(1, module=b"avgpu", process=0) + slot(owner=0xFFFF8000 | ACTIVE, depth=1, thread=4,
                                                                    frames=[(SEND, 2, 50 * MS)])
        self.script = {
            "driver_log": ["avgpu: DriverEntry",
                           f"avgpu: trace table at physical 0x{TABLE_ADDRESS:x} bytes {len(self.kernel)}"],
            "trace_memory": {"address": TABLE_ADDRESS, "b64": base64.b64encode(self.kernel).decode()},
            "inspect": {
                "list": "table pid=4242 module=avvk scope=global exe=C:\\av\\vulkan info.exe\r\n"
                        "table pid=5000 module=avgl scope=local exe=C:\\x.exe\r\nnoise\r\n",
                "trace": base64.b64encode(snapshot(1, 100 * MS, self.table)).decode(),
                "state": "connection/0 luid=0x1 adapter=2\nconnection/0/handle/1 kind=instance\n"
                         "connection/0/handle/2 kind=device\nconnection/1/handle/3 kind=instance\nchunk/0 bytes=4\n",
                "kernel-trace": base64.b64encode(snapshot(1, 60 * MS, self.kernel)).decode(),
                "kernel-state": "adapter/0 devices=1\nadapter/0/device/1 process=4242\n"
                                "adapter/0/av_context/0 doorbell=0 lost=false\nadapter/0/allocation/7 bytes=4096\n"},
            "powershell": {"Get-Process -Id 4242).Threads": OS_THREADS,
                           "Get-Process -Id 4242,5000": "4242|vulkaninfo|3|2026-09-16T20:00:00\r\n",
                           "'crash-*.txt'": "crash-avvk-4242.txt|120|2026-09-16T21:00:00\r\n",
                           "'*.log'": "avvk-4242.log|30\r\nbad|x\r\n",
                           "Write-Output hello": "hello\r\n"},
            "files": {"C:\\ProgramData\\av\\crash-avvk-4242.txt": "exception 0xC0000005\r\n",
                      "C:\\ProgramData\\av\\avvk-4242.log": "avvk: negotiated\r\n"},
        }
        self.start()

    def start(self):
        (self.scratch / "script.json").write_text(json.dumps(self.script))
        self.journal = self.scratch / "journal.txt"
        environment = dict(os.environ, FAKE_GUEST_SCRIPT=str(self.scratch / "script.json"),
                           FAKE_GUEST_JOURNAL=str(self.journal))
        argv = ["-chardev", f"socket,id=agent,path={self.work / 'agent.sock'},server=on,wait=off",
                "-chardev", f"file,id=debugcon,path={self.work / 'avgpu.log'}",
                "-qmp", f"unix:{self.work / 'qmp.sock'},server=on,wait=off",
                "-drive", f"if=none,id=disk,file={self.scratch / 'disk.qcow2'}",
                "-device", "virtio-gpu-av-pci,id=gpu"]
        self.qemu = subprocess.Popen([sys.executable, "-B", str(FAKE_QEMU), *argv], env=environment)
        self.addCleanup(self.qemu.wait)
        self.addCleanup(self.qemu.kill)
        deadline = time.monotonic() + 10
        while not ((self.work / "agent.sock").exists() and (self.work / "qmp.sock").exists()):
            self.assertLess(time.monotonic(), deadline, "the fake QEMU never listened")
            time.sleep(0.05)
        self.inspector = av_inspect.Inspector()
        self.inspector.scopes.names = NAMES

    def executed(self):
        return [line for line in self.journal.read_text().splitlines() if line.startswith("exec")]

    def test_processes_lists_the_tables_with_what_the_guest_says_of_each(self):
        answer = self.inspector.processes(str(self.work))
        self.assertEqual(answer["via"], "agent")
        self.assertEqual(answer["tables"], [
            {"pid": 4242, "module": "avvk", "scope": "global", "exe": "C:\\av\\vulkan info.exe",
             "name": "vulkaninfo", "threads": 3, "started": "2026-09-16T20:00:00"},
            {"pid": 5000, "module": "avgl", "scope": "local", "exe": "C:\\x.exe", "exited": True}])
        self.assertIn("exec C:\\av\\avinspect.exe list --out C:\\av\\inspect-list.bin", self.executed())
        self.assertTrue(any(line.startswith("exec C:\\Windows\\System32\\cmd.exe /c C:\\Windows\\System32\\"
                                            "WindowsPowerShell\\v1.0\\powershell.exe -NoProfile") and "> C:\\av\\"
                            in line for line in self.executed()))

    def test_threads_joins_the_table_with_the_os_threads(self):
        answer = self.inspector.threads(str(self.work), 4242, "avvk", stall_seconds=0.05)
        self.assertIn("exec C:\\av\\avinspect.exe trace 4242 avvk --out C:\\av\\inspect-trace.bin",
                      self.executed())
        waiting, holding = answer["threads"]
        self.assertEqual((waiting["frames"][0]["name"], waiting["frames"][0]["age_ms"], waiting["stalled"]),
                         ("IcdRoot::destroy_instance", 90, True))
        self.assertEqual((waiting["lock_held_by"], waiting["os_state"], waiting["wait_reason"], waiting["cpu_ms"]),
                         ([41], "Wait", "UserRequest", 15.0))
        self.assertEqual((holding["os_state"], holding["wait_reason"], holding["stalled"]), ("Running", None, False))
        self.assertEqual(answer["outside_av"], [{"thread": 99, "os_state": "Wait", "wait_reason": "Suspended",
                                                  "cpu_ms": 0.0}])

    def test_state_contexts_and_handles_come_from_the_process_dump(self):
        state = self.inspector.app_state(str(self.work), 4242, "avvk")
        self.assertEqual(len(state), 5)
        self.assertEqual([entry["path"] for entry in self.inspector.contexts(str(self.work), 4242)],
                         ["connection/0", "connection/0/handle/1", "connection/0/handle/2",
                          "connection/1/handle/3"])
        handles = self.inspector.handles(str(self.work), 4242)
        self.assertEqual({owner: [entry["fields"]["kind"] for entry in entries] for owner, entries in handles.items()},
                         {"connection/0": ["instance", "device"], "connection/1": ["instance"]})

    def test_the_kernel_s_threads_and_objects_come_from_its_escape(self):
        threads = self.inspector.kernel_threads(str(self.work))
        self.assertEqual(threads["module"], "avgpu")
        self.assertEqual((threads["threads"][0]["frames"][0]["name"], threads["threads"][0]["frames"][0]["level"],
                          threads["threads"][0]["frames"][0]["age_ms"]), ("KmtTransport::send_sync", 2, 10))
        self.assertEqual([entry["path"] for entry in self.inspector.kernel_objects(str(self.work), "allocation")],
                         ["adapter/0/allocation/7"])
        self.assertEqual([entry["fields"] for entry in self.inspector.contexts(str(self.work))],
                         [{"doorbell": "0", "lost": "false"}])
        self.assertEqual(len(self.inspector.kernel_state(str(self.work))), 4)

    def test_crash_files_and_logs_are_listed_and_pulled_by_name_only(self):
        self.assertEqual(self.inspector.crashes(str(self.work)),
                         [{"name": "crash-avvk-4242.txt", "bytes": 120, "written": "2026-09-16T21:00:00"}])
        self.assertEqual(self.inspector.crash(str(self.work), "crash-avvk-4242.txt"), "exception 0xC0000005\r\n")
        self.assertEqual(self.inspector.icd_logs(str(self.work)), [{"name": "avvk-4242.log", "bytes": 30}])
        self.assertEqual(self.inspector.icd_logs(str(self.work), "avvk-4242.log"), "avvk: negotiated\r\n")
        for name in ("..\\secret.txt", "crash-a.txt\\..\\x", "avvk.dll"):
            with self.subTest(name=name), self.assertRaises(av_inspect.ToolError):
                self.inspector.crash(str(self.work), name)
        with self.assertRaisesRegex(av_inspect.ToolError, "cannot read"):
            self.inspector.crash(str(self.work), "crash-none.txt")

    def test_a_script_runs_with_its_output_in_a_pulled_file(self):
        self.assertEqual(self.inspector.guest_powershell(str(self.work), "Write-Output hello"),
                         {"exit_code": 0, "output": "hello\r\n"})
        pushed = [line for line in self.journal.read_text().splitlines()
                  if line.startswith("file C:\\av\\inspect-ps-")]
        self.assertEqual(len(pushed), 1)

    def test_an_answer_the_inspector_could_not_give_is_a_tool_error(self):
        self.script["inspect"].pop("state")
        self.script["inspect"]["kernel-trace"] = base64.b64encode(snapshot(3, 0, b"escape refused: 0xC0000001")
                                                                  ).decode()
        self.script["inspect"]["trace"] = base64.b64encode(snapshot(2, 0, b"a/0")).decode()
        self.qemu.kill()
        self.qemu.wait()
        for name in ("agent.sock", "qmp.sock"):
            (self.work / name).unlink()
        self.start()
        with self.assertRaisesRegex(av_inspect.ToolError, "avinspect state exited 1"):
            self.inspector.app_state(str(self.work), 4242, "avvk")
        with self.assertRaisesRegex(av_inspect.ToolError, "^escape refused: 0xC0000001$"):
            self.inspector.kernel_threads(str(self.work))
        with self.assertRaisesRegex(av_inspect.ToolError, "a state snapshot, not a table"):
            self.inspector.threads(str(self.work), 4242, "avvk")

    def test_the_kernel_table_is_read_from_guest_memory_over_qmp(self):
        answer = self.inspector.kernel_trace_from_memory(str(self.work))
        self.assertEqual(answer["physical_address"], f"{TABLE_ADDRESS:#x}")
        self.assertEqual(answer["threads"][0]["frames"][0]["name"], "KmtTransport::send_sync")
        self.assertEqual(answer["threads"][0]["frames"][0]["age_ms"], 0)
        self.assertIn(f"pmemsave {TABLE_ADDRESS:#x} {len(self.kernel)}", self.journal.read_text())
        self.assertEqual(sorted(path.name for path in self.work.iterdir()),
                         ["agent.sock", "avgpu.log", "environment.json", "qmp.sock"])
        with self.assertRaisesRegex(av_inspect.ToolError, "run holds it"):
            self.inspector.kernel_trace_from_memory(str(self.work))


if __name__ == "__main__":
    unittest.main()
