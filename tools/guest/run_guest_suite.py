#!/usr/bin/env python3
"""The in-guest suite (docs/testing.md, L5): one command from a build to a
verdict from a real Windows guest, with the evidence to debug it.

A copy-on-write overlay of a prepared image boots on the patched QEMU with
virtio-gpu-rvk-pci, the guest agent's channel, QMP and the driver's debug
port. The package is pushed, test-signed and installed with Driver Verifier's
standard checks, each reboot waited through; the adapter must have started on
rvkgpu; rvktest.exe runs. Whatever happened, the guest's event logs, device
install log, device and verifier state and live kernel dumps are pulled into
diag/. A bugcheck is named from the Hyper-V crash MSRs, and its kernel dump is
pulled after the guest reboots - or, when it bugchecks on every boot, from the
same overlay booted again without the device. The guest is powered off and the
overlay deleted, so the image is never written.

  run_guest_suite.py --image windows.qcow2 [--work build-guest] [options]
  run_guest_suite.py --image windows.qcow2 --prepare-image

--prepare-image boots the image itself, once, and readies it for runs: test
signing, kernel dumps, and nothing that interrupts a run. --hold keeps a failed
guest running for tools/guest/guest_shell.py, --gdb, --vnc and QMP.

A prepared image: Windows 11 24H2 or later, x64, UEFI without Secure Boot,
the QEMU guest agent installed and running. Exits 0 only when every step
passed and the report counts no failure.
"""
import argparse
import collections
import os
import pathlib
import re
import shutil
import signal
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from guest_agent import AgentError, Qmp, wait_for_agent  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
GUEST_DIRECTORY = "C:\\rvk"
PACKAGE_DIRECTORY = GUEST_DIRECTORY + "\\package"
POWERSHELL = "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"
REBOOT_REQUIRED = 3010
SUMMARY = re.compile(rb"^rvktest: (\d+) passed, (\d+) failed$", re.M)
# The guest ends its lines the Windows way, so nothing anchored at $ may
# assume the line stops at the newline.
CYCLES_DONE = re.compile(r"^cycle: (\d+) done\r?$", re.M)
UPGRADED = re.compile(r"^upgrade: installed (\S+) running (\S+) problem 0\r?$", re.M)
STRESS_FENCES = re.compile(rb"^ok stress\.fences (\d+)\r?$", re.M)
STRESS_WORST = re.compile(rb"^ok stress\.worst_wait_us (\d+)\r?$", re.M)
MANIFEST = re.compile(r"^(file|dump) (.+?)\r?$", re.M)
# The enlightenments Windows expects of KVM, and hv-crash: Windows writes its
# bugcheck code and parameters to the crash MSRs, which QEMU reports as
# GUEST_PANICKED while the guest goes on to write its dump.
HYPERV = ("hv-relaxed,hv-vapic,hv-spinlocks=0x1fff,hv-vpindex,hv-runtime,hv-synic,hv-stimer,hv-time,"
          "hv-frequencies,hv-tlbflush,hv-ipi,hv-crash")
# Events already queued arrive at once; a second of silence means none is coming.
EVENT_QUIET = 1.0
EVENT_POLL = 0.05
# QMP closing means QEMU is on its way out; its exit status follows at once.
QEMU_EXIT_GRACE = 5.0
BUGCHECKS = {
    0x0A: "IRQL_NOT_LESS_OR_EQUAL", 0x1E: "KMODE_EXCEPTION_NOT_HANDLED", 0x3B: "SYSTEM_SERVICE_EXCEPTION",
    0x50: "PAGE_FAULT_IN_NONPAGED_AREA", 0x7B: "INACCESSIBLE_BOOT_DEVICE",
    0x7E: "SYSTEM_THREAD_EXCEPTION_NOT_HANDLED", 0x7F: "UNEXPECTED_KERNEL_MODE_TRAP",
    0xC4: "DRIVER_VERIFIER_DETECTED_VIOLATION", 0xC9: "DRIVER_VERIFIER_IOMANAGER_VIOLATION",
    0xD1: "DRIVER_IRQL_NOT_LESS_OR_EQUAL", 0xEF: "CRITICAL_PROCESS_DIED", 0x10E: "VIDEO_MEMORY_MANAGEMENT_INTERNAL",
    0x113: "VIDEO_DXGKRNL_FATAL_ERROR", 0x116: "VIDEO_TDR_FAILURE", 0x119: "VIDEO_SCHEDULER_INTERNAL_ERROR",
    0x117: "VIDEO_TDR_TIMEOUT_DETECTED", 0x133: "DPC_WATCHDOG_VIOLATION",
    0x139: "KERNEL_SECURITY_CHECK_FAILURE", 0x141: "VIDEO_ENGINE_TIMEOUT_DETECTED",
    0x1A8: "VIDEO_DXGKRNL_BLACK_SCREEN_LIVEDUMP",
}
# DUMP_HEADER64: the signature it starts with, and where it keeps BugCheckCode.
LIVE_DUMP_HEADER = b"PAGEDU64"
LIVE_DUMP_CODE = 0x38
# A live kernel dump is the guest telling on itself without bugchecking, and its
# code says what for. A black screen is the one this driver expects until
# something renders into the primary (features 042-044): dxgkrnl takes the dump
# because the source shows nothing, and our pages are still zeros. Every other
# code - a TDR above all - is this driver's own failure, and SC-005 says so.
BLACK_SCREEN_LIVEDUMP = 0x1A8
MEMORY_DUMP = "MEMORY.DMP"


class StepFailed(Exception):
    """A named step of the run did not do what it had to."""

    def __init__(self, step, reason):
        super().__init__(f"{step}: {reason}")
        self.step = step
        self.reason = reason


def arguments():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--image", default=os.environ.get("AV_GUEST_IMAGE"),
                        help="the prepared Windows qcow2 (default: $AV_GUEST_IMAGE)")
    parser.add_argument("--work", type=pathlib.Path, default=ROOT / "build-guest")
    parser.add_argument("--memory-dump", action="store_true",
                        help="also collect C:\\Windows\\MEMORY.DMP. It is copied through the guest agent in "
                             "base64 chunks, so a kernel dump routinely outlasts the step timeout and takes "
                             "the rest of the manifest - the minidumps - down with it")
    parser.add_argument("--prepare-image", action="store_true",
                        help="boot the image itself and ready it for runs, instead of running the suite")
    parser.add_argument("--qemu", default=str(ROOT / "build-qemu" / "qemu" / "qemu-system-x86_64"))
    parser.add_argument("--qemu-img", default="qemu-img")
    parser.add_argument("--executor", default=str(ROOT / "build-qemu" / "librvk_echo_executor.so"))
    parser.add_argument("--driver", default=str(ROOT / "build" / "driver" / "rvkgpu.sys"))
    parser.add_argument("--inf", default=str(ROOT / "src" / "km" / "rvkgpu.inf"))
    parser.add_argument("--test", default=str(ROOT / "build" / "guest" / "rvktest.exe"))
    parser.add_argument("--provision", default=str(ROOT / "tools" / "guest" / "provision.ps1"))
    parser.add_argument("--firmware", default="/usr/share/edk2/x64/OVMF_CODE.4m.fd")
    parser.add_argument("--firmware-vars", default="/usr/share/edk2/x64/OVMF_VARS.4m.fd")
    parser.add_argument("--memory", type=int, default=4096, help="MiB")
    parser.add_argument("--cpus", type=int, default=4)
    parser.add_argument("--outputs", type=int, default=1, help="the device's max_outputs")
    parser.add_argument("--power-poll", type=float, default=5.0, help="seconds between display-power screendumps")
    parser.add_argument("--upgrades", type=int, default=0,
                        help="install this many higher-versioned packages over the running one (feature 040, US2)")
    parser.add_argument("--power-cycles", type=int, default=0,
                        help="disable and enable the adapter this many times (feature 040, US2)")
    parser.add_argument("--stall-ms", type=int, default=0,
                        help="make the test executor take this long per command (feature 040, US3)")
    parser.add_argument("--fault-after", type=int, default=0,
                        help="make the test executor lose the device on its Nth command")
    parser.add_argument("--s3", action="store_true",
                        help="let the machine advertise S3; this firmware cannot resume from it")
    parser.add_argument("--hibernate", action="store_true",
                        help="also suspend to disk and boot the overlay again (feature 040, US1)")
    parser.add_argument("--stress-minutes", type=float, default=0.0,
                        help="minutes of fences awaited while the blank's interrupt is masked and unmasked "
                             "(feature 039, US3); 0 leaves the long run out")
    parser.add_argument("--accel", default="kvm")
    parser.add_argument("--boot-timeout", type=float, default=900.0)
    parser.add_argument("--step-timeout", type=float, default=600.0)
    parser.add_argument("--agent-timeout", type=float, default=30.0)
    parser.add_argument("--on-panic", choices=("reboot", "pause"), default="reboot",
                        help="reboot: let the guest write its dump and reboot (default); "
                             "pause: stop the guest at the bugcheck for --gdb")
    parser.add_argument("--hold", action="store_true",
                        help="on failure, keep the guest running until Enter is pressed")
    parser.add_argument("--gdb", type=int, metavar="PORT", help="QEMU's gdbstub on 127.0.0.1:PORT")
    parser.add_argument("--vnc", type=int, metavar="DISPLAY", help="the guest's display on 127.0.0.1:5900+DISPLAY")
    parser.add_argument("--trace", action="append", default=[], metavar="PATTERN",
                        help="enable QEMU trace events matching PATTERN into qemu.log, e.g. 'virtio_gpu_*'")
    parsed = parser.parse_args()
    if not parsed.image:
        parser.error("no image: pass --image or set AV_GUEST_IMAGE")
    return parsed


def qemu_command(args, work, disk, rescue):
    """QEMU for a run; a rescue boot leaves the device out so the driver never loads."""
    suffix = "-rescue" if rescue else ""
    device = [] if rescue else ["-device", f"virtio-gpu-rvk-pci,id={DISPLAY_DEVICE},host-gpu=auto,"
                                          f"executor={args.executor},max_outputs={args.outputs}"]
    command = [
        args.qemu, "-machine", f"q35,accel={args.accel},memory-backend=ram",
        # OVMF on q35 advertises S3 through ACPI and cannot resume from it: the
        # guest wakes and its HAL bugchecks 0x5C HAL_INITIALIZATION_FAILED
        # (0x201, ..., STATUS_UNSUCCESSFUL) before anything the driver does
        # after D0. The machine says so instead of lying, unless --s3 says the
        # firmware here can do it (feature 040, US1).
        "-global", f"ICH9-LPC.disable_s3={0 if args.s3 else 1}",
        "-cpu", f"host,{HYPERV}" if args.accel == "kvm" else "max",
        "-smp", str(args.cpus), "-m", f"{args.memory}M",
        "-object", f"memory-backend-memfd,id=ram,size={args.memory}M,share=on",
        "-drive", f"if=pflash,format=raw,readonly=on,file={args.firmware}",
        "-drive", f"if=pflash,format=raw,file={work / 'vars.fd'}",
        "-drive", f"if=none,id=disk,format=qcow2,file={disk}",
        "-device", "ide-hd,drive=disk,bus=ide.0",
        *device,
        "-device", "virtio-serial-pci",
        "-chardev", f"socket,id=agent,path={work / 'agent.sock'},server=on,wait=off",
        "-device", "virtserialport,chardev=agent,name=org.qemu.guest_agent.0",
        "-chardev", f"file,id=debugcon,path={work / f'rvkgpu{suffix}.log'}",
        "-device", "isa-debugcon,iobase=0x402,chardev=debugcon",
        "-qmp", f"unix:{work / 'qmp.sock'},server=on,wait=off",
        "-action", f"panic={'pause' if args.on_panic == 'pause' else 'none'}",
        "-msg", "timestamp=on", "-d", "guest_errors",
        "-vga", "none", "-display", "none", "-nic", "none",
    ]
    if args.vnc is not None:
        command += ["-vnc", f"127.0.0.1:{args.vnc}"]
    if args.gdb is not None:
        command += ["-gdb", f"tcp:127.0.0.1:{args.gdb}"]
    for pattern in args.trace:
        command += ["-trace", f"enable={pattern}"]
    return command


def fingerprint(path):
    """Size and modification time: hashing a guest image would take minutes."""
    status = os.stat(path)
    return status.st_size, status.st_mtime_ns


def describe_panic(data):
    info = data.get("info") or {}
    if info.get("type") != "hyper-v":
        return f"the guest panicked (action {data.get('action', 'unknown')}) without bugcheck parameters"
    code = info.get("arg1", 0)
    parameters = ", ".join(f"{info.get(f'arg{number}', 0):#x}" for number in range(2, 6))
    return f"bugcheck {code:#x} {BUGCHECKS.get(code, 'unnamed')} ({parameters})"


DISPLAY_SIZE = (1280, 800)
"""The mode the guest commits with QEMU's default -display size."""
PLACEHOLDER_GREY = b"\xaa\xaa\xaa"
"""The grey QEMU draws its "display output is not active" text in."""
DISPLAY_DEVICE = "gpu"
"""The device id screendump names when it asks for a head other than the first."""
REFRESH_HZ = 60
"""DisplayMode::refresh_hz: the rate every mode is committed at."""
VSYNC_TOLERANCE_PERCENT = 5
"""SC-003."""
TICKS_PER_SECOND = 10000000
"""The 100 ns units the driver's interrupt time counts in."""
VSYNC_LINE = re.compile(r"rvkgpu: vsync (\d+) blanks in (\d+) ticks, enabled (\d+) times")
ACCESS_DENIED = 5
STRESS_PATIENCE = 2.0
"""How much longer than the stress window the guest is given to finish it."""
"""What SetDisplayConfig answers a caller outside the console session."""
POWER_ATTEMPTS = 24
"""Screendumps to wait for the display to go dark or come back: with the default
poll that is two minutes, and the policy's shortest timeout is one."""
SETTLE_SECONDS = 1.0
"""Long enough for the DPC that follows a commit to have run."""
OFFERED_MODES = ["1024x768", "1280x800"]
"""The modes this run works with, both of which
src/core/display/display_mode_list.cpp offers for a 1280x800 display.
EnumDisplaySettings is not the driver's list - Windows adds sizes of its own
above the display (1920x1200, 2560x1600) and drops some of the driver's
(1280x720) - so the run asserts that these two are offered, not that the list
is the driver's."""
SWITCH_MODE = "1024x768"
"""The mode the run asks for. The agent is a session 0 service, and the display
the host scans out belongs to the console session, so Windows may refuse the
switch outright; what must hold either way is that the host's scanout follows
a switch that was taken and does not move for one that was not."""
UNOFFERED_MODE = "2560x1600"
"""Larger than the display: the driver never offers it, so it must be refused
rather than committed and scanned out."""


class Screendump:
    """One QMP screendump: the device's scanout as QEMU writes it, in PPM.

    QEMU answers screendump for whatever the device is scanning out, with
    -display none and no VGA, which is how a run verifies that a frame reached
    the host at all.
    """

    def __init__(self, path):
        self.path = pathlib.Path(path)

    def capture(self, qmp, head=0):
        """Writes the dump of `head`; raises StepFailed when QEMU refuses or
        writes nothing."""
        arguments = {"filename": str(self.path)}
        if head != 0:
            arguments.update({"device": DISPLAY_DEVICE, "head": head})
        try:
            qmp.execute("screendump", arguments)
        except AgentError as error:
            raise StepFailed("screendump", str(error))
        if not self.path.exists() or self.path.stat().st_size == 0:
            raise StepFailed("screendump", f"QEMU wrote no dump to {self.path}")
        return self

    def size(self):
        """The PPM's width and height, or (0, 0) when it is not a P6 PPM."""
        with open(self.path, "rb") as dump:
            head = dump.read(64)
        fields = head.split(maxsplit=4)
        if len(fields) < 3 or fields[0] != b"P6":
            return (0, 0)
        try:
            return (int(fields[1]), int(fields[2]))
        except ValueError:
            return (0, 0)

    def pixels(self):
        with open(self.path, "rb") as dump:
            return dump.read()

    def differs_from(self, other):
        return self.pixels() != other.pixels()


class GuestRun:
    """One run: its work directory, its QEMU, and the sockets into the guest."""

    def __init__(self, args):
        self.args = args
        self.work = args.work.resolve()
        self.disk = None
        self.qemu = None
        self.qemu_log = None
        self.qmp = None
        self.qmp_lost = False
        self.agent = None
        self.log = []
        self.colours = 0
        self.heads = 0
        self.unexpected_dump = None
        self.sleep_states = None

    def say(self, line):
        self.log.append(line)
        print(f"guest: {line}", flush=True)

    def step(self, name, action):
        try:
            result = action()
        except AgentError as error:
            raise StepFailed(name, str(error)) from error
        self.say(f"{name} ok")
        return result

    def prepare(self):
        required = [self.args.image, self.args.provision]
        if not self.args.prepare_image:
            required += [self.args.driver, self.args.inf, self.args.test]
        for path in required:
            if not os.path.isfile(path):
                raise StepFailed("prepare", f"{path} does not exist")
        for stale in ("overlay.qcow2", "vars.fd", "agent.sock", "qmp.sock", "rvkgpu.log", "rvkgpu-rescue.log",
                      "qemu-rescue.log", "panic.txt"):
            (self.work / stale).unlink(missing_ok=True)
        for stale in ("diag", "symbols"):
            shutil.rmtree(self.work / stale, ignore_errors=True)
        shutil.copyfile(self.args.firmware_vars, self.work / "vars.fd")
        if self.args.prepare_image:
            self.disk = os.path.abspath(self.args.image)
            return
        self.keep_symbols()
        self.disk = self.work / "overlay.qcow2"
        created = subprocess.run([self.args.qemu_img, "create", "-f", "qcow2", "-F", "qcow2",
                                  "-b", os.path.abspath(self.args.image), str(self.disk)],
                                 capture_output=True, text=True)
        if created.returncode != 0:
            raise StepFailed("overlay", created.stderr.strip() or f"qemu-img exited {created.returncode}")
        self.say("overlay ok")

    def keep_symbols(self):
        """The driver under test and its PDB beside the dumps it produces: a rebuild replaces both."""
        symbols = self.work / "symbols"
        symbols.mkdir()
        driver = pathlib.Path(self.args.driver)
        for path in (driver, driver.with_suffix(".pdb")):
            if path.is_file():
                shutil.copyfile(path, symbols / path.name)

    def boot(self, rescue=False):
        prefix = "rescue." if rescue else ""
        self.qemu_log = self.work / ("qemu-rescue.log" if rescue else "qemu.log")
        self.qmp_lost = False
        output = open(self.qemu_log, "wb")
        environment = dict(os.environ, QEMU_MODULE_DIR=os.path.dirname(os.path.abspath(self.args.qemu)))
        # What the test executor is told to do to the host's work: a stall
        # longer than TdrDelay is what dxgkrnl times out on (feature 040, US3).
        if self.args.stall_ms:
            environment["RVK_ECHO_STALL_MS"] = str(self.args.stall_ms)
        if self.args.fault_after:
            environment["RVK_ECHO_FAULT_AFTER"] = str(self.args.fault_after)
        self.qemu = subprocess.Popen(qemu_command(self.args, self.work, self.disk, rescue), stdout=output,
                                     stderr=subprocess.STDOUT, env=environment)
        output.close()
        deadline = time.monotonic() + self.args.agent_timeout
        while not (self.work / "qmp.sock").exists():
            if self.qemu.poll() is not None or time.monotonic() > deadline:
                raise StepFailed(f"{prefix}boot", f"QEMU exited {self.qemu.poll()} before QMP listened; see qemu.log")
            time.sleep(0.1)
        self.qmp = self.step(f"{prefix}qmp", lambda: Qmp(str(self.work / "qmp.sock"), self.args.agent_timeout))
        self.connect_agent(f"{prefix}boot")

    def panicked_while_booting(self):
        if self.qmp is None:
            return None
        self.qmp.drain(EVENT_POLL)
        return "the guest panicked while booting" if self.qmp.seen("GUEST_PANICKED") else None

    def connect_agent(self, name, timeout=None):
        if self.agent is not None:
            self.agent.close()
            self.agent = None
        deadline = time.monotonic() + (self.args.boot_timeout if timeout is None else timeout)
        self.agent = self.step(name, lambda: wait_for_agent(str(self.work / "agent.sock"), deadline,
                                                            self.args.agent_timeout,
                                                            abandoned=self.panicked_while_booting))

    def reboot(self, after):
        self.qmp.forget_events("RESET")
        self.agent.notify("guest-shutdown", {"mode": "reboot"})
        self.step(f"{after}.reset", lambda: self.qmp.wait_event("RESET", self.args.boot_timeout))
        self.connect_agent(f"{after}.reboot")

    def push(self):
        files = [(self.args.provision, GUEST_DIRECTORY + "\\provision.ps1")]
        if not self.args.prepare_image:
            files += [(self.args.driver, PACKAGE_DIRECTORY + "\\rvkgpu.sys"),
                      (self.args.inf, PACKAGE_DIRECTORY + "\\rvkgpu.inf"),
                      (self.args.test, GUEST_DIRECTORY + "\\rvktest.exe")]

        def send():
            make = f"New-Item -ItemType Directory -Force -Path {PACKAGE_DIRECTORY}"
            code, _, error = self.agent.run(POWERSHELL, ["-NoProfile", "-Command", make], self.args.step_timeout)
            if code != 0:
                raise AgentError(f"cannot create {PACKAGE_DIRECTORY}: {error.decode(errors='replace')}")
            for local, remote in files:
                self.agent.push(pathlib.Path(local).read_bytes(), remote)

        self.step("push", send)

    def provision(self, name, extra=()):
        return self.step(name, lambda: self.agent.run(
            POWERSHELL, ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", GUEST_DIRECTORY + "\\provision.ps1",
                         "-Stage", name, *extra], self.args.step_timeout))

    def check_monitors(self, output):
        """US4: Windows attaches one monitor to every output the device
        reports, and the check stage counted them."""
        monitors = re.search(r"monitors=(\d+)", output)
        if monitors is None:
            raise StepFailed("check", "the check stage did not count the guest's monitors")
        if int(monitors.group(1)) != self.args.outputs:
            raise StepFailed("check", f"the guest has {monitors.group(1)} monitors, not {self.args.outputs}")
        self.say(f"check ok: {monitors.group(1)} monitor(s)")

    def stage_output(self, name, saved=None, extra=()):
        """The stage's own output, for a step that reads what it said."""
        code, out, error = self.provision(name, extra)
        text = (out + error).decode(errors="replace")
        if saved is not None:
            (self.work / saved).write_bytes(out + error)
        if code not in (0, REBOOT_REQUIRED):
            raise StepFailed(name, f"exited {code}: {text.strip()[-400:]}")
        return text

    def stage(self, name, saved=None):
        """Runs one provisioning stage: whether it asked for a reboot."""
        code, out, error = self.provision(name)
        if saved is not None:
            (self.work / saved).write_bytes(out + error)
        if code not in (0, REBOOT_REQUIRED):
            raise StepFailed(name, f"exited {code}: {(out + error).decode(errors='replace').strip()[-400:]}")
        return code == REBOOT_REQUIRED

    def suite(self):
        seconds = int(self.args.stress_minutes * 60)
        # The guest is given the whole stress window plus the rest of the suite.
        patience = self.args.step_timeout + seconds * STRESS_PATIENCE
        code, out, error = self.step("rvktest", lambda: self.agent.run(GUEST_DIRECTORY + "\\rvktest.exe",
                                                                       [str(seconds)] if seconds else [],
                                                                       patience))
        (self.work / "report.txt").write_bytes(out)
        summary = SUMMARY.search(out)
        if summary is None:
            raise StepFailed("report", f"no summary line (exit {code}): {error.decode(errors='replace')[-200:]}")
        failed = int(summary.group(2))
        if failed != 0 or code != 0:
            raise StepFailed("report", f"{failed} step(s) failed, exit {code}; see report.txt")
        self.say(f"report ok: {int(summary.group(1))} passed")
        self.report_stress(out)

    def report_stress(self, report):
        """What the long fence run saw, when one was asked for (feature 039, US3).
        The suite fails its own steps; this says what the numbers were."""
        fences = STRESS_FENCES.search(report)
        worst = STRESS_WORST.search(report)
        if fences is None or worst is None:
            if self.args.stress_minutes:
                raise StepFailed("stress", "the suite ran no fence stress")
            return
        self.say(f"stress ok: {int(fences.group(1))} fences awaited, worst wait "
                 f"{int(worst.group(1))} us")

    def display(self):
        """The frame the host scans out of the guest's primary (feature 038, US1).

        What a screendump can prove today is that the host reads *our* resource:
        the dump is the committed mode's size, and it is no longer QEMU's "no
        output" placeholder. What it cannot prove is that the picture is a
        desktop - DWM composites with the GPU, and nothing executes GPU work
        until the host executor and the guest transport land (features 042-044),
        so the primary's pages are still all zeros. The driver's own log carries
        the rest of the proof: the primary's aperture pages were recorded, and no
        frame was refused for want of a backing.
        """
        self.heads = self.args.outputs if self.args.outputs == 1 or self.extend_desktop() else 1
        for head in range(self.heads):
            self.display_head(head)
        driver = self.driver_log()
        if "rvkgpu: aperture map segment 1" not in driver:
            raise StepFailed("display", "the primary's aperture pages were never recorded")
        refused = [line for line in driver.splitlines() if line.startswith("rvkgpu: scanout ")]
        if refused:
            raise StepFailed("display", f"a frame was not carried: {refused[0]}")
        self.say(f"display ok: {self.heads} head(s) of {DISPLAY_SIZE[0]}x{DISPLAY_SIZE[1]} scanned out of "
                 f"the guest's primaries, {self.colours} colour(s)")

    def guest_modes(self):
        """Every mode the driver offers the guest, and the one in force."""
        code, out, error = self.provision("modes")
        if code != 0:
            raise StepFailed("modes", f"exited {code}: {(out + error).decode(errors='replace').strip()[-400:]}")
        lines = out.decode(errors="replace").splitlines()
        offered = [line.split(":", 1)[1].strip() for line in lines if line.startswith("mode:")]
        current = next((line.split(":", 1)[1].strip() for line in lines if line.startswith("current:")), "")
        return offered, current

    def set_guest_mode(self, size):
        """What ChangeDisplaySettings answered: 0 is the switch, anything else
        a refusal (DISP_CHANGE_BADMODE is -2)."""
        width, height = size.split("x")
        code, out, error = self.provision("setmode", ("-Width", width, "-Height", height))
        if code != 0:
            raise StepFailed("setmode", f"exited {code}: {(out + error).decode(errors='replace').strip()[-400:]}")
        answer = next((line.split(":", 1)[1].strip() for line in out.decode(errors="replace").splitlines()
                       if line.startswith("set:")), "")
        if not answer.lstrip("-").isdigit():
            raise StepFailed("setmode", f"the guest said {answer!r} switching to {size}")
        return int(answer)

    def scanout_size(self, name):
        """The host's scanout, once the driver has had a frame to follow a
        commit: the address is programmed from the DPC, not from the switch."""
        time.sleep(SETTLE_SECONDS)
        dump = Screendump(self.work / "screen0.ppm").capture(self.qmp)
        width, height = dump.size()
        if width == 0 or height == 0:
            raise StepFailed(name, f"the screendump is not a PPM: {dump.path}")
        return f"{width}x{height}"

    def mode_switch(self):
        """US2: the modes the device's display holds are offered, a switch to
        one of them moves the host's scanout, and one it does not hold is
        refused with the mode in force left alone."""
        committed = f"{DISPLAY_SIZE[0]}x{DISPLAY_SIZE[1]}"
        offered, current = self.guest_modes()
        missing = [size for size in OFFERED_MODES if size not in offered]
        if missing:
            raise StepFailed("modes", f"the guest is not offered {missing}: {offered}")
        self.say(f"modes ok: {len(offered)} listed, all {len(OFFERED_MODES)} the driver offers among them, "
                 f"{current} in force")
        taken = self.set_guest_mode(SWITCH_MODE) == 0
        expected = SWITCH_MODE if taken else committed
        switched = self.scanout_size("setmode")
        if switched != expected:
            raise StepFailed("setmode", f"the host scans out {switched} after a switch to {SWITCH_MODE} the guest "
                                        f"{'took' if taken else 'refused'}")
        if self.set_guest_mode(UNOFFERED_MODE) == 0:
            raise StepFailed("setmode", f"the guest took {UNOFFERED_MODE}, which the driver never offered")
        kept = self.scanout_size("setmode")
        if kept != expected:
            raise StepFailed("setmode", f"a refused switch left the host scanning out {kept}, not {expected}")
        if taken and self.set_guest_mode(committed) != 0:
            raise StepFailed("setmode", f"the guest refused {committed} going back")
        back = self.scanout_size("setmode")
        if back != committed:
            raise StepFailed("setmode", f"the host scans out {back} after going back to {committed}")
        self.say(f"setmode ok: {SWITCH_MODE} {'taken' if taken else 'refused, and the console session kept'}, "
                 f"{UNOFFERED_MODE} refused, {committed} scanned out")

    def vsync_rate(self):
        """US3: the driver's own vertical blank rate, in hertz, from the fastest
        window it logged. A waiter in the guest cannot see this - the blank is a
        pulse it can miss - so it is read here, where the whole log is. The
        fastest window is the one dxgkrnl left the blanks on through; a window it
        turned them off in measures its power policy, not the driver's period."""
        windows = [(int(blanks), int(ticks)) for blanks, ticks, _enables in VSYNC_LINE.findall(self.driver_log())
                   if int(ticks) != 0]
        if not windows:
            raise StepFailed("vsync", "the driver logged no vertical blank window")
        return max(blanks * TICKS_PER_SECOND / ticks for blanks, ticks in windows)

    def vsync(self):
        rate = self.vsync_rate()
        tolerance = REFRESH_HZ * VSYNC_TOLERANCE_PERCENT / 100.0
        if abs(rate - REFRESH_HZ) > tolerance:
            raise StepFailed("vsync", f"the driver reports {rate:.2f} vertical blanks a second, not "
                                      f"{REFRESH_HZ} within {VSYNC_TOLERANCE_PERCENT}%")
        self.say(f"vsync ok: {rate:.2f} blanks a second")

    def extend_desktop(self):
        """US4: Windows attaches a monitor to every output but leaves the
        desktop on the first until something extends it. Whether this run can is
        what it answers: SetDisplayConfig refuses a caller that is not the
        console session's (ERROR_ACCESS_DENIED), and the agent is a session 0
        service, so a run without an interactive logon asserts the first head
        and says why the others are dark."""
        code, out, error = self.provision("extend")
        if code != 0:
            raise StepFailed("extend", f"exited {code}: {(out + error).decode(errors='replace').strip()[-400:]}")
        answer = next((line.split(":", 1)[1].strip() for line in out.decode(errors="replace").splitlines()
                       if line.startswith("extend:")), "")
        if answer == "0":
            time.sleep(SETTLE_SECONDS)
            self.say(f"extend ok: the desktop is across {self.args.outputs} displays")
            return True
        if answer != str(ACCESS_DENIED):
            raise StepFailed("extend", f"the guest answered {answer!r} extending the desktop")
        self.say("extend ok: the console session's topology is not a session 0 service's to change "
                 f"(error {ACCESS_DENIED}); only the first display is asserted")
        return False

    def display_power(self):
        """US4: the power policy turns the display off and a key turns it back
        on, and the host scans out the same mode again. The system's idle timer
        needs no session, which is what makes this testable where a mode switch
        is not."""
        self.provision_ok("monitortimeout", ("-Minutes", "1"))
        try:
            self.await_dark()
            # A key and a mouse move: whichever the guest's power policy counts
            # as the input that ends its idle.
            self.qmp.execute("input-send-event", {"events": [
                {"type": "key", "data": {"down": True, "key": {"type": "qcode", "data": "shift"}}},
                {"type": "key", "data": {"down": False, "key": {"type": "qcode", "data": "shift"}}}]})
            self.qmp.execute("input-send-event", {"events": [
                {"type": "rel", "data": {"axis": "x", "value": 10}},
                {"type": "rel", "data": {"axis": "y", "value": 10}}]})
            self.await_lit()
        finally:
            self.provision_ok("monitortimeout", ("-Minutes", "0"))
        self.say(f"power ok: the display went dark and came back at {DISPLAY_SIZE[0]}x{DISPLAY_SIZE[1]}")

    def provision_ok(self, name, extra=()):
        code, out, error = self.provision(name, extra)
        if code != 0:
            raise StepFailed(name, f"exited {code}: {(out + error).decode(errors='replace').strip()[-400:]}")

    def await_dark(self):
        """Until the host has no scanout of ours: the driver takes the source off
        the screen when dxgkrnl hides it."""
        for _attempt in range(POWER_ATTEMPTS):
            time.sleep(self.args.power_poll)
            if self.placeholder(0):
                return
        raise StepFailed("power", f"the display never went dark within {POWER_ATTEMPTS * self.args.power_poll:.0f} s "
                                  "of the policy's minute")

    def await_lit(self):
        for _attempt in range(POWER_ATTEMPTS):
            time.sleep(self.args.power_poll)
            if not self.placeholder(0):
                self.display_head(0)
                return
        raise StepFailed("power", "the display never came back")

    def placeholder(self, head):
        """Whether the head shows QEMU's "display output is not active" rather
        than a scanout of ours."""
        dump = Screendump(self.work / f"screen{head}.ppm").capture(self.qmp, head)
        width, height = dump.size()
        if width == 0 or height == 0:
            return True
        pixels = dump.pixels()
        body = pixels[pixels.index(b"255\n") + 4:]
        colours = collections.Counter(body[at:at + 3] for at in range(0, width * height * 3, 3))
        return (width, height) != DISPLAY_SIZE or (len(colours) != 1 and PLACEHOLDER_GREY in colours)

    def display_head(self, head):
        """One head's scanout: the committed size, and our resource rather than
        QEMU's placeholder."""
        dump = Screendump(self.work / f"screen{head}.ppm").capture(self.qmp, head)
        width, height = dump.size()
        if width == 0 or height == 0:
            raise StepFailed("display", f"the screendump is not a PPM: {dump.path}")
        pixels = dump.pixels()
        body = pixels[pixels.index(b"255\n") + 4:]
        if len(body) < width * height * 3:
            raise StepFailed("display", f"the screendump holds {len(body)} bytes for {width}x{height}")
        if (width, height) != DISPLAY_SIZE:
            raise StepFailed("display", f"the host scans out {width}x{height} on head {head}, not "
                                        f"{DISPLAY_SIZE[0]}x{DISPLAY_SIZE[1]}")
        colours = collections.Counter(body[at:at + 3] for at in range(0, width * height * 3, 3))
        if len(colours) != 1 and PLACEHOLDER_GREY in colours:
            raise StepFailed("display", f"head {head} scans out QEMU's placeholder, not the guest's primary")
        self.colours = len(colours)

    def driver_log(self):
        path = self.work / "rvkgpu.log"
        return path.read_text(errors="replace") if path.exists() else ""

    def offers_sleep(self, state):
        """What the guest says it can do, asked before it is asked to do it: a
        suspend the machine does not offer would just stop answering."""
        if self.sleep_states is None:
            self.sleep_states = self.stage_output("sleepstates", saved="sleepstates.txt")
        return f"{state}=yes" in self.sleep_states

    def upgrade(self):
        """US2: a package with a higher version installed over the running one,
        as an update arrives - no reboot, and the adapter running the new
        version afterwards."""
        for step in range(self.args.upgrades):
            output = self.stage_output("upgrade", saved="upgrade.txt", extra=("-Minutes", str(step + 1)))
            installed = UPGRADED.search(output)
            if installed is None:
                raise StepFailed("upgrade", f"the guest did not take the package: {output.strip()[-300:]}")
            self.say(f"upgrade ok: version {installed.group(1)} running, no reboot")
        if self.args.upgrades:
            self.check_monitors(self.stage_output("check", saved="device-after-upgrade.txt"))

    def cycle(self):
        """US2 of feature 040: what pnputil does to an adapter, as often as the
        run asks. The stage disables and enables the device itself - a round
        trip through the agent per cycle would spend the run's time on the
        agent rather than on the driver - and says what Driver Verifier tracked
        after the last one."""
        if not self.args.power_cycles:
            return
        output = self.stage_output("cycle", saved="cycle.txt", extra=("-Minutes", str(self.args.power_cycles)))
        done = CYCLES_DONE.search(output)
        if done is None:
            raise StepFailed("cycle", f"the guest did not finish its cycles: {output.strip()[-300:]}")
        self.check_monitors(self.stage_output("check", saved="device-after-cycles.txt"))
        self.say(f"cycle ok: {done.group(1)} disable and enable cycles, the adapter back each time")

    def suspend(self):
        """US1 of feature 040: S3 through the guest agent, woken with QMP, and
        the adapter still there afterwards. A machine that does not offer S3
        answers the agent with an error, and that is a skip, not a failure."""
        if not self.offers_sleep("standby"):
            self.say("suspend skipped: this machine offers no S3")
            return
        self.qmp.forget_events("SUSPEND")
        self.qmp.forget_events("WAKEUP")
        self.agent.notify("guest-suspend-ram", {})
        # QEMU says when the machine is actually in S3; waking one that has not
        # got there yet wakes nothing.
        self.step("suspend.asleep", lambda: self.qmp.wait_event("SUSPEND", self.args.boot_timeout))
        self.qmp.execute("system_wakeup", {})
        self.step("suspend.wake", lambda: self.qmp.wait_event("WAKEUP", self.args.boot_timeout))
        self.connect_agent("suspend.agent")
        self.check_monitors(self.stage_output("check", saved="device-after-suspend.txt"))
        self.say("suspend ok: the adapter came back from S3")

    def hibernate(self):
        """US1: S4 leaves the machine off, and the overlay boots again on the
        same QEMU configuration - a fresh device, and the guest's own image
        expecting the one it had."""
        if not self.args.hibernate:
            return
        self.provision_ok("hibernate")
        # What the guest said before the stage turned hibernation on is no
        # longer what it would say: ask again.
        self.sleep_states = None
        if not self.offers_sleep("hibernate"):
            self.say("hibernate skipped: this machine offers no S4")
            return
        self.agent.notify("guest-suspend-disk", {})
        self.step("hibernate.off", lambda: self.qemu.wait(timeout=self.args.boot_timeout))
        self.stop_qemu()
        self.boot()
        self.connect_agent("hibernate.resume")
        self.check_monitors(self.stage_output("check", saved="device-after-hibernate.txt"))
        self.say("hibernate ok: the adapter came back from S4")

    def install_and_test(self):
        if self.stage("sign"):
            self.reboot("sign")
        if self.stage("install"):
            self.reboot("install")
        self.check_monitors(self.stage_output("check", saved="device.txt"))
        self.display()
        self.mode_switch()
        self.display_power()
        self.cycle()
        self.upgrade()
        self.suspend()
        self.hibernate()
        self.suite()
        self.vsync()

    def prepare_guest(self):
        if self.stage("prepare"):
            self.reboot("prepare")
            if self.stage("prepare"):
                raise StepFailed("prepare", "still asks for a reboot after rebooting")

    def harvest_panics(self):
        """Names every bugcheck QEMU reported; whether there was one."""
        if self.qmp is None:
            return False
        try:
            self.qmp.drain(EVENT_QUIET)
        except AgentError as error:
            self.say(f"qmp failed: {error}")
            self.qmp_lost = True
        panics = [describe_panic(message.get("data") or {}) for message in self.qmp.take_events("GUEST_PANICKED")]
        for panic in panics:
            self.say(f"panic: {panic}")
        if panics:
            (self.work / "panic.txt").write_text("\n".join(panics) + "\n")
        return bool(panics)

    def reach_guest(self, panicked):
        """An agent to collect through: after a bugcheck, the guest's reboot, or a rescue boot without the device."""
        try:
            if panicked:
                self.step("panic.reset", lambda: self.qmp.wait_event("RESET", self.args.boot_timeout))
            self.connect_agent("collect.agent", None if panicked else self.args.agent_timeout)
            return True
        except StepFailed as failed:
            self.say(f"{failed.step} failed: {failed.reason}")
        if not panicked:
            return False
        self.say("rescue: booting the overlay without virtio-gpu-rvk-pci for its dumps")
        self.stop_qemu()
        try:
            self.boot(rescue=True)
            return True
        except StepFailed as failed:
            self.say(f"{failed.step} failed: {failed.reason}")
            return False

    def collect(self, memory_dump):
        code, out, error = self.provision("collect", ["-MemoryDump"] if memory_dump else [])
        if code != 0:
            raise StepFailed("collect", f"exited {code}: {(out + error).decode(errors='replace').strip()[-400:]}")
        diagnostics = self.work / "diag"
        (diagnostics / "dumps").mkdir(parents=True, exist_ok=True)

        def pull():
            pulled = 0
            for kind, guest_path in MANIFEST.findall(out.decode(errors="replace")):
                name = guest_path.rsplit("\\", 1)[-1]
                self.agent.pull(guest_path, diagnostics / "dumps" / name if kind == "dump" else diagnostics / name)
                pulled += 1
            return pulled

        self.say(f"collect: {self.step('collect.pull', pull)} files in {diagnostics}")
        self.judge_dumps(diagnostics / "dumps")

    def judge_dumps(self, dumps):
        """What the guest's live kernel dumps were for: an expected black screen is
        said out loud, and anything else fails the run (SC-005)."""
        unexpected = []
        black_screens = 0
        for dump in sorted(dumps.glob("*.dmp")):
            # The kernel dump belongs to a bugcheck harvest_panics already named.
            if dump.name.upper() == MEMORY_DUMP:
                continue
            head = dump.read_bytes()[:LIVE_DUMP_CODE + 4]
            if not head.startswith(LIVE_DUMP_HEADER) or len(head) < LIVE_DUMP_CODE + 4:
                unexpected.append(f"{dump.name} is not a kernel dump")
                continue
            code = struct.unpack_from("<I", head, LIVE_DUMP_CODE)[0]
            if code == BLACK_SCREEN_LIVEDUMP:
                black_screens += 1
            else:
                unexpected.append(f"{dump.name} 0x{code:X} {BUGCHECKS.get(code, 'unknown')}")
        if unexpected:
            self.unexpected_dump = ", ".join(unexpected)
            self.say(f"dumps failed: {self.unexpected_dump}")
        elif black_screens:
            self.say(f"dumps ok: {black_screens} live dump(s), every one "
                     f"{BUGCHECKS[BLACK_SCREEN_LIVEDUMP]} - nothing renders into the primary yet")

    def collect_evidence(self, panicked):
        if not self.reach_guest(panicked):
            return
        try:
            self.collect(memory_dump=self.args.memory_dump)
        except StepFailed as failed:
            self.say(f"{failed.step} failed: {failed.reason}")

    def hold(self):
        for channel in (self.agent, self.qmp):
            if channel is not None:
                channel.close()
        self.agent = None
        self.qmp = None
        self.say("hold: the guest is left running")
        print(f"  shell  tools/guest/guest_shell.py --work {self.work} ps 'Get-PnpDevice -Class Display'")
        print(f"  qmp    socat - UNIX-CONNECT:{self.work / 'qmp.sock'}")
        if self.args.gdb is not None:
            print(f"  gdb    target remote 127.0.0.1:{self.args.gdb}")
        if self.args.vnc is not None:
            print(f"  vnc    127.0.0.1:{5900 + self.args.vnc}")
        print(f"  logs   {self.work / 'rvkgpu.log'}, {self.work / 'qemu.log'}")
        print("  press Enter to power the guest off", flush=True)
        try:
            sys.stdin.readline()
        except KeyboardInterrupt:
            pass
        if self.qemu.poll() is None:
            try:
                self.connect_agent("hold.agent", self.args.agent_timeout)
            except StepFailed as failed:
                self.say(f"{failed.step} failed: {failed.reason}")

    def power_off(self):
        if self.agent is not None and self.qemu is not None and self.qemu.poll() is None:
            try:
                self.agent.notify("guest-shutdown", {"mode": "powerdown"})
                self.qemu.wait(timeout=self.args.step_timeout)
                self.say("powerdown ok")
            except (AgentError, subprocess.TimeoutExpired):
                self.say("powerdown timed out: QEMU killed")

    def aftermath(self, failure):
        """Whatever the run did: the bugchecks, the guest's diagnostics, then off - or held for debugging."""
        if self.qemu is None or self.qemu_exited(0):
            return
        panicked = self.harvest_panics()
        if self.qmp_lost and self.qemu_exited(QEMU_EXIT_GRACE):
            return
        paused = panicked and self.args.on_panic == "pause"
        if paused:
            self.say("panic: the guest is paused at the bugcheck")
        elif not self.args.prepare_image:
            self.collect_evidence(panicked)
        if failure is not None and self.args.hold:
            self.hold()
        if not paused:
            self.power_off()

    def qemu_exited(self, timeout):
        """Whether QEMU is gone, reported with the last line it logged - an assertion, usually."""
        try:
            code = self.qemu.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            return False
        how = f"on {signal.Signals(-code).name}" if code < 0 else str(code)
        lines = self.qemu_log.read_text(errors="replace").strip().splitlines()
        self.say(f"qemu exited {how}: {lines[-1] if lines else 'nothing in ' + self.qemu_log.name}")
        return True

    def stop_qemu(self):
        for channel in (self.agent, self.qmp):
            if channel is not None:
                channel.close()
        self.agent = None
        self.qmp = None
        if self.qemu is not None and self.qemu.poll() is None:
            self.qemu.kill()
            self.qemu.wait()
        for socket_name in ("agent.sock", "qmp.sock"):
            (self.work / socket_name).unlink(missing_ok=True)

    def finish(self):
        self.stop_qemu()
        for leftover in ("overlay.qcow2", "vars.fd"):
            (self.work / leftover).unlink(missing_ok=True)

    def run(self):
        self.work.mkdir(parents=True, exist_ok=True)
        watched = not self.args.prepare_image and os.path.isfile(self.args.image)
        before = fingerprint(self.args.image) if watched else None
        failure = None
        try:
            try:
                self.prepare()
                self.boot()
                self.push()
                if self.args.prepare_image:
                    self.prepare_guest()
                else:
                    self.install_and_test()
            except StepFailed as failed:
                failure = failed
                self.say(f"{failed.step} failed: {failed.reason}")
            self.aftermath(failure)
        finally:
            self.finish()
        verdict = 0 if failure is None and self.unexpected_dump is None else 1
        if before is not None and fingerprint(self.args.image) != before:
            self.say("image failed: the base image changed")
            verdict = 1
        (self.work / "verdict.txt").write_text("\n".join(self.log) + "\n")
        return verdict


def main():
    sys.exit(GuestRun(arguments()).run())


if __name__ == "__main__":
    main()
