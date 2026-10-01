#!/usr/bin/env python3
"""An MCP server (stdio, JSON-RPC 2.0, one message per line) over the thread
and state tracking of docs/trace.md: which guest and host threads are inside
av code and where, what each process, the driver and the device hold, and
what a crash left behind.

The guest is reached through a run's agent relay (<work>/agent-relay.sock) or,
when no run holds the agent, its socket; everything large comes back as a
guest file that is pulled, because output the agent captured stalled
vulkaninfo on a live guest. Host tables are read from /dev/shm directly.

  claude mcp add av-inspect -- python3 tools/mcp/av_inspect.py
"""
import json
import os
import pathlib
import re
import socket
import subprocess
import sys
import tempfile
import time

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "tools" / "guest"))
import trace_format  # noqa: E402
from guest_agent import AgentError, AgentTimeout, GuestAgent, Qmp  # noqa: E402

PROTOCOL = "2025-06-18"
SERVER = {"name": "av-inspect", "version": "1"}
GUEST_DIRECTORY = "C:\\av"
INSPECTOR = GUEST_DIRECTORY + "\\avinspect.exe"
CMD = "C:\\Windows\\System32\\cmd.exe"
POWERSHELL = "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"
DATA_DIRECTORY = "C:\\ProgramData\\av"
AGENT_TIMEOUT = 10.0
QMP_TIMEOUT = 3.0
INSPECT_TIMEOUT = 30.0
TABLE_LINE = re.compile(r"^avgpu: trace table at physical 0x([0-9a-fA-F]+) bytes (\d+)\s*$", re.M)
SCOPE = re.compile(r'AV_SCOPE\("((?:[^"\\]|\\.)*)"\)')
SOURCE_SUFFIXES = (".cpp", ".hpp", ".h", ".c")
HOST_MODULES = ("device", "executor")


class ToolError(Exception):
    """A tool could not answer; the text says why, and the call is isError."""


def shm_directory():
    return pathlib.Path(os.environ.get("AV_INSPECT_SHM", "/dev/shm"))


def repository():
    return pathlib.Path(os.environ.get("AV_INSPECT_ROOT", str(ROOT)))


def work_directory(work):
    path = pathlib.Path(work)
    if not path.is_absolute():
        path = repository() / path
    if not path.is_dir():
        raise ToolError(f"no work directory {path}")
    return path


def host_clock():
    return time.clock_gettime_ns(time.CLOCK_MONOTONIC) // 100


class ScopeNames:
    """Name ids back to names: tools/scope_marks.py --names when it answers,
    else every AV_SCOPE literal under src/ hashed here. Either way a pair is
    kept only when the name hashes to the id, so no output format is trusted."""

    def __init__(self):
        self.names = None

    def load(self):
        if self.names is None:
            self.names = self.from_tool() or self.from_sources()
        return self.names

    def from_tool(self):
        tool = repository() / "tools" / "scope_marks.py"
        if not tool.is_file():
            return {}
        try:
            listing = subprocess.run([sys.executable, "-B", str(tool), "--names"], capture_output=True, text=True,
                                     timeout=120, cwd=repository())
        except (OSError, subprocess.TimeoutExpired):
            return {}
        if listing.returncode != 0:
            return {}
        names = {}
        for line in listing.stdout.splitlines():
            tokens = line.replace("\t", " ").split()
            numbers = set()
            for token in tokens:
                for base in (16, 10):
                    try:
                        numbers.add(int(token, base))
                    except ValueError:
                        pass
            for token in tokens:
                token = token.strip('"')
                if trace_format.fnv1a(token) in numbers:
                    names[trace_format.fnv1a(token)] = token
        return names

    def from_sources(self):
        names = {}
        for base, _dirs, files in os.walk(repository() / "src"):
            for name in files:
                if not name.endswith(SOURCE_SUFFIXES):
                    continue
                text = pathlib.Path(base, name).read_text(errors="replace")
                for found in SCOPE.findall(text):
                    names[trace_format.fnv1a(found)] = found
        return names


class Guest:
    """One guest's agent: the run's relay when it serves, the socket otherwise."""

    def __init__(self, work):
        self.work = work
        relay = work / "agent-relay.sock"
        direct = work / "agent.sock"
        if relay.exists():
            agent = None
            try:
                agent = GuestAgent(str(relay), AGENT_TIMEOUT)
                agent.execute("guest-ping")
                self.agent, self.via = agent, "relay"
                return
            except AgentError:
                if agent is not None:
                    agent.close()
        if not direct.exists():
            raise ToolError(f"{work} has no agent socket: is the guest running?")
        try:
            self.agent = GuestAgent(str(direct), AGENT_TIMEOUT)
            self.agent.sync()
        except AgentTimeout as error:
            raise ToolError(f"the guest agent did not answer ({error}): a run may hold its connection "
                            "without a relay") from error
        except AgentError as error:
            raise ToolError(f"cannot reach the guest agent: {error}") from error
        self.via = "agent"

    def close(self):
        self.agent.close()

    def run(self, path, arguments, timeout=INSPECT_TIMEOUT):
        try:
            return self.agent.run(path, arguments, timeout)
        except AgentError as error:
            raise ToolError(f"{path}: {error}") from error

    def read(self, guest_path):
        """A guest file's bytes."""
        with tempfile.TemporaryDirectory() as scratch:
            local = pathlib.Path(scratch) / "pulled"
            try:
                self.agent.pull(guest_path, local)
            except AgentError as error:
                raise ToolError(f"cannot read {guest_path}: {error}") from error
            return local.read_bytes()

    def write(self, guest_path, data):
        try:
            self.agent.push(data, guest_path)
        except AgentError as error:
            raise ToolError(f"cannot write {guest_path}: {error}") from error

    def inspect(self, command, *arguments):
        """avinspect.exe COMMAND ARGS --out F, and F's bytes."""
        out = f"{GUEST_DIRECTORY}\\inspect-{command}.bin"
        code, _out, error = self.run(INSPECTOR, [command, *map(str, arguments), "--out", out])
        if code != 0:
            raise ToolError(f"avinspect {command} exited {code}: {error.decode(errors='replace').strip()}"
                            " (is avinspect.exe in C:\\av?)")
        return self.read(out)

    def powershell(self, script, timeout=INSPECT_TIMEOUT):
        """Runs a script with its output in a file that is pulled: (exit code, text)."""
        stem = f"{GUEST_DIRECTORY}\\inspect-ps-{os.getpid()}-{time.monotonic_ns()}"
        self.write(stem + ".ps1", script.encode("utf-8-sig"))
        command = f"{POWERSHELL} -NoProfile -ExecutionPolicy Bypass -File {stem}.ps1 > {stem}.txt 2>&1"
        code, _out, _error = self.run(CMD, ["/c", command], timeout)
        return code, self.read(stem + ".txt").decode("utf-8", errors="replace")


def with_guest(work, action):
    guest = Guest(work_directory(work))
    try:
        return action(guest)
    finally:
        guest.close()


def snapshot_of(data, wanted):
    try:
        kind, clock, payload = trace_format.parse_snapshot(data)
    except ValueError as error:
        raise ToolError(f"avinspect wrote no snapshot: {error}") from error
    if kind == "error":
        raise ToolError(payload.decode(errors="replace"))
    if kind != wanted:
        raise ToolError(f"avinspect wrote a {kind} snapshot, not a {wanted}")
    return clock, payload


def table_of(payload):
    try:
        return trace_format.parse_table(payload)
    except ValueError as error:
        raise ToolError(f"malformed trace table: {error}") from error


def thread_listing(text):
    """PowerShell's `Id|State|WaitReason|CpuMs` lines."""
    threads = {}
    for line in text.splitlines():
        parts = line.strip().split("|")
        if len(parts) != 4 or not parts[0].isdigit():
            continue
        threads[int(parts[0])] = {"os_state": parts[1], "wait_reason": parts[2] or None,
                                  "cpu_ms": float(parts[3]) if re.fullmatch(r"[\d.]+", parts[3]) else None}
    return threads


OS_THREADS = """$ErrorActionPreference = 'Stop'
foreach ($t in (Get-Process -Id {pid}).Threads) {{
  $reason = ''
  if ($t.ThreadState -eq 'Wait') {{ try {{ $reason = [string]$t.WaitReason }} catch {{ }} }}
  '{{0}}|{{1}}|{{2}}|{{3}}' -f $t.Id, $t.ThreadState, $reason, [int]$t.TotalProcessorTime.TotalMilliseconds
}}
"""

PROCESS_LISTING = """$ErrorActionPreference = 'SilentlyContinue'
foreach ($p in Get-Process -Id {pids}) {{
  '{{0}}|{{1}}|{{2}}|{{3}}' -f $p.Id, $p.ProcessName, $p.Threads.Count, $p.StartTime.ToString('o')
}}
"""


class Inspector:
    """The tools, each a method answering a JSON-able value or raising ToolError."""

    def __init__(self):
        self.scopes = ScopeNames()

    def tools(self):
        work = {"type": "string", "description": "the run's work directory, e.g. build-guest-vulkan"}
        pid = {"type": "integer", "description": "a guest process id"}
        module = {"type": "string", "description": "the table's module: avvk, avgl or avtest"}
        stall = {"type": "number", "description": "an active thread whose innermost frame is older is stalled"}
        return [
            tool("runs", "Guest work directories with their sockets and QEMU, and host QEMU processes with trace "
                         "tables.", {}),
            tool("processes", "Guest processes that have an av trace table, with name, thread count and start.",
                 {"work": work}, ["work"]),
            tool("threads", "A guest process's threads: av frames with ages, locks waited on and their holders, "
                            "stalls, joined with the OS thread states.",
                 {"work": work, "pid": pid, "module": module, "stall_seconds": stall}, ["work", "pid", "module"]),
            tool("app_state", "A guest process's state dump: connections, contexts, handles and the rest, one "
                              "entry per object.", {"work": work, "pid": pid, "module": module},
                 ["work", "pid", "module"]),
            tool("contexts", "Contexts and connections: a process's, or with no pid the driver's av contexts.",
                 {"work": work, "pid": pid, "module": module}, ["work"]),
            tool("handles", "A process's handles, grouped by connection.", {"work": work, "pid": pid,
                                                                            "module": module},
                 ["work", "pid"]),
            tool("kernel_threads", "Threads inside avgpu.sys, through the driver's escape.",
                 {"work": work, "stall_seconds": stall}, ["work"]),
            tool("kernel_state", "The driver's state dump of every adapter.", {"work": work}, ["work"]),
            tool("kernel_objects", "Driver state entries of one kind: device, allocation, av_context, ...",
                 {"work": work, "kind": {"type": "string"}}, ["work", "kind"]),
            tool("kernel_trace_from_memory", "The driver's table read from guest physical memory over QMP, for a "
                                             "guest that cannot run anything.", {"work": work}, ["work"]),
            tool("crashes", "Crash files the guest's av processes wrote.", {"work": work}, ["work"]),
            tool("crash", "One crash file.", {"work": work, "name": {"type": "string"}}, ["work", "name"]),
            tool("icd_logs", "The ICDs' logs in C:\\ProgramData\\av: the list, or one by name.",
                 {"work": work, "name": {"type": "string"}}, ["work"]),
            tool("driver_log", "The tail of the driver's debug-port log.",
                 {"work": work, "lines": {"type": "integer"}, "grep": {"type": "string"}}, ["work"]),
            tool("verdict", "The run's verdict and panic record.", {"work": work}, ["work"]),
            tool("guest_powershell", "Runs a PowerShell script in the guest; its output is returned.",
                 {"work": work, "script": {"type": "string"}, "timeout": {"type": "number"}},
                 ["work", "script"]),
            tool("host_threads", "Threads inside the device or the executor of a host QEMU process.",
                 {"pid": {"type": "integer"}, "module": {"type": "string", "enum": list(HOST_MODULES)},
                  "stall_seconds": stall}, ["pid"]),
            tool("host_state", "The device's or the executor's state dump from a host QEMU process.",
                 {"pid": {"type": "integer"}, "module": {"type": "string", "enum": list(HOST_MODULES)}}, ["pid"]),
            tool("scope_names", "Every traced function name and its id, optionally filtered by a substring.",
                 {"pattern": {"type": "string"}}),
            tool("scope_name", "The function a name id stands for.",
                 {"id": {"type": ["integer", "string"], "description": "decimal, or hex with 0x"}}, ["id"]),
        ]

    def runs(self):
        found = []
        for work in sorted(repository().glob("build-guest*")):
            if not work.is_dir():
                continue
            sockets = {name: (work / name).exists() for name in ("qmp.sock", "agent.sock", "agent-relay.sock")}
            found.append({"work": work.name, "sockets": sockets, "qemu_pids": qemu_pids(work)})
        return {"guest_runs": found, "host": self.host_tables()}

    def host_tables(self):
        tables = []
        for path in sorted(shm_directory().glob("av-trace-*-*")):
            match = re.fullmatch(r"av-trace-(\w+)-(\d+)", path.name)
            if match is None:
                continue
            pid = int(match.group(2))
            tables.append({"module": match.group(1), "pid": pid, "alive": pathlib.Path(f"/proc/{pid}").exists(),
                           "state_socket": (shm_directory() / f"av-state-device-{pid}.sock").exists()})
        return tables

    def processes(self, work):
        def listed(guest):
            text = guest.inspect("list").decode(errors="replace")
            tables = []
            for line in text.splitlines():
                match = re.match(r"^table pid=(\d+) module=(\S+) scope=(\S+) exe=(.*)$", line.strip())
                if match:
                    tables.append({"pid": int(match.group(1)), "module": match.group(2), "scope": match.group(3),
                                   "exe": match.group(4)})
            pids = sorted({table["pid"] for table in tables})
            if pids:
                _code, output = guest.powershell(PROCESS_LISTING.format(pids=",".join(map(str, pids))))
                details = {}
                for line in output.splitlines():
                    parts = line.strip().split("|")
                    if len(parts) == 4 and parts[0].isdigit():
                        details[int(parts[0])] = {"name": parts[1], "threads": int(parts[2]) if parts[2].isdigit()
                                                  else None, "started": parts[3]}
                for table in tables:
                    table.update(details.get(table["pid"], {"exited": True}))
            return {"via": guest.via, "tables": tables}
        return with_guest(work, listed)

    def threads(self, work, pid, module, stall_seconds=5.0):
        def read(guest):
            clock, payload = snapshot_of(guest.inspect("trace", pid, module), "table")
            described = trace_format.describe_threads(table_of(payload), self.scopes.load(), clock or None,
                                                      stall_seconds)
            code, output = guest.powershell(OS_THREADS.format(pid=int(pid)))
            system = thread_listing(output) if code == 0 else {}
            for thread in described["threads"]:
                thread.update(system.pop(thread["thread"], {}))
            described["outside_av"] = [dict(thread=number, **state) for number, state in sorted(system.items())]
            if code != 0:
                described["os_threads_error"] = output.strip()
            return described
        return with_guest(work, read)

    def app_state(self, work, pid, module):
        def read(guest):
            _clock, payload = snapshot_of(guest.inspect("state", pid, module), "state")
            return trace_format.parse_state(payload.decode(errors="replace"))
        return with_guest(work, read)

    def contexts(self, work, pid=None, module="avvk"):
        if pid is None:
            return [entry for entry in self.kernel_state(work) if "av_context" in entry["kinds"]]
        return [entry for entry in self.app_state(work, pid, module)
                if {"connection", "context"} & set(entry["kinds"])]

    def handles(self, work, pid, module="avvk"):
        grouped = {}
        for entry in self.app_state(work, pid, module):
            if "handle" not in entry["kinds"]:
                continue
            segments = entry["path"].split("/")
            owner = "/".join(segments[:segments.index("handle")]) or "-"
            grouped.setdefault(owner, []).append(entry)
        return grouped

    def kernel_threads(self, work, stall_seconds=5.0):
        def read(guest):
            clock, payload = snapshot_of(guest.inspect("kernel-trace"), "table")
            return trace_format.describe_threads(table_of(payload), self.scopes.load(), clock or None,
                                                 stall_seconds)
        return with_guest(work, read)

    def kernel_state(self, work):
        def read(guest):
            _clock, payload = snapshot_of(guest.inspect("kernel-state"), "state")
            return trace_format.parse_state(payload.decode(errors="replace"))
        return with_guest(work, read)

    def kernel_objects(self, work, kind):
        return [entry for entry in self.kernel_state(work) if entry["kinds"] and entry["kinds"][-1] == kind]

    def kernel_trace_from_memory(self, work):
        path = work_directory(work)
        log = path / "avgpu.log"
        found = TABLE_LINE.findall(log.read_text(errors="replace")) if log.is_file() else []
        if not found:
            raise ToolError(f"{log} names no trace table: the driver logs its address at DriverEntry")
        address, size = int(found[-1][0], 16), int(found[-1][1])
        try:
            qmp = Qmp(str(path / "qmp.sock"), QMP_TIMEOUT)
        except AgentTimeout as error:
            raise ToolError("QMP did not greet: QEMU serves one QMP client, and the run holds it "
                            f"({error})") from error
        except AgentError as error:
            raise ToolError(f"cannot reach QMP: {error}") from error
        try:
            with tempfile.TemporaryDirectory(dir=path) as scratch:
                dump = pathlib.Path(scratch) / "trace-table.bin"
                qmp.execute("pmemsave", {"val": address, "size": size, "filename": str(dump)})
                if not dump.is_file():
                    raise ToolError("pmemsave wrote nothing")
                data = dump.read_bytes()
        except AgentError as error:
            raise ToolError(f"pmemsave: {error}") from error
        finally:
            qmp.close()
        described = trace_format.describe_threads(table_of(data), self.scopes.load())
        described["physical_address"] = f"{address:#x}"
        return described

    def crashes(self, work):
        def listed(guest):
            code, output = guest.powershell(
                f"Get-ChildItem -Path '{DATA_DIRECTORY}' -Filter 'crash-*.txt' -ErrorAction SilentlyContinue | "
                "ForEach-Object { '{0}|{1}|{2}' -f $_.Name, $_.Length, $_.LastWriteTime.ToString('o') }")
            files = []
            for line in output.splitlines():
                parts = line.strip().split("|")
                if len(parts) == 3:
                    files.append({"name": parts[0], "bytes": int(parts[1]) if parts[1].isdigit() else None,
                                  "written": parts[2]})
            return files
        return with_guest(work, listed)

    def crash(self, work, name):
        return self.guest_file(work, name, r"crash-[\w.-]+\.txt")

    def icd_logs(self, work, name=None):
        if name is not None:
            return self.guest_file(work, name, r"[\w.-]+\.log")

        def listed(guest):
            _code, output = guest.powershell(
                f"Get-ChildItem -Path '{DATA_DIRECTORY}' -Filter '*.log' -ErrorAction SilentlyContinue | "
                "ForEach-Object { '{0}|{1}' -f $_.Name, $_.Length }")
            return [{"name": part[0], "bytes": int(part[1])} for part in
                    (line.strip().split("|") for line in output.splitlines()) if len(part) == 2 and part[1].isdigit()]
        return with_guest(work, listed)

    def guest_file(self, work, name, allowed):
        if re.fullmatch(allowed, name) is None:
            raise ToolError(f"{name!r} is not a file this tool reads")
        return with_guest(work, lambda guest: guest.read(f"{DATA_DIRECTORY}\\{name}").decode(errors="replace"))

    def driver_log(self, work, lines=200, grep=None):
        log = work_directory(work) / "avgpu.log"
        if not log.is_file():
            raise ToolError(f"no {log}")
        kept = log.read_text(errors="replace").splitlines()
        if grep:
            pattern = re.compile(grep)
            kept = [line for line in kept if pattern.search(line)]
        return "\n".join(kept[-int(lines):])

    def verdict(self, work):
        path = work_directory(work)
        answer = {}
        for name in ("verdict.txt", "panic.txt"):
            if (path / name).is_file():
                answer[name] = (path / name).read_text(errors="replace")
        if not answer:
            raise ToolError(f"{path} holds no verdict yet")
        return answer

    def guest_powershell(self, work, script, timeout=60):
        def ran(guest):
            code, output = guest.powershell(script, float(timeout))
            return {"exit_code": code, "output": output}
        return with_guest(work, ran)

    def host_threads(self, pid, module="device", stall_seconds=5.0):
        if module not in HOST_MODULES:
            raise ToolError(f"module {module!r}: one of {', '.join(HOST_MODULES)}")
        path = shm_directory() / f"av-trace-{module}-{int(pid)}"
        try:
            data = path.read_bytes()
        except OSError as error:
            raise ToolError(f"cannot read {path}: {error}") from error
        described = trace_format.describe_threads(table_of(data), self.scopes.load(), host_clock(), stall_seconds)
        described["alive"] = pathlib.Path(f"/proc/{int(pid)}").exists()
        return described

    def host_state(self, pid, module="device"):
        if module not in HOST_MODULES:
            raise ToolError(f"module must be one of {', '.join(HOST_MODULES)}, not {module}")
        path = shm_directory() / f"av-state-{module}-{int(pid)}.sock"
        received = bytearray()
        try:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
                connection.settimeout(AGENT_TIMEOUT)
                connection.connect(str(path))
                connection.sendall(b"state\n")
                while True:
                    data = connection.recv(65536)
                    if not data:
                        break
                    received += data
        except OSError as error:
            raise ToolError(f"cannot read the {module}'s state from {path}: {error}") from error
        text = received.decode(errors="replace")
        # A binary that cannot dump says why on one line of its own.
        if text.startswith("error "):
            raise ToolError(text.strip())
        return trace_format.parse_state(text)

    def scope_names(self, pattern=None):
        names = self.scopes.load()
        return [{"id": f"{identity:#010x}", "name": name} for identity, name in sorted(names.items(),
                                                                                       key=lambda item: item[1])
                if pattern is None or pattern in name]

    def scope_name(self, id):
        try:
            identity = int(id, 0) if isinstance(id, str) else int(id)
        except ValueError as error:
            raise ToolError(f"{id!r} is not a number") from error
        name = self.scopes.load().get(identity)
        if name is None:
            raise ToolError(f"no traced function has id {identity:#010x}")
        return name


def tool(name, description, properties, required=()):
    return {"name": name, "description": description,
            "inputSchema": {"type": "object", "properties": properties, "required": list(required),
                            "additionalProperties": False}}


def qemu_pids(work):
    """QEMU processes whose command line names this work directory's sockets."""
    pids = []
    marker = str(work / "qmp.sock").encode()
    for entry in pathlib.Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            if marker in (entry / "cmdline").read_bytes():
                pids.append(int(entry.name))
        except OSError:
            continue
    return pids


class Server:
    """JSON-RPC 2.0 over newline-delimited stdio, as MCP's stdio transport has it."""

    def __init__(self, inspector, reader, writer):
        self.inspector = inspector
        self.reader = reader
        self.writer = writer

    def serve(self):
        for line in self.reader:
            line = line.strip()
            if not line:
                continue
            answer = self.handle(line)
            if answer is not None:
                self.writer.write(json.dumps(answer) + "\n")
                self.writer.flush()

    def handle(self, line):
        try:
            message = json.loads(line)
        except ValueError:
            return error_reply(None, -32700, "parse error")
        if not isinstance(message, dict) or message.get("jsonrpc") != "2.0" or "method" not in message:
            return error_reply(message.get("id") if isinstance(message, dict) else None, -32600,
                               "invalid request")
        method = message["method"]
        params = message.get("params") or {}
        if "id" not in message:
            return None
        identity = message["id"]
        if method == "initialize":
            requested = params.get("protocolVersion") if isinstance(params, dict) else None
            return reply(identity, {"protocolVersion": requested or PROTOCOL, "capabilities": {"tools": {}},
                                    "serverInfo": SERVER})
        if method == "ping":
            return reply(identity, {})
        if method == "tools/list":
            return reply(identity, {"tools": self.inspector.tools()})
        if method == "tools/call":
            return self.call(identity, params)
        return error_reply(identity, -32601, f"method not found: {method}")

    def call(self, identity, params):
        name = params.get("name") if isinstance(params, dict) else None
        arguments = (params.get("arguments") or {}) if isinstance(params, dict) else {}
        known = {entry["name"]: entry for entry in self.inspector.tools()}
        if name not in known:
            return error_reply(identity, -32602, f"unknown tool: {name}")
        if not isinstance(arguments, dict):
            return error_reply(identity, -32602, f"{name}: arguments are an object")
        schema = known[name]["inputSchema"]
        missing = [field for field in schema["required"] if field not in arguments]
        unexpected = [field for field in arguments if field not in schema["properties"]]
        if missing or unexpected:
            return error_reply(identity, -32602, f"{name}: missing {missing}, unexpected {unexpected}")
        try:
            value = getattr(self.inspector, name)(**arguments)
        except ToolError as error:
            return reply(identity, content(str(error), True))
        except Exception as error:  # a tool's bug is the caller's answer, not the server's end
            return reply(identity, content(f"{name} failed: {type(error).__name__}: {error}", True))
        text = value if isinstance(value, str) else json.dumps(value, indent=1)
        return reply(identity, content(text, False))


def content(text, failed):
    return {"content": [{"type": "text", "text": text}], "isError": failed}


def reply(identity, result):
    return {"jsonrpc": "2.0", "id": identity, "result": result}


def error_reply(identity, code, text):
    return {"jsonrpc": "2.0", "id": identity, "error": {"code": code, "message": text}}


def main():
    Server(Inspector(), sys.stdin, sys.stdout).serve()


if __name__ == "__main__":
    main()
