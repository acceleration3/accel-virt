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
import threading
import time

CHUNK = 48 * 1024
# A crash dump is gigabytes; the agent caps one read at 48 MiB.
PULL_CHUNK = 8 * 1024 * 1024
RECEIVE = 1024 * 1024
# One connection attempt while the guest boots: its agent may not listen yet.
ATTEMPT = 5.0
DELIMITER = b"\xff"


class AgentError(Exception):
    """The guest agent or QMP did not give the answer asked for."""


class AgentTimeout(AgentError):
    """Nothing arrived before the deadline."""


class Channel:
    """One unix socket carrying JSON lines, read against a deadline."""

    def __init__(self, path, timeout):
        self.timeout = timeout
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.buffer = bytearray()
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
            raise AgentTimeout("timed out waiting for an answer")
        self.socket.settimeout(remaining)
        try:
            data = self.socket.recv(RECEIVE)
        except socket.timeout as error:
            raise AgentTimeout("timed out waiting for an answer") from error
        except ConnectionResetError as error:
            raise AgentError("connection closed") from error
        except OSError as error:
            raise AgentError(f"receive failed: {error}") from error
        if not data:
            raise AgentError("connection closed")
        self.buffer += data

    def discard_through_delimiter(self, deadline):
        while DELIMITER not in self.buffer:
            self.buffer = bytearray()
            self.fill(deadline)
        del self.buffer[:self.buffer.index(DELIMITER) + 1]

    def line(self, deadline):
        searched = 0
        end = self.buffer.find(b"\n")
        while end < 0:
            searched = len(self.buffer)
            self.fill(deadline)
            end = self.buffer.find(b"\n", searched)
        line = bytes(self.buffer[:end])
        del self.buffer[:end + 1]
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
        # One request and its answer at a time: the runner and the relay
        # (AgentRelay) share this connection, and the agent answers in order.
        self.lock = threading.RLock()

    def close(self):
        with self.lock:
            self.channel.close()

    def sync(self):
        """Resynchronizes the stream: whatever an earlier session left unread is dropped."""
        token = random.randrange(1, 2**31)
        with self.lock:
            deadline = time.monotonic() + self.timeout
            self.channel.send({"execute": "guest-sync-delimited", "arguments": {"id": token}}, prefix=DELIMITER)
            while True:
                self.channel.discard_through_delimiter(deadline)
                message = self.channel.message(deadline)
                if message.get("return") == token:
                    return

    def exchange(self, request):
        """One request and the agent's whole answer, an error object included."""
        with self.lock:
            self.channel.send(request)
            return self.channel.message(time.monotonic() + self.timeout)

    def execute(self, command, arguments=None):
        request = {"execute": command}
        if arguments is not None:
            request["arguments"] = arguments
        return answer_of(self.exchange(request))

    def notify(self, command, arguments):
        """A command the agent does not answer, such as a shutdown."""
        with self.lock:
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

    def pull(self, guest_path, local_path, chunk=PULL_CHUNK):
        """Copies a guest file to `local_path`: the number of bytes copied."""
        handle = self.execute("guest-file-open", {"path": guest_path, "mode": "rb"})
        copied = 0
        try:
            with open(local_path, "wb") as output:
                while True:
                    read = self.execute("guest-file-read", {"handle": handle, "count": chunk})
                    if not isinstance(read, dict) or "buf-b64" not in read:
                        raise AgentError(f"guest-file-read of {guest_path} answered {read}")
                    data = decode(read["buf-b64"])
                    output.write(data)
                    copied += len(data)
                    if read.get("eof") or not data:
                        return copied
        finally:
            self.execute("guest-file-close", {"handle": handle})


# Commands the agent never answers, or answers outside the line protocol: a
# relayed one would leave the shared connection waiting or out of step.
UNRELAYED = ("guest-sync", "guest-sync-delimited", "guest-shutdown", "guest-suspend-disk", "guest-suspend-ram",
             "guest-suspend-hybrid")


class AgentRelay:
    """Lets another program - tools/mcp/rvk_inspect.py - reach the guest while
    a run holds the agent's only connection: a unix socket taking one JSON
    request per line and answering each with one line, forwarded over whatever
    connection the run holds now (it changes at every reboot)."""

    def __init__(self, path, current):
        self.path = str(path)
        self.current = current
        self.server = None
        self.thread = None

    def start(self):
        try:
            os.unlink(self.path)
        except FileNotFoundError:
            pass
        self.server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.server.bind(self.path)
        self.server.listen(4)
        self.thread = threading.Thread(target=self.serve, args=(self.server,), daemon=True)
        self.thread.start()

    def stop(self):
        if self.server is None:
            return
        server, self.server = self.server, None
        try:
            server.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        server.close()
        try:
            os.unlink(self.path)
        except FileNotFoundError:
            pass

    def serve(self, server):
        while True:
            try:
                connection, _ = server.accept()
            except OSError:
                return
            threading.Thread(target=self.client, args=(connection,), daemon=True).start()

    def client(self, connection):
        buffer = b""
        with connection:
            while True:
                try:
                    data = connection.recv(RECEIVE)
                except OSError:
                    return
                if not data:
                    return
                buffer += data
                while b"\n" in buffer:
                    line, buffer = buffer.split(b"\n", 1)
                    if not line.strip():
                        continue
                    try:
                        connection.sendall(json.dumps(self.answer(line)).encode() + b"\n")
                    except OSError:
                        return

    def answer(self, line):
        try:
            request = json.loads(line)
        except ValueError:
            return failure("JSONParsing", "not a JSON request")
        if not isinstance(request, dict) or not isinstance(request.get("execute"), str):
            return failure("InvalidParameter", "a request is an object naming a command in \"execute\"")
        if request["execute"] in UNRELAYED:
            return failure("Refused", f"{request['execute']} is not relayed: the run's connection would wait "
                                      "for an answer that never comes")
        forwarded = {"execute": request["execute"]}
        if "arguments" in request:
            forwarded["arguments"] = request["arguments"]
        agent = self.current()
        if agent is None:
            return failure("Unavailable", "the run holds no agent connection now (booting or rebooting)")
        try:
            message = agent.exchange(forwarded)
        except AgentTimeout:
            # The late answer would be read as the next request's: drop it.
            try:
                agent.sync()
            except AgentError:
                pass
            return failure("Timeout", "the guest agent did not answer in time")
        except AgentError as error:
            return failure("Unavailable", str(error))
        if "error" in message or "return" in message:
            return {key: message[key] for key in ("return", "error") if key in message}
        return failure("Protocol", f"not an answer: {message}")


def failure(kind, description):
    return {"error": {"class": kind, "desc": description}}


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
        self.pending = []
        try:
            greeting = self.channel.message(time.monotonic() + timeout)
            if "QMP" not in greeting:
                raise AgentError(f"not a QMP greeting: {greeting}")
            self.execute("qmp_capabilities")
        except AgentError:
            self.channel.close()
            raise

    def close(self):
        self.channel.close()

    def execute(self, command, arguments=None):
        message = {"execute": command}
        if arguments is not None:
            message["arguments"] = arguments
        self.channel.send(message)
        deadline = time.monotonic() + self.timeout
        while True:
            message = self.channel.message(deadline)
            if "event" in message:
                self.pending.append(message)
                continue
            return answer_of(message)

    def take_events(self, name):
        """Every event `name` seen so far, which are then no longer pending."""
        taken = [message for message in self.pending if message["event"] == name]
        self.pending = [message for message in self.pending if message["event"] != name]
        return taken

    def seen(self, name):
        return any(message["event"] == name for message in self.pending)

    def wait_event(self, name, timeout):
        """Waits for event `name`, counting ones already seen: the event."""
        for index, message in enumerate(self.pending):
            if message["event"] == name:
                return self.pending.pop(index)
        deadline = time.monotonic() + timeout
        while True:
            message = self.channel.message(deadline)
            if message.get("event") == name:
                return message
            if "event" in message:
                self.pending.append(message)

    def drain(self, seconds):
        """Reads the events QEMU sends for `seconds`: a guest in a bugcheck loop never goes quiet."""
        deadline = time.monotonic() + seconds
        while True:
            try:
                message = self.channel.message(deadline)
            except AgentTimeout:
                return
            if "event" in message:
                self.pending.append(message)

    def forget_events(self, name):
        self.take_events(name)


def wait_for_agent(path, deadline, timeout, pause=1.0, abandoned=None):
    """Connects and syncs with the guest agent once it answers, or raises at the
    deadline, or as soon as `abandoned` names a reason it never will."""
    last = None
    while time.monotonic() < deadline:
        reason = abandoned() if abandoned is not None else None
        if reason is not None:
            raise AgentError(reason)
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
