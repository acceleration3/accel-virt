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
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from fetch_vulkan_runtime import Refused, pins_from, problems_on_disk  # noqa: E402
from guest_agent import AgentError, Qmp, wait_for_agent  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
GUEST_DIRECTORY = "C:\\rvk"
PACKAGE_DIRECTORY = GUEST_DIRECTORY + "\\package"
POWERSHELL = "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"
REBOOT_REQUIRED = 3010
SUMMARY = re.compile(rb"^rvktest: (\d+) passed, (\d+) failed$", re.M)
# The no-op's round trip the transport scenario measures, in microseconds (feature 044).
RVK_LATENCY = re.compile(rb"^ok transport\.latency (\d+)", re.MULTILINE)
# SC-002's regression threshold. The first passing live runs measured 0.87 ms (an
# idle host) and 4.4 ms (a host building beside it); a no-op past 50 ms is a
# transport that has stopped being interrupt-driven, and one near dxgkrnl's 2 s
# TdrDelay is a reset waiting to happen.
RVK_LATENCY_LIMIT_US = 50000
# The guest ends its lines the Windows way, so nothing anchored at $ may
# assume the line stops at the newline.
CYCLES_DONE = re.compile(r"^cycle: (\d+) done\r?$", re.M)
UPGRADED = re.compile(r"^upgrade: installed (\S+) running (\S+) problem 0\r?$", re.M)
STRESS_FENCES = re.compile(rb"^ok stress\.fences (\d+)\r?$", re.M)
STRESS_WORST = re.compile(rb"^ok stress\.worst_wait_us (\d+)\r?$", re.M)
# What Adapter::open_region logs at StartDevice, and the only place a run can
# see the second segment: the list itself never leaves dxgkrnl.
REGION_LINE = re.compile(r"^rvkgpu: host-visible region status 0x([0-9a-fA-F]{8})\r?$", re.M)
HOST_VISIBLE_WRITTEN = re.compile(rb"^ok host_visible\.pattern (\d+)\r?$", re.M)
PROBE_SUMMARY = re.compile(rb"^rvkvkprobe: (\d+) passed, (\d+) failed\r?$", re.M)
PROBE_STEP = re.compile(rb"^ok vulkan\.(\w+)(?: (\S+))?\r?$", re.M)
# Every step of feature 045's proof, in the probe's order: a probe that stops
# printing one has not proven it, whatever its summary counts.
PROBE_STEPS = ("instance", "adapter_luid", "physical_device", "device", "compute", "draw", "lost", "recreated")
# A physical device's section in vulkaninfo's full text output (Vulkan-Tools
# DumpGpu) starts with its name alone on a line. Device groups and presentable
# surfaces name devices too, always with more on the line. --summary prints no
# LUID, so the full output is what is read.
VULKANINFO_DEVICE = re.compile(r"^[ \t]*GPU(\d+):[ \t]*\r?$", re.M)
# DumpVkPhysicalDeviceIDProperties prints deviceLUID only while
# deviceLUIDValid, as the structure's eight bytes in hex.
VULKANINFO_LUID = re.compile(r"deviceLUID[ \t]*[=:][ \t]*\"?([0-9A-Fa-f][0-9A-Fa-f :-]*)")
LUID_HEX_DIGITS = 16
LUID_LOW_PART_BYTES = 4
KHRONOS_DRIVERS = "HKLM\\SOFTWARE\\Khronos\\Vulkan\\Drivers"
REG = "C:\\Windows\\System32\\reg.exe"
CMD = "C:\\Windows\\System32\\cmd.exe"
# reg.exe answers 1 for every failure and says which in English on this image:
# only the one naming a missing key is an absent key.
REG_FAILED = 1
REG_NO_KEY = "unable to find the specified registry key"
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
# What dxgkrnl takes a live dump for when it resets an engine that stopped
# answering: expected only in a run that told the host to stop answering.
TIMEOUT_LIVEDUMPS = (0x117, 0x141)
MEMORY_DUMP = "MEMORY.DMP"


TIMEOUTS = re.compile(r"^timeouts: (\d+)\r?$", re.M)
"""How many event 4101s the guest found: dxgkrnl saying it reset a driver that
stopped responding."""

# The guest's tdr run outlives the stall the host was told to keep, and then
# dxgkrnl's own timeout and reset on top of it.
TDR_PATIENCE = 4.0


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
    parser.add_argument("--host-gpu", default="auto",
                        help="the device's host-gpu property: auto, or a Vulkan device UUID (tools/host/list_gpus.py)")
    parser.add_argument("--driver", default=str(ROOT / "build" / "driver" / "rvkgpu.sys"))
    parser.add_argument("--inf", default=str(ROOT / "src" / "km" / "rvkgpu.inf"))
    parser.add_argument("--test", default=str(ROOT / "build" / "guest" / "rvktest.exe"))
    parser.add_argument("--icd", default=str(ROOT / "build" / "guest" / "rvkvk.dll"),
                        help="the Vulkan ICD the package installs beside rvkgpu.sys (feature 045)")
    parser.add_argument("--icd-manifest", default=str(ROOT / "src" / "um" / "vulkan" / "rvkvk.json"))
    parser.add_argument("--provision", default=str(ROOT / "tools" / "guest" / "provision.ps1"))
    parser.add_argument("--firmware", default="/usr/share/edk2/x64/OVMF_CODE.4m.fd")
    parser.add_argument("--firmware-vars", default="/usr/share/edk2/x64/OVMF_VARS.4m.fd")
    parser.add_argument("--memory", type=int, default=4096, help="MiB")
    parser.add_argument("--cpus", type=int, default=4)
    parser.add_argument("--outputs", type=int, default=1, help="the device's max_outputs")
    parser.add_argument("--require-host-visible", action="store_true",
                        help="fail the run when the host cannot find the guest's pattern in the BAR; off until "
                             "something asks for host memory (feature 043)")
    parser.add_argument("--hostmem", type=int, default=256, metavar="MIB",
                        help="the device's host-visible region, in MiB; 0 gives it none, where the driver "
                             "reports one segment and the same allocation goes to guest memory "
                             "(feature 041, FR-007)")
    parser.add_argument("--power-poll", type=float, default=5.0, help="seconds between display-power screendumps")
    parser.add_argument("--vsync-wait", type=float, default=60.0,
                        help="seconds to wait for the driver to log a window at its own period")
    parser.add_argument("--upgrades", type=int, default=0,
                        help="install this many higher-versioned packages over the running one (feature 040, US2)")
    parser.add_argument("--power-cycles", type=int, default=0,
                        help="disable and enable the adapter this many times (feature 040, US2)")
    parser.add_argument("--stall-ms", type=int, default=0,
                        help="make the test executor take this long per command (feature 040, US3)")
    parser.add_argument("--stall-context", type=int, default=1, metavar="N",
                        help="aim the stall and the fault at the Nth context the device is asked to make, "
                             "so the others keep completing (feature 040, US3)")
    parser.add_argument("--fault-after", type=int, default=0,
                        help="make the test executor lose the device on its Nth command")
    parser.add_argument("--s3", action="store_true",
                        help="let the machine advertise S3; this firmware cannot resume from it")
    parser.add_argument("--hibernate", nargs="?", type=int, const=1, default=0, metavar="N",
                        help="suspend to disk and boot the overlay again this many times, once if the "
                             "option is given no number (feature 040, US1)")
    parser.add_argument("--quick", action="store_true",
                        help="an iteration run: install, check and the suite, without the display, mode, power, "
                             "cycle, upgrade, sleep and hibernate scenarios the gate runs (feature 044)")
    parser.add_argument("--rvk", action="store_true",
                        help="run rvktest.exe's rvk scenarios - real Vulkan, OpenCL and OpenGL work on the host GPU "
                             "- instead of the suite; needs --executor naming the real library (feature 044)")
    parser.add_argument("--vulkan", action="store_true",
                        help="find the Vulkan ICD through the Khronos loader and run rvkvkprobe.exe instead of the "
                             "suite; needs the fetched runtime and --executor naming the real library (feature 045)")
    parser.add_argument("--probe", default=str(ROOT / "build" / "guest" / "rvkvkprobe.exe"))
    parser.add_argument("--vulkan-runtime", default=str(ROOT / "third_party" / "vulkan-runtime"),
                        help="where tools/fetch_vulkan_runtime.py put vulkan-1.dll and vulkaninfo.exe")
    parser.add_argument("--vulkan-pins", default=str(ROOT / "tools" / "vulkan_runtime_pins.txt"))
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
    if parsed.rvk and parsed.vulkan:
        parser.error("--rvk and --vulkan are separate boots: pass one")
    return parsed


def qemu_command(args, work, disk, rescue):
    """QEMU for a run; a rescue boot leaves the device out so the driver never loads."""
    suffix = "-rescue" if rescue else ""
    properties = [f"virtio-gpu-rvk-pci,id={DISPLAY_DEVICE}", f"host-gpu={args.host_gpu}", f"executor={args.executor}",
                  f"max_outputs={args.outputs}", f"vectors={FENCE_VECTORS}"]
    # The region exists only where it is asked for, and the capset's
    # host-visible bit follows the device's own hostmem size, so a run without
    # it is a run against an adapter that must fall back (feature 041, FR-001).
    if args.hostmem:
        properties.append(f"hostmem={args.hostmem}M")
    device = [] if rescue else ["-device", ",".join(properties)]
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


def host_visible_pattern(count):
    """The bytes the guest's host-visible scenario writes through the view
    D3DKMTLock2 gave it (src/guest/suite/guest_pattern.hpp): the byte at
    offset n is 1 + (n * 7) % 251, so none is zero and no page of it repeats
    another - what the region held before the guest wrote cannot be mistaken for
    it. The run looks for them anywhere in the region, because VidMm chooses
    where in the segment an allocation lands and never tells the driver."""
    return bytes(1 + (index * 7) % 251 for index in range(count))


def luid_low_part(printed):
    """The LowPart of a LUID vulkaninfo printed as its eight bytes, which are
    the LUID structure's - LowPart first, little-endian - or None when what was
    printed is not eight bytes."""
    digits = re.sub(r"[ :-]", "", printed)
    if len(digits) != LUID_HEX_DIGITS or re.fullmatch(r"[0-9A-Fa-f]+", digits) is None:
        return None
    return int.from_bytes(bytes.fromhex(digits)[:LUID_LOW_PART_BYTES], "little")


def vulkaninfo_devices(listing):
    """{GPU index: every distinct deviceLUID printed in that device's
    sections}. A device printed twice is one device; a LUID outside any device
    is nobody's."""
    devices = {}
    headers = list(VULKANINFO_DEVICE.finditer(listing))
    for number, header in enumerate(headers):
        end = headers[number + 1].start() if number + 1 < len(headers) else len(listing)
        luids = devices.setdefault(int(header.group(1)), [])
        for luid in VULKANINFO_LUID.findall(listing, header.end(), end):
            if luid.strip() not in luids:
                luids.append(luid.strip())
    return devices


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
# The device's three sources and eight messages its contexts' fence queues
# share, matching rvkgpu.inf's MessageNumberLimit (feature 043).
FENCE_VECTORS = 11
"""The device id screendump names when it asks for a head other than the first."""
HOSTMEM_BAR = 4
"""Where virtio-gpu-pci registers the host-visible region, at offset 0 of the
BAR (hw/display/virtio-gpu-pci.c). tests/qemu/test_virtio_gpu_blobs.py reads the
same region out of the device's virtio shared-memory capability; a run has no
config space of its own to walk, so it asks QEMU where the guest programmed the
BAR instead."""
MEGABYTE = 1 << 20
REFRESH_HZ = 60
"""DisplayMode::refresh_hz: the rate every mode is committed at."""
VSYNC_TOLERANCE_PERCENT = 5
"""SC-003."""
TICKS_PER_SECOND = 10000000
"""The 100 ns units the driver's interrupt time counts in."""
VSYNC_POLL_SECONDS = 1.0
"""Between reads of the driver's log while waiting for a window at its period."""

VSYNC_LINE = re.compile(r"rvkgpu: vsync (\d+) periods in (\d+) ticks, (\d+) reported, enabled (\d+) times")
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
        self.region = None

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
            required += [self.args.driver, self.args.inf, self.args.icd, self.args.icd_manifest, self.args.test]
        if self.args.vulkan:
            required.append(self.args.probe)
        for path in required:
            if not os.path.isfile(path):
                raise StepFailed("prepare", f"{path} does not exist")
        if self.args.vulkan:
            self.vulkan_runtime()
        for stale in ("overlay.qcow2", "vars.fd", "agent.sock", "qmp.sock", "rvkgpu.log", "rvkgpu-rescue.log",
                      "qemu-rescue.log", "panic.txt", "hostmem.bin"):
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

    def vulkan_runtime(self):
        """The loader and vulkaninfo a --vulkan run pushes, as they were pinned:
        a runtime nobody fetched, or one that is not the pinned one, is refused
        before anything boots."""
        try:
            _url, _digest, pinned = pins_from(pathlib.Path(self.args.vulkan_pins))
        except OSError as error:
            raise StepFailed("prepare", f"--vulkan: no pins: {error}")
        except Refused as refused:
            raise StepFailed("prepare", f"--vulkan: {refused}")
        runtime = pathlib.Path(self.args.vulkan_runtime)
        problems = problems_on_disk(runtime, pinned)
        if problems:
            listed = ", ".join(f"{name} {problem}" for name, problem in sorted(problems.items()))
            raise StepFailed("prepare", f"--vulkan needs the pinned Vulkan runtime in {runtime} ({listed}): "
                                        "run tools/fetch_vulkan_runtime.py")

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
        if self.args.stall_ms or self.args.fault_after:
            environment["RVK_ECHO_ONLY_NTH"] = str(self.args.stall_context)
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
            # The INF names the ICD whatever the run is for, pnputil refuses a
            # package missing a file it names, and the sign stage catalogs the
            # package's whole directory (feature 045).
            files += [(self.args.driver, PACKAGE_DIRECTORY + "\\rvkgpu.sys"),
                      (self.args.inf, PACKAGE_DIRECTORY + "\\rvkgpu.inf"),
                      (self.args.icd, PACKAGE_DIRECTORY + "\\rvkvk.dll"),
                      (self.args.icd_manifest, PACKAGE_DIRECTORY + "\\rvkvk.json"),
                      (self.args.test, GUEST_DIRECTORY + "\\rvktest.exe")]
        if self.args.vulkan:
            # The loader beside the programs that import it, where Windows
            # looks first.
            runtime = pathlib.Path(self.args.vulkan_runtime)
            files += [(runtime / "vulkan-1.dll", GUEST_DIRECTORY + "\\vulkan-1.dll"),
                      (runtime / "vulkaninfo.exe", GUEST_DIRECTORY + "\\vulkaninfo.exe"),
                      (self.args.probe, GUEST_DIRECTORY + "\\rvkvkprobe.exe")]

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

    def segments(self):
        """FR-001: what memory the adapter told VidMm it has. The segment list
        goes to dxgkrnl and nowhere else, so the line the driver logs for the
        region it opened at StartDevice is where a run reads it: with the region
        the adapter reports two segments, and without it one."""
        region = REGION_LINE.search(self.driver_log())
        if not self.args.hostmem:
            self.say("segments ok: 1 reported, this device has no region and the same allocation goes to "
                     "guest memory (FR-007)")
            return
        if region is None:
            raise StepFailed("segments", "the driver opened no host-visible region: the capset's host-visible "
                                         "bit is clear, or CapsetExchange failed at StartDevice")
        status = int(region.group(1), 16)
        if status != 0:
            raise StepFailed("segments", f"the driver could not open the host-visible region: status 0x{status:08x}")
        self.region = self.hostmem_bar()
        base, size = self.region
        self.say(f"segments ok: 2 reported, the second {size // MEGABYTE} MiB of host memory at {base:#x}")

    def hostmem_bar(self):
        """Where the guest programmed the device's hostmem BAR, and how much of
        it there is, from QEMU's own view of the bus."""
        try:
            buses = self.qmp.execute("query-pci")
        except AgentError as error:
            raise StepFailed("segments", str(error))
        regions = [region for bus in buses for device in bus.get("devices", [])
                   if device.get("qdev_id") == DISPLAY_DEVICE
                   for region in device.get("regions", []) if region.get("bar") == HOSTMEM_BAR]
        if not regions:
            raise StepFailed("segments", f"QEMU maps no BAR {HOSTMEM_BAR} on the device: the region the driver "
                                         "opened is nowhere in the guest's physical address space")
        return regions[0]["address"], regions[0]["size"]

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
        return out

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

    def host_visible(self, report):
        """SC-003, and the one claim in this feature the guest cannot make for
        itself: the pattern the guest wrote through the view D3DKMTLock2 gave it
        is in the host's own view of the BAR.

        A read-back inside the guest proves only that the view is memory -
        guest RAM passes that just as well, which is exactly the fallback
        FR-007 makes observable. Nothing the guest can do puts bytes where the
        host reads them unless the mapping really is the device's memory, so
        this read, taken from outside the guest, is the only one that cannot be
        faked from inside it.
        """
        written = HOST_VISIBLE_WRITTEN.search(report)
        if written is None:
            self.say("hostvisible skipped: the suite reported no host-visible pattern")
            return
        count = int(written.group(1))
        self.say(f"hostvisible ok: {count} bytes written through the guest's view")
        if self.region is None:
            self.say("hostvisible skipped: with no region on this device the pattern was written into guest "
                     "memory, which proves nothing the guest did not already say")
            return
        base, size = self.region
        if host_visible_pattern(count) in self.read_guest_memory(base, size):
            self.say("hostvisible ok: the host reads the same bytes out of the BAR")
            return
        missing = (f"the host's own view of the {size // MEGABYTE} MiB at {base:#x} does not hold the "
                   f"{count} bytes the guest wrote: the view was not the BAR")
        if self.args.require_host_visible:
            raise StepFailed("hostvisible", missing)
        # The driver reports the segment and maps the region, and asks VidMm for
        # nothing in it: every allocation whose PreferredSegment was that
        # segment came back STATUS_INVALID_PARAMETER on a live guest, so the
        # pattern is in guest memory and this read cannot find it (feature 041,
        # T026). --require-host-visible is what feature 043 turns on once it has
        # a client that asks for the memory.
        self.say(f"hostvisible skipped: {missing}; nothing asks for host memory yet (feature 043)")

    def read_guest_memory(self, address, size):
        """QMP pmemsave: QEMU's own read of a guest physical range, which over
        the hostmem BAR is the host memory the device put behind it."""
        path = self.work / "hostmem.bin"
        path.unlink(missing_ok=True)
        try:
            self.qmp.execute("pmemsave", {"val": address, "size": size, "filename": str(path)})
        except AgentError as error:
            raise StepFailed("hostvisible", str(error))
        if not path.exists() or path.stat().st_size != size:
            raise StepFailed("hostvisible", f"QEMU wrote no {size}-byte read of {address:#x} to {path}")
        image = path.read_bytes()
        # A copy of the whole region is not evidence worth leaving behind.
        path.unlink()
        return image

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
        window it logged, or None while it has logged none. A waiter in the
        guest cannot see this - the blank is a pulse it can miss - so it is read
        here, where the whole log is. The window counts the driver's own timer
        periods, which run whether or not dxgkrnl is asking for blanks; the
        fastest of them is taken all the same, because a window that straddles a
        power transition or a stopped device measures that."""
        windows = [(int(periods), int(ticks))
                   for periods, ticks, _reported, _enables in VSYNC_LINE.findall(self.driver_log())
                   if int(ticks) != 0]
        if not windows:
            return None
        return max(periods * TICKS_PER_SECOND / ticks for periods, ticks in windows)

    def vsync(self):
        """The driver says its rate once every 120 blanks, so the window that
        proves the period may not be written yet when the suite ends - after a
        resume the only windows in the log are the ones dxgkrnl kept the blanks
        off through, and they measure its power policy. Wait for a window at the
        driver's period, and fail with the best one seen if none arrives."""
        deadline = time.monotonic() + self.args.vsync_wait
        tolerance = REFRESH_HZ * VSYNC_TOLERANCE_PERCENT / 100.0
        while True:
            rate = self.vsync_rate()
            if rate is not None and abs(rate - REFRESH_HZ) <= tolerance:
                self.say(f"vsync ok: {rate:.2f} blanks a second")
                return
            if time.monotonic() >= deadline:
                break
            time.sleep(VSYNC_POLL_SECONDS)
        if rate is None:
            raise StepFailed("vsync", "the driver logged no vertical blank window")
        raise StepFailed("vsync", f"the driver reports {rate:.2f} vertical blanks a second, not "
                                  f"{REFRESH_HZ} within {VSYNC_TOLERANCE_PERCENT}%")

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
        for cycle in range(self.args.hibernate):
            self.agent.notify("guest-suspend-disk", {})
            self.step("hibernate.off", lambda: self.qemu.wait(timeout=self.args.boot_timeout))
            self.stop_qemu()
            self.boot()
            self.connect_agent("hibernate.resume")
            self.check_monitors(self.stage_output("check", saved=f"device-after-hibernate{cycle + 1}.txt"))
            # What SC-001 asks is not that the machine comes back but that the
            # driver still works when it does.
            self.suite()
            self.say(f"hibernate ok: the adapter came back from S4, {cycle + 1} of {self.args.hibernate}")

    def injecting(self):
        """Whether this run told the host to stop answering a context, by
        holding its work or by losing it."""
        return bool(self.args.stall_ms or self.args.fault_after)

    def tdr(self):
        """US3 and US4: the host holds one context past dxgkrnl's timeout
        (--stall-ms) or loses it outright (--fault-after) while the guest
        submits on two. dxgkrnl times that node out, resets the engine and logs
        event 4101 - and the claim is that the other context's work still
        completed. Only this runs in such a run: every other scenario would be
        measuring the injection."""
        patience = self.args.step_timeout + self.args.stall_ms / 1000.0 * TDR_PATIENCE
        code, out, error = self.step("tdr", lambda: self.agent.run(GUEST_DIRECTORY + "\\rvktest.exe",
                                                                   ["tdr"], patience))
        (self.work / "tdr.txt").write_bytes(out)
        summary = SUMMARY.search(out)
        if summary is None:
            raise StepFailed("tdr", f"no summary line (exit {code}): {error.decode(errors='replace')[-200:]}")
        failed = int(summary.group(2))
        if failed != 0 or code != 0:
            raise StepFailed("tdr", f"{failed} step(s) failed, exit {code}; see tdr.txt")
        self.say(f"tdr ok: {int(summary.group(1))} passed, the held context lost and the other one's work done")
        self.timeout_events()

    def rvk(self):
        """Feature 044: the rvk scenarios send real streams to the executor the
        device loaded, so which library and which GPU they ran on is part of the
        verdict, and the transport's no-op round trip is its latency."""
        self.say(f"rvk: executor {self.args.executor}, host-gpu {self.args.host_gpu}")
        code, out, error = self.step("rvk", lambda: self.agent.run(GUEST_DIRECTORY + "\\rvktest.exe", ["rvk"],
                                                                   self.args.step_timeout))
        (self.work / "rvk.txt").write_bytes(out)
        summary = SUMMARY.search(out)
        if summary is None:
            raise StepFailed("rvk", f"no summary line (exit {code}): {error.decode(errors='replace')[-200:]}")
        failed = int(summary.group(2))
        if failed != 0 or code != 0:
            raise StepFailed("rvk", f"{failed} step(s) failed, exit {code}; see rvk.txt")
        self.say(f"rvk ok: {int(summary.group(1))} passed")
        latency = RVK_LATENCY.search(out)
        if latency is None:
            return
        microseconds = int(latency.group(1))
        self.say(f"rvk latency: a transport no-op round trip took {microseconds} us")
        if microseconds > RVK_LATENCY_LIMIT_US:
            raise StepFailed("rvk", f"a no-op took {microseconds} us, past the {RVK_LATENCY_LIMIT_US} us threshold")

    def vulkan(self):
        """Feature 045: the Khronos loader finds rvkvk.dll through the adapter's
        VulkanDriverName alone, vulkaninfo lists the one device that gives it,
        and the probe proves a device, its work and its loss through the loader
        on the host GPU the executor runs on."""
        self.say(f"vulkan: executor {self.args.executor}, host-gpu {self.args.host_gpu}")
        self.khronos_key()
        listed = self.vulkaninfo()
        steps = self.probe()
        adapter = self.probed_luid(steps, "adapter_luid")
        device = self.probed_luid(steps, "physical_device")
        if device != adapter:
            raise StepFailed("luid", f"the probe's physical device has LUID LowPart {device}, not the adapter's "
                                     f"{adapter}")
        if listed != adapter:
            raise StepFailed("luid", f"vulkaninfo's physical device has LUID LowPart {listed}, not the adapter's "
                                     f"{adapter}")
        self.say(f"luid ok: the adapter, the probe's physical device and vulkaninfo's all {adapter}")

    def khronos_key(self):
        """FR-001: no global registration. A value there would make the loader
        list the device twice, and would hide whether the adapter's own value
        is what found it."""
        code, out, error = self.step("khronos", lambda: self.agent.run(REG, ["query", KHRONOS_DRIVERS],
                                                                       self.args.step_timeout))
        text = (out + error).decode(errors="replace")
        if code == REG_FAILED and REG_NO_KEY in text:
            self.say(f"khronos ok: no {KHRONOS_DRIVERS} key")
            return
        if code != 0:
            raise StepFailed("khronos", f"reg query exited {code}: {text.strip()[-300:]}")
        registered = [line.strip() for line in text.splitlines() if "rvk" in line.lower()]
        if registered:
            raise StepFailed("khronos", f"the global key lists {registered[0]}: the ICD must be found through "
                                        "the adapter alone")
        self.say("khronos ok: the key lists no rvk driver")

    def vulkaninfo(self):
        """SC-001: the loader, asked by a program that knows nothing of rvk,
        lists exactly one physical device and that device's LUID; its LowPart."""
        # The listing goes to a guest file and is pulled: with the agent capturing
        # its standard output, vulkaninfo on a live guest never exited, although
        # the ICD had destroyed its instance and closed its device.
        listing = GUEST_DIRECTORY + "\\vulkaninfo.txt"
        command = f"{GUEST_DIRECTORY}\\vulkaninfo.exe > {listing} 2>&1"
        code, _out, _error = self.step("vulkaninfo", lambda: self.agent.run(CMD, ["/c", command],
                                                                          self.args.step_timeout))
        self.step("vulkaninfo.pull", lambda: self.agent.pull(listing, self.work / "vulkaninfo.txt"))
        out = (self.work / "vulkaninfo.txt").read_bytes()
        if code != 0:
            raise StepFailed("vulkaninfo", f"exited {code}; see vulkaninfo.txt")
        devices = vulkaninfo_devices(out.decode(errors="replace"))
        if len(devices) != 1:
            named = ", ".join(f"GPU{index} LUID LowPart {' and '.join(str(luid_low_part(luid)) for luid in luids)}"
                              for index, luids in sorted(devices.items()))
            raise StepFailed("vulkaninfo", f"{len(devices)} physical devices listed, not one"
                                           + (f": {named}" if named else ""))
        index, luids = next(iter(devices.items()))
        if not luids:
            raise StepFailed("vulkaninfo", f"GPU{index} has no valid deviceLUID")
        if len(luids) != 1 or luid_low_part(luids[0]) is None:
            raise StepFailed("vulkaninfo", f"GPU{index} has a deviceLUID that is not eight bytes: "
                                           f"{' and '.join(luids)}")
        low_part = luid_low_part(luids[0])
        self.say(f"vulkaninfo ok: one physical device, LUID LowPart {low_part}")
        return low_part

    def probe(self):
        """SC-002 and SC-003: rvkvkprobe.exe's report, every step of it; the
        values the steps printed, by step."""
        code, out, error = self.step("rvkvkprobe", lambda: self.agent.run(GUEST_DIRECTORY + "\\rvkvkprobe.exe", [],
                                                                         self.args.step_timeout))
        (self.work / "vulkan.txt").write_bytes(out)
        summary = PROBE_SUMMARY.search(out)
        if summary is None:
            raise StepFailed("vulkan", f"no summary line (exit {code}): {error.decode(errors='replace')[-200:]}")
        failed = int(summary.group(2))
        if failed != 0 or code != 0:
            raise StepFailed("vulkan", f"{failed} step(s) failed, exit {code}; see vulkan.txt")
        steps = {name.decode(): value.decode() for name, value in PROBE_STEP.findall(out)}
        missing = [name for name in PROBE_STEPS if name not in steps]
        if missing:
            raise StepFailed("vulkan", f"the probe reported no {', '.join(missing)}; see vulkan.txt")
        self.say(f"vulkan ok: {int(summary.group(1))} passed")
        return steps

    def probed_luid(self, steps, name):
        """A LUID LowPart the probe printed, in decimal."""
        if not steps[name].isdigit():
            raise StepFailed("luid", f"the probe printed {name} {steps[name]!r}, not a decimal LowPart")
        return int(steps[name])

    def timeout_events(self):
        """Event 4101 is dxgkrnl saying it reset a driver that stopped
        responding. Without it nothing timed out, and whatever the guest saw was
        about something else."""
        output = self.stage_output("timeouts", saved="timeouts.txt")
        counted = TIMEOUTS.search(output)
        if counted is None or int(counted.group(1)) == 0:
            raise StepFailed("tdr", f"the System log holds no event 4101: {output.strip()[-300:]}")
        self.say(f"tdr ok: {counted.group(1)} recovery event(s) 4101 in the System log")

    def install_and_test(self):
        if self.stage("sign"):
            self.reboot("sign")
        if self.stage("install"):
            self.reboot("install")
        self.check_monitors(self.stage_output("check", saved="device.txt"))
        self.segments()
        # The display and power scenarios are the echo run's; these boots are
        # for the work the real executor does (features 044 and 045).
        if self.args.vulkan:
            self.vulkan()
            return
        if self.args.rvk:
            self.rvk()
            return
        if self.args.quick:
            self.say("quick: the display, power, cycle, upgrade, sleep and hibernate scenarios are left to the gate")
        else:
            self.whole_machine()
        if self.injecting():
            self.tdr()
        else:
            self.host_visible(self.suite())
            self.vsync()

    def whole_machine(self):
        """What only a whole guest session answers, and what makes a run long:
        the display, its modes and power, the adapter cycled and upgraded, and
        the machine slept and hibernated."""
        self.display()
        self.mode_switch()
        self.display_power()
        self.cycle()
        self.upgrade()
        self.suspend()
        self.hibernate()

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

    def expected_dumps(self):
        """The live dumps this run is allowed to leave behind, and why each one
        is not this driver's failure."""
        expected = {BLACK_SCREEN_LIVEDUMP: "nothing renders into the primary yet"}
        if self.injecting():
            for code in TIMEOUT_LIVEDUMPS:
                expected[code] = "the host was told to stop answering one context"
        return expected

    def judge_dumps(self, dumps):
        """What the guest's live kernel dumps were for: one this run expected is
        said out loud, and anything else fails the run (SC-005)."""
        expected = self.expected_dumps()
        unexpected = []
        seen = collections.Counter()
        for dump in sorted(dumps.glob("*.dmp")):
            # The kernel dump belongs to a bugcheck harvest_panics already named.
            if dump.name.upper() == MEMORY_DUMP:
                continue
            head = dump.read_bytes()[:LIVE_DUMP_CODE + 4]
            if not head.startswith(LIVE_DUMP_HEADER) or len(head) < LIVE_DUMP_CODE + 4:
                unexpected.append(f"{dump.name} is not a kernel dump")
                continue
            code = struct.unpack_from("<I", head, LIVE_DUMP_CODE)[0]
            if code in expected:
                seen[code] += 1
            else:
                unexpected.append(f"{dump.name} 0x{code:X} {BUGCHECKS.get(code, 'unknown')}")
        if unexpected:
            self.unexpected_dump = ", ".join(unexpected)
            self.say(f"dumps failed: {self.unexpected_dump}")
            return
        for code, count in sorted(seen.items()):
            self.say(f"dumps ok: {count} live dump(s), every one {BUGCHECKS[code]} - {expected[code]}")

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
