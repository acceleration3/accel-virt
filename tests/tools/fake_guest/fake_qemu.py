#!/usr/bin/env python3
"""A qemu-system-x86_64 with a scripted Windows guest behind it, for the runner's
tests: it parses the command line tools/guest/run_guest_suite.py builds,
serves QMP and a guest agent on the sockets it names, writes the debug-port
log, and answers guest-exec and guest-file-read from a script.

The script is JSON at $FAKE_GUEST_SCRIPT:
  stages     {"sign": [3010, 0], ...}: exit codes of successive calls of a stage
  outputs    {"check": "...", "avtest": "..."}: standard output by stage, or by
             program for the ones in PROGRAMS
  files      {"C:\\guest\\path": "content"}: what guest-file-read returns, byte
             per character (latin-1), so a dump's binary header can be scripted
  avtest, avvkprobe, avglprobe, avclprobe, avd3d9probe, avd3d11probe, avd3d12probe,
             vulkaninfo, reg, deqp-vk
             exit code of that program; one run as `cmd.exe /c PROGRAM > FILE`
             leaves its output in FILE
  panic      {"during": stage, "code": n, "loop": bool}: the guest bugchecks when
             the stage runs, then reboots; with loop, it bugchecks on every boot
             until QEMU is started again without virtio-gpu-av-pci
  die_during stage: QEMU aborts with an assertion when the stage runs
  refuse     ["guest-suspend-ram", ...]: agent commands answered with an error
  window_status  "0xc0000017" or null: what the driver logs for the host
             window a device with hostmem= has; null takes the line away, as a
             capset without the host-visible bit does
  inspect    {"list": text, "trace": base64, "state": text, "kernel-trace":
             base64, "kernel-state": text}: what avinspect.exe writes at its
             --out path; trace answers are whole snapshot files, state answers
             are wrapped as snapshot kind 2 (docs/trace.md); a command not
             scripted exits 1 and writes nothing
  powershell {"substring": "output"}: `powershell -File F` prints the output of
             the first key found in the script pushed to F
  console_user   who is logged into the console session, which the runner asks
             for before it makes a scheduled task (feature 046); "" is nobody
  console_running  how many `schtasks /query` answers say Running before the
             task is Ready
  trace_memory {"address": n, "b64": ...}: what pmemsave reads from guest
             physical memory at that address
  answer_delay seconds the agent takes over every answer, so two clients of
             one connection are waiting at once
  exit_at_boot, agent_silent, garbage, no_reset, hang, pmemsave_nothing:
             misbehaviours
Everything the guest was asked is appended to $FAKE_GUEST_JOURNAL.
"""
import base64
import json
import os
import signal
import socket
import struct
import sys
import threading
import time

REBOOT = 0.3
ASSERTION = "qemu-system-x86_64: ../source/hw/pci/msix.c:122: msix_fire_vector_notifier: Assertion `ret >= 0' failed."
LOOP_PERIOD = 0.5
HOSTMEM_BAR = 4
"""The BAR virtio-gpu-pci registers the host window in."""
INSPECTOR = "avinspect"
SNAPSHOT_STATE = 2
HOSTMEM_BASE = 0xC0000000
"""Where this fake's guest programmed that BAR."""
MODERN_BAR_BASE = 0xB0000000
PROGRAMS = ("avtest", "avvkprobe", "avglprobe", "avclprobe", "avd3d9probe", "avd3d11probe",
            "avd3d12probe", "vulkaninfo", "reg", "deqp-vk")
"""The programs the runner starts itself rather than through provision.ps1."""


def option(argv, flag, kind=None):
    for index, word in enumerate(argv[:-1]):
        if word == flag and (kind is None or argv[index + 1].startswith(kind)):
            return argv[index + 1]
    return None


def argument(arguments, name):
    """The value after `name` in a provision.ps1 argument list."""
    return arguments[arguments.index(name) + 1] if name in arguments else ""


def field(spec, name):
    for part in spec.split(","):
        if part.startswith(name + "="):
            return part[len(name) + 1:]
    return None


def size_property(value):
    """A QEMU size property - "256M" - in bytes; 0 where the device has none."""
    return int(value[:-1]) << 20 if value else 0


class Guest:
    def __init__(self, script, journal, device, hostmem):
        self.script = script
        self.journal = journal
        self.device = device
        self.hostmem = hostmem
        self.lock = threading.Lock()
        self.send_lock = threading.Lock()
        self.calls = {}
        self.files = {}
        self.written = {}
        self.processes = {}
        self.tasks = {}
        self.qmp = None
        self.up = True
        self.suspended = False

    def note(self, line):
        with self.lock, open(self.journal, "a") as out:
            out.write(line + "\n")

    def emit(self, event, data=None):
        message = {"event": event, "timestamp": {"seconds": 0, "microseconds": 0}}
        if data is not None:
            message["data"] = data
        if self.qmp is not None:
            with self.send_lock:
                self.qmp.sendall(json.dumps(message).encode() + b"\n")

    def bugcheck(self):
        panic = self.script["panic"]
        self.emit("GUEST_PANICKED", {"action": "run", "info": {
            "type": "hyper-v", "arg1": panic.get("code", 0xD1), "arg2": 0x10, "arg3": 2, "arg4": 0, "arg5": 0xfffff80}})

    def crash(self):
        self.note("panic")
        self.up = False
        self.bugcheck()
        time.sleep(REBOOT)
        self.emit("RESET")
        if not self.script["panic"].get("loop"):
            self.up = True
            return
        while True:
            time.sleep(LOOP_PERIOD)
            self.bugcheck()
            self.emit("RESET")

    def display_modes(self):
        """Every mode the guest is offered; the default is what a 1280x800
        display holds."""
        return self.script.get("modes", ["640x480", "800x600", "1024x768", "1280x720", "1280x800",
                                         "1920x1080"])   # Windows lists more than the driver offers

    def mode_stage(self, name, arguments):
        """The display stages: what the guest's mode list says, and what a
        switch does to the scanout the host sees."""
        if name == "modes":
            width, height = self.script.get("screendump", [1280, 800])[:2]
            lines = [f"mode: {mode}" for mode in self.display_modes()]
            return 0, "\n".join(lines + [f"current: {width}x{height}"]) + "\n"
        size = f"{argument(arguments, '-Width')}x{argument(arguments, '-Height')}"
        if size not in self.display_modes() or self.script.get("setmode_refused"):
            return 0, "set: -2\n"   # DISP_CHANGE_BADMODE
        if not self.script.get("screendump_fixed"):
            self.script["screendump"] = [int(part) for part in size.split("x")]
        return 0, "set: 0\n"

    def shell(self, arguments):
        """cmd.exe /c PROGRAM ARG... > FILE 2>&1: the program runs as the agent
        would run it, and what it printed becomes FILE for a pull."""
        line = arguments[1] if len(arguments) == 2 and arguments[0] == "/c" else ""
        command, _, redirect = line.partition(" > ")
        target = redirect.removesuffix(" 2>&1")
        if not command or not target:
            return {"exited": True, "exitcode": 1, "out-data": "", "err-data": ""}
        words = command.split()
        ran = self.process(words[0], words[1:])
        if ran["exited"]:
            self.script.setdefault("files", {})[target] = base64.b64decode(ran["out-data"]).decode("latin-1")
        return dict(ran, **{"out-data": ""})

    def schtasks(self, arguments):
        """schtasks /create|/run|/query|/delete, as ConsoleProgram drives it: a
        program run in the console session (feature 046). /run executes the .cmd
        the task names, which the runner pushed, so its output lands in the file
        that .cmd redirects to - exactly as a run through cmd.exe does.
        """
        verb = arguments[0].lstrip("/").lower() if arguments else ""
        name = option(arguments, "/tn")
        if verb == "create":
            self.tasks[name] = {"command": option(arguments, "/tr"), "result": 0, "ran": False}
            return {"exited": True, "exitcode": 0,
                    "out-data": base64.b64encode(b"SUCCESS: created\n").decode(), "err-data": ""}
        if name not in self.tasks:
            return {"exited": True, "exitcode": 1,
                    "out-data": "", "err-data": base64.b64encode(b"ERROR: cannot find the task\n").decode()}
        if verb == "delete":
            del self.tasks[name]
            return {"exited": True, "exitcode": 0, "out-data": "", "err-data": ""}
        if verb == "run":
            task = self.tasks[name]
            pushed = self.written.get(task["command"], b"").decode("latin-1")
            line = next((text for text in pushed.splitlines() if ">" in text), "")
            ran = self.shell(["/c", line]) if line else {"exited": True, "exitcode": 1}
            task["result"] = ran.get("exitcode", 1)
            task["ran"] = True
            return {"exited": True, "exitcode": 0,
                    "out-data": base64.b64encode(b"SUCCESS: attempted to run\n").decode(), "err-data": ""}
        if verb == "query":
            task = self.tasks[name]
            task["queries"] = task.get("queries", 0) + 1
            running = task["queries"] <= self.script.get("console_running", 0)
            listing = (f"TaskName: {name}\r\nStatus: {'Running' if running else 'Ready'}\r\n"
                       f"Last Result: {task['result']}\r\n")
            return {"exited": True, "exitcode": 0,
                    "out-data": base64.b64encode(listing.encode()).decode(), "err-data": ""}
        return {"exited": True, "exitcode": 1, "out-data": "", "err-data": ""}

    def inspect(self, arguments):
        """avinspect.exe COMMAND ... --out F: the scripted answer lands in F."""
        answers = self.script.get("inspect", {})
        command = arguments[0] if arguments else ""
        target = arguments[arguments.index("--out") + 1] if "--out" in arguments[:-1] else None
        if target is None or command not in answers:
            return {"exited": True, "exitcode": 1, "out-data": "", "err-data": ""}
        answer = answers[command]
        if command in ("trace", "kernel-trace"):
            content = base64.b64decode(answer)
        elif command in ("state", "kernel-state"):
            text = answer.encode()
            content = b"AVSNAP1\0" + struct.pack("<QII", 0, SNAPSHOT_STATE, len(text)) + text
        else:
            content = answer.encode()
        self.script.setdefault("files", {})[target] = content.decode("latin-1")
        return {"exited": True, "exitcode": 0, "out-data": "", "err-data": ""}

    def powershell_script(self, arguments):
        """powershell -File F, F pushed by the caller: the scripted output for it."""
        pushed = self.written.get(arguments[arguments.index("-File") + 1], b"").decode("latin-1")
        for needle, output in self.script.get("powershell", {}).items():
            if needle in pushed:
                return {"exited": True, "exitcode": 0,
                        "out-data": base64.b64encode(output.encode()).decode(), "err-data": ""}
        return {"exited": True, "exitcode": 0, "out-data": "", "err-data": ""}

    def process(self, path, arguments):
        program = path.rsplit("\\", 1)[-1].lower().removesuffix(".exe")
        if program == "cmd":
            self.note(f"exec {path} {' '.join(arguments)}")
            return self.shell(arguments)
        if program == "schtasks":
            self.note(f"exec {path} {' '.join(arguments)}")
            return self.schtasks(arguments)
        if program == INSPECTOR:
            self.note(f"exec {path} {' '.join(arguments)}")
            return self.inspect(arguments)
        if program == "powershell" and "-Command" in arguments[:-1] and "UserName" in arguments[-1]:
            self.note(f"exec {path} {' '.join(arguments)}")
            user = self.script.get("console_user", "guest\\tester")
            return {"exited": True, "exitcode": 0,
                    "out-data": base64.b64encode(f"{user}\r\n".encode()).decode(), "err-data": ""}
        if program == "powershell" and "-File" in arguments[:-1] and "-Stage" not in arguments:
            self.note(f"exec {path} {' '.join(arguments)}")
            return self.powershell_script(arguments)
        name = program if program in PROGRAMS else None
        if "-Stage" in arguments:
            name = arguments[arguments.index("-Stage") + 1]
        self.note(f"exec {path} {' '.join(arguments)}")
        count = self.calls.get(name, 0)
        self.calls[name] = count + 1
        if name is not None and name == self.script.get("die_during"):
            sys.stderr.write(ASSERTION + "\n")
            sys.stderr.flush()
            os.kill(os.getpid(), signal.SIGKILL)
        panic = self.script.get("panic")
        if self.device and panic is not None and name == panic["during"] and count == 0:
            threading.Thread(target=self.crash, daemon=True).start()
            return {"exited": False}
        if name == "monitortimeout" and name not in self.script.get("outputs", {}):
            minutes = argument(arguments, "-Minutes")
            if minutes != "0" and not self.script.get("power_stays_lit"):
                self.script["screendump_dark"] = True
            return {"exited": True, "exitcode": 0,
                    "out-data": base64.b64encode(f"timeout: {minutes}\n".encode()).decode(), "err-data": ""}
        if name == "extend" and name not in self.script.get("outputs", {}):
            heads = self.script.get("extend_heads", 0)
            if heads != 0:
                self.script["screendump_heads"] = heads
            return {"exited": True, "exitcode": 0,
                    "out-data": base64.b64encode(b"extend: 0\n").decode(), "err-data": ""}
        if name in ("modes", "setmode") and name not in self.script.get("outputs", {}):
            code, text = self.mode_stage(name, arguments)
            return {"exited": True, "exitcode": code,
                    "out-data": base64.b64encode(text.encode()).decode(), "err-data": ""}
        if name in PROGRAMS:
            code = self.script.get(name, 0)
        else:
            codes = self.script.get("stages", {}).get(name, [0])
            code = codes[min(count, len(codes) - 1)]
        out = self.script.get("outputs", {}).get(name, "").encode()
        hung = name is not None and name == self.script.get("hang")
        return {"exited": not hung, "exitcode": code,
                "out-data": base64.b64encode(out).decode(), "err-data": ""}

    def answer(self, request, connection):
        command = request.get("execute")
        arguments = request.get("arguments", {})
        if command == "guest-sync-delimited":
            return b"\xff" + json.dumps({"return": arguments["id"]}).encode()
        if command == "guest-ping":
            return json.dumps({"return": {}}).encode()
        if command == "guest-exec":
            pid = len(self.processes) + 100
            self.processes[pid] = self.process(arguments["path"], arguments.get("arg", []))
            return json.dumps({"return": {"pid": pid}}).encode()
        if command == "guest-exec-status":
            status = dict(self.processes[arguments["pid"]])
            if not status["exited"]:
                status = {"exited": False}
            return json.dumps({"return": status}).encode()
        if command == "guest-file-open":
            path = arguments["path"]
            if arguments["mode"] == "rb" and path not in self.script.get("files", {}):
                return json.dumps({"error": {"class": "GenericError", "desc": f"no file {path}"}}).encode()
            handle = len(self.files) + 1000
            self.files[handle] = [path, b"", self.script.get("files", {}).get(path, "").encode("latin-1")]
            return json.dumps({"return": handle}).encode()
        if command == "guest-file-write":
            data = base64.b64decode(arguments["buf-b64"])
            self.files[arguments["handle"]][1] += data
            return json.dumps({"return": {"count": len(data), "eof": False}}).encode()
        if command == "guest-file-read":
            entry = self.files[arguments["handle"]]
            data, entry[2] = entry[2][:arguments["count"]], entry[2][arguments["count"]:]
            return json.dumps({"return": {"count": len(data), "buf-b64": base64.b64encode(data).decode(),
                                          "eof": not entry[2]}}).encode()
        if command == "guest-file-close":
            path, written, _ = self.files[arguments["handle"]]
            if written:
                self.written[path] = written
            self.note(f"file {path} {len(written)}" if written else f"pull {path}")
            return json.dumps({"return": {}}).encode()
        if command in ("guest-suspend-ram", "guest-suspend-disk"):
            self.note(command)
            if command in self.script.get("refuse", []):
                return json.dumps({"error": {"class": "GenericError", "desc": f"{command} is not supported"}}).encode()
            # Down before the socket closes: a wakeup that arrives while this
            # is still deciding must not be undone by it.
            self.up = False
            connection.close()
            if command == "guest-suspend-disk":
                # S4 leaves the machine off; the runner boots the overlay again.
                self.emit("SHUTDOWN")
                os._exit(0)
            self.suspended = True
            # QEMU says when the machine has actually entered S3, which is what
            # a wakeup has to wait for.
            self.emit("SUSPEND")
            return None
        if command == "guest-shutdown":
            self.note(arguments["mode"])
            connection.close()
            if arguments["mode"] == "powerdown":
                self.emit("SHUTDOWN")
                os._exit(0)
            self.up = False
            if not self.script.get("no_reset"):
                self.emit("RESET")
            time.sleep(REBOOT)
            self.up = True
            return None
        return json.dumps({"error": {"class": "CommandNotFound", "desc": command}}).encode()

    def serve_agent(self, path):
        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        server.bind(path)
        server.listen(4)
        while True:
            connection, _ = server.accept()
            threading.Thread(target=self.agent_connection, args=(connection,), daemon=True).start()

    def agent_connection(self, connection):
        buffer = b""
        while True:
            try:
                data = connection.recv(65536)
            except OSError:
                return
            if not data:
                return
            buffer += data.replace(b"\xff", b"")
            while b"\n" in buffer:
                line, buffer = buffer.split(b"\n", 1)
                if self.script.get("agent_silent") or not self.up:
                    continue
                reply = self.answer(json.loads(line), connection)
                if reply is None:
                    return
                if self.script.get("garbage"):
                    reply = b"}{ not json\n[1,2]\n" + reply
                time.sleep(self.script.get("answer_delay", 0))
                try:
                    connection.sendall(reply + b"\n")
                except OSError:
                    return

    def pci_devices(self):
        """What QEMU says about the bus: the runner reads the hostmem BAR's
        guest physical address out of it, having no config space of its own."""
        regions = [{"bar": 2, "address": MODERN_BAR_BASE, "size": 0x4000, "type": "memory",
                    "prefetch": False, "mem_type_64": True}]
        if self.hostmem:
            regions.append({"bar": HOSTMEM_BAR, "address": HOSTMEM_BASE, "size": self.hostmem,
                            "type": "memory", "prefetch": True, "mem_type_64": True})
        return [{"bus": 0, "devices": [{"bus": 0, "slot": 1, "function": 0, "qdev_id": "gpu",
                                        "regions": regions}]}]

    def memory_image(self, address, size):
        """The host's own view of a guest physical range: zeros, but for the
        trace table the script puts there."""
        image = bytearray(size)
        traced = self.script.get("trace_memory")
        if traced is not None and address == traced["address"]:
            table = base64.b64decode(traced["b64"])[:size]
            image[:len(table)] = table
        return bytes(image[:size])

    def qmp_command(self, request):
        """screendump writes a PPM of the scripted size, as QEMU would; input
        events wake a display the power policy turned off; pmemsave hands out
        the host's own read of guest physical memory."""
        if request.get("execute") == "query-pci":
            return self.pci_devices()
        if request.get("execute") == "pmemsave":
            arguments = request.get("arguments", {})
            self.note(f"pmemsave {arguments['val']:#x} {arguments['size']}")
            if not self.script.get("pmemsave_nothing"):
                with open(arguments["filename"], "wb") as out:
                    out.write(self.memory_image(arguments["val"], arguments["size"]))
            return None
        if request.get("execute") == "system_wakeup":
            self.note("system_wakeup")
            self.suspended = False
            self.up = True
            self.emit("WAKEUP")
            return
        if request.get("execute") == "input-send-event":
            self.note("input-send-event")
            self.script.pop("screendump_dark", None)
            return
        if request.get("execute") != "screendump":
            return
        width, height = self.script.get("screendump", [1280, 800])[:2]
        frame = self.script.get("screendump_frame", 0)
        head = request.get("arguments", {}).get("head", 0)
        path = request.get("arguments", {})["filename"]
        with open(path, "wb") as dump:
            dump.write(f"P6\n{width} {height}\n255\n".encode())
            if self.script.get("screendump_placeholder") or self.script.get("screendump_dark"):
                # QEMU's "display output is not active": grey text on black.
                pixels = bytearray(b"\x00" * (width * height * 3))
                pixels[:2052] = b"\xaa" * 2052
                dump.write(bytes(pixels))
            else:
                # Not one flat colour: vary per pixel, as a real frame does.
                bytes_needed = width * height * 3
                ramp = bytes(range(frame & 0xFF, 256)) + bytes(range(frame & 0xFF))
                dump.write((ramp * (bytes_needed // len(ramp) + 1))[:bytes_needed])
        self.note(f"screendump {width}x{height}" + (f" head {head}" if head != 0 else ""))

    def serve_qmp(self, path):
        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        server.bind(path)
        server.listen(1)
        connection, _ = server.accept()
        connection.sendall(json.dumps({"QMP": {"version": {}, "capabilities": []}}).encode() + b"\n")
        buffer = b""
        while True:
            try:
                data = connection.recv(65536)
            except OSError:
                data = b""
            if not data:
                while True:
                    time.sleep(1)
            buffer += data
            while b"\n" in buffer:
                line, buffer = buffer.split(b"\n", 1)
                request = json.loads(line)
                self.qmp = connection
                answer = self.qmp_command(request)
                with self.send_lock:
                    connection.sendall(json.dumps({"return": {} if answer is None else answer}).encode() + b"\n")


def append_later(path, lines, seconds):
    time.sleep(seconds)
    with open(path, "a") as log:
        log.write("".join(line + "\n" for line in lines))


def main():
    argv = sys.argv[1:]
    script = json.load(open(os.environ["FAKE_GUEST_SCRIPT"]))
    device = any(word.startswith("virtio-gpu-av-pci") for word in argv)
    hostmem = size_property(field(option(argv, "-device", "virtio-gpu-av-pci") or "", "hostmem"))
    guest = Guest(script, os.environ["FAKE_GUEST_JOURNAL"], device, hostmem)
    guest.note("argv " + " ".join(argv))
    # What the device's executor was told to do, as the runner set it: the
    # environment is how a test executor is steered without a rebuild, and
    # how the Vulkan layer that loses a device is loaded into it.
    work = os.path.dirname(os.environ["FAKE_GUEST_JOURNAL"])
    with open(os.path.join(work, "work", "environment.json"), "w") as out:
        json.dump({name: value for name, value in os.environ.items() if name.startswith(("AV_ECHO_", "VK_INSTANCE_LAYERS", "VK_ADD_LAYER_PATH"))}, out)
    if script.get("exit_at_boot") or (not device and os.environ.get("FAKE_QEMU_RESCUE_FAILS")):
        sys.exit(1)
    agent = field(option(argv, "-chardev", "socket,id=agent"), "path")
    debug = field(option(argv, "-chardev", "file,id=debugcon"), "path")
    qmp = option(argv, "-qmp").split(",")[0][len("unix:"):]
    disk = field(option(argv, "-drive", "if=none,id=disk"), "file")
    if not os.path.isfile(disk):
        sys.exit(f"fake qemu: no disk at {disk}")
    with open(debug, "w") as log:
        lines = script.get("driver_log", ["avgpu: DriverEntry",
                                          "avgpu: aperture map segment 1 offset 0 pages 1000 status 0",
                                          "avgpu: vsync 600 periods in 100000000 ticks, 600 reported, enabled 0 times"])
        # A device with a window makes the driver say what came of setting it
        # up, whatever else the scenario had it log, because the line belongs
        # to the device rather than to the run.
        status = script.get("window_status", "0x00000000")
        if hostmem and status is not None:
            lines = lines + [f"avgpu: host window status {status}"]
        log.write("".join(line + "\n" for line in lines) if device else "")
    # A driver goes on logging while the run reads: what it says every 120
    # blanks arrives long after the line before it.
    late = script.get("driver_log_late", [])
    if device and late:
        threading.Thread(target=append_later, daemon=True,
                         args=(debug, late, script.get("driver_log_late_seconds", 1.5))).start()
    threading.Thread(target=guest.serve_agent, args=(agent,), daemon=True).start()
    guest.serve_qmp(qmp)


if __name__ == "__main__":
    main()
