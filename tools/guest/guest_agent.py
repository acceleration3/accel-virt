"""Clients for the two sockets tools/guest/run_guest_suite.py drives a guest
through: the QEMU guest agent inside Windows, and QMP on QEMU itself.

Both speak one JSON object per line. Every read has a deadline, and anything
that is not the answer asked for - garbage, a stale reply, an error object, a
closed connection - raises AgentError instead of hanging or guessing.
"""
import base64
import json
import os
import random
import socket
import time

CHUNK = 48 * 1024
# One connection attempt while the guest boots: its agent may not listen yet.
ATTEMPT = 5.0
DELIMITER = b"\xff"


class AgentError(Exception):
    """The guest agent or QMP did not give the answer asked for."""


class Channel:
    """One unix socket carrying JSON lines, read against a deadline."""

    def __init__(self, path, timeout):
        self.timeout = timeout
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.buffer = b""
        try:
            self.socket.settimeout(timeout)
            self.socket.connect(path)
        except OSError as error:
            self.socket.close()
            raise AgentError(f"cannot connect to {path}: {error}") from error

    def close(self):
        self.socket.close()

    def send(self, message, prefix=b""):
        try:
            self.socket.sendall(prefix + json.dumps(message).encode() + b"\n")
        except (BrokenPipeError, ConnectionResetError) as error:
            raise AgentError("connection closed") from error
        except OSError as error:
            raise AgentError(f"send failed: {error}") from error

    def fill(self, deadline):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise AgentError("timed out waiting for an answer")
        self.socket.settimeout(remaining)
        try:
            data = self.socket.recv(65536)
        except socket.timeout as error:
            raise AgentError("timed out waiting for an answer") from error
        except ConnectionResetError as error:
            raise AgentError("connection closed") from error
        except OSError as error:
            raise AgentError(f"receive failed: {error}") from error
        if not data:
            raise AgentError("connection closed")
        self.buffer += data

    def discard_through_delimiter(self, deadline):
        while DELIMITER not in self.buffer:
            self.buffer = b""
            self.fill(deadline)
        self.buffer = self.buffer[self.buffer.index(DELIMITER) + 1:]

    def line(self, deadline):
        while b"\n" not in self.buffer:
            self.fill(deadline)
        line, self.buffer = self.buffer.split(b"\n", 1)
        return line.strip()

    def message(self, deadline):
        """The next JSON object, skipping lines that are not one."""
        while True:
            line = self.line(deadline)
            try:
                message = json.loads(line)
            except ValueError:
                continue
            if isinstance(message, dict):
                return message


def answer_of(message):
    if "error" in message:
        error = message["error"]
        described = error.get("desc", error) if isinstance(error, dict) else error
        raise AgentError(f"guest agent error: {described}")
    if "return" not in message:
        raise AgentError(f"not an answer: {message}")
    return message["return"]


class GuestAgent:
    """The QEMU guest agent: commands, processes and files in the guest."""

    def __init__(self, path, timeout):
        self.channel = Channel(path, timeout)
        self.timeout = timeout

    def close(self):
        self.channel.close()

    def sync(self):
        """Resynchronizes the stream: whatever an earlier session left unread is dropped."""
        token = random.randrange(1, 2**31)
        deadline = time.monotonic() + self.timeout
        self.channel.send({"execute": "guest-sync-delimited", "arguments": {"id": token}}, prefix=DELIMITER)
        while True:
            self.channel.discard_through_delimiter(deadline)
            message = self.channel.message(deadline)
            if message.get("return") == token:
                return

    def execute(self, command, arguments=None):
        request = {"execute": command}
        if arguments is not None:
            request["arguments"] = arguments
        self.channel.send(request)
        return answer_of(self.channel.message(time.monotonic() + self.timeout))

    def notify(self, command, arguments):
        """A command the agent does not answer, such as a shutdown."""
        self.channel.send({"execute": command, "arguments": arguments})

    def run(self, path, arguments, timeout, poll=0.25):
        """Runs a program in the guest: its exit code, standard output and error."""
        started = self.execute("guest-exec", {"path": path, "arg": list(arguments), "capture-output": True})
        if not isinstance(started, dict) or "pid" not in started:
            raise AgentError(f"guest-exec answered {started}")
        deadline = time.monotonic() + timeout
        while True:
            status = self.execute("guest-exec-status", {"pid": started["pid"]})
            if not isinstance(status, dict):
                raise AgentError(f"guest-exec-status answered {status}")
            if status.get("exited"):
                return (status.get("exitcode", -1), decode(status.get("out-data", "")),
                        decode(status.get("err-data", "")))
            if time.monotonic() >= deadline:
                raise AgentError(f"{path} still running after {timeout} s")
            time.sleep(poll)

    def push(self, data, guest_path):
        handle = self.execute("guest-file-open", {"path": guest_path, "mode": "wb"})
        try:
            for offset in range(0, len(data), CHUNK):
                chunk = base64.b64encode(data[offset:offset + CHUNK]).decode()
                written = self.execute("guest-file-write", {"handle": handle, "buf-b64": chunk})
                if not isinstance(written, dict) or written.get("count") != min(CHUNK, len(data) - offset):
                    raise AgentError(f"short write to {guest_path}: {written}")
        finally:
            self.execute("guest-file-close", {"handle": handle})


def decode(text):
    try:
        return base64.b64decode(text, validate=True)
    except (ValueError, TypeError) as error:
        raise AgentError(f"undecodable process output: {error}") from error


class Qmp:
    """QEMU's machine protocol: events and a quit."""

    def __init__(self, path, timeout):
        self.channel = Channel(path, timeout)
        self.timeout = timeout
        deadline = time.monotonic() + timeout
        greeting = self.channel.message(deadline)
        if "QMP" not in greeting:
            raise AgentError(f"not a QMP greeting: {greeting}")
        self.pending = []
        self.execute("qmp_capabilities")

    def close(self):
        self.channel.close()

    def execute(self, command):
        self.channel.send({"execute": command})
        deadline = time.monotonic() + self.timeout
        while True:
            message = self.channel.message(deadline)
            if "event" in message:
                self.pending.append(message["event"])
                continue
            return answer_of(message)

    def wait_event(self, name, timeout):
        """Waits for event `name`, counting ones already seen."""
        if name in self.pending:
            self.pending.remove(name)
            return
        deadline = time.monotonic() + timeout
        while True:
            message = self.channel.message(deadline)
            if message.get("event") == name:
                return
            if "event" in message:
                self.pending.append(message["event"])

    def forget_events(self):
        self.pending = []


def wait_for_agent(path, deadline, timeout, pause=1.0):
    """Connects and syncs with the guest agent once it answers, or raises at the deadline."""
    last = None
    while time.monotonic() < deadline:
        if os.path.exists(path):
            agent = None
            try:
                agent = GuestAgent(path, min(timeout, ATTEMPT, max(0.1, deadline - time.monotonic())))
                agent.sync()
                agent.execute("guest-ping")
                agent.timeout = timeout
                return agent
            except AgentError as error:
                last = error
                if agent is not None:
                    agent.close()
        time.sleep(pause)
    raise AgentError(f"the guest agent never answered: {last}")
