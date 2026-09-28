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
import select
import signal
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from cts_results import CtsRatchet, QpaResults, SuiteComparison  # noqa: E402
from recording_cost import RecordingCost  # noqa: E402
from submit_ratchet import SubmitRatchet  # noqa: E402
from cts_shards import CtsShards  # noqa: E402
from cts_subset import CtsSubset  # noqa: E402
from fetch_cts import CtsFetch, CtsPins  # noqa: E402
from frame_reference import FrameReference  # noqa: E402
from fetch_layers import LayerFetch, LayerPins, Refused as LayerRefused  # noqa: E402
from fetch_vulkan_runtime import Refused, pins_from, problems_on_disk  # noqa: E402
from fetch_opencl_loader import Refused as ClRefused, pins_from as cl_pins_from  # noqa: E402
from fetch_opencl_loader import problems_on_disk as cl_problems_on_disk  # noqa: E402
from guest_agent import AgentError, AgentRelay, Qmp, wait_for_agent  # noqa: E402
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "host"))
import list_gpus  # noqa: E402

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
# SC-002's own measurement (feature 065): the same no-op, 31 warm rounds after
# the first, whose median is what a recording's cost is taken from. The first
# round trip is cold and swings by milliseconds on this host, which is orders
# of magnitude more than the one write a record costs.
RVK_WARM_MEDIAN = re.compile(rb"^ok transport\.warm_median (\d+)", re.MULTILINE)
# The guest ends its lines the Windows way, so nothing anchored at $ may
# assume the line stops at the newline.
CYCLES_DONE = re.compile(r"^cycle: (\d+) done\r?$", re.M)
UPGRADED = re.compile(r"^upgrade: installed (\S+) running (\S+) problem 0\r?$", re.M)
# What the doorbell bought, as the in-guest benchmark reports it (feature 049,
# US3): both per-submission costs and the ratio the record holds.
SUBMIT_DOORBELL_NS = re.compile(rb"^ok submit\.doorbell_ns (\d+)\r?$", re.M)
SUBMIT_KERNEL_NS = re.compile(rb"^ok submit\.kernel_ns (\d+)\r?$", re.M)
SUBMIT_RATIO = re.compile(rb"^ok submit\.ratio (\d+)\r?$", re.M)
SUBMIT_DOORBELL = re.compile(rb"^note submit\.doorbell (\w+)\r?$", re.M)
STRESS_FENCES = re.compile(rb"^ok stress\.fences (\d+)\r?$", re.M)
STRESS_WORST = re.compile(rb"^ok stress\.worst_wait_us (\d+)\r?$", re.M)
# What the adapter logs when it sets up the host window for a device
# (docs/host-window.md): the only place a run sees whether it has one.
WINDOW_LINE = re.compile(r"^rvkgpu: host window status 0x([0-9a-fA-F]{8})\r?$", re.M)
PROBE_SUMMARY = re.compile(rb"^rvkvkprobe: (\d+) passed, (\d+) failed\r?$", re.M)
# rvkglprobe --discovery's facts (feature 050): `discovery <key> <text> value=0x.. error=..`.
DISCOVERY_LINE = re.compile(r"^discovery (\w+) (.*?)\r?$", re.M)
DISCOVERY_OPEN = re.compile(r"^discovery open_adapter_from_hdc (\w+) value=0x([0-9A-Fa-f]+) error=(\d+)\r?$", re.M)
PROBE_STEP = re.compile(rb"^ok vulkan\.(\w+)(?: (\S+))?\r?$", re.M)
# Every step of feature 045's proof, in the probe's order: a probe that stops
# printing one has not proven it, whatever its summary counts.
# The window-system steps are feature 046's: a surface on the probe's own
# window, the frames it presented read back off that window, the pacing they
# were held to, and the window resized under the swapchain.
PROBE_STEPS = ("instance", "adapter_luid", "physical_device", "device", "compute", "draw", "lost", "recreated",
               "surface", "present", "pacing", "resized")
# Feature 050's probe through the system opengl32.dll: the summary, each step,
# and the strings the context answered, which the probe prints as notes.
GL_SUMMARY = re.compile(r"^rvkglprobe: (\d+) passed, (\d+) failed\r?$", re.M)
GL_STEP = re.compile(r"^ok opengl\.(\w+)(?: (\S+))?\r?$", re.M)
GL_NOTE = re.compile(r"^note opengl\.(\w+) (.*?)\r?$", re.M)
GL_STEPS = ("pixel_format", "context", "version", "draw", "share", "busy", "compat", "interval", "resized",
            "attribs", "lost", "recreated")
# Feature 052's probe through the Khronos OpenCL loader: the summary, each step,
# and the device the platform answered with, which the probe prints as a note.
CL_SUMMARY = re.compile(r"^rvkclprobe: (\d+) passed, (\d+) failed\r?$", re.M)
CL_STEP = re.compile(r"^ok opencl\.(\w+)(?: (\S+))?\r?$", re.M)
CL_NOTE = re.compile(r"^note opencl\.(\w+) (.*?)\r?$", re.M)
# The probe's own order, from the platform the loader listed to the objects it
# released: a probe that stops printing one has not proven it, whatever its
# summary counts.
CL_STEPS = ("platform_count", "platforms", "platform", "device_ids", "device", "luid", "context", "queue",
            "buffers", "write", "write_second", "program", "build", "kernel", "arguments", "run", "finish",
            "read", "sum", "map", "unmap", "svm", "user_event", "wait", "callback", "release")
# Renderers that are not a GPU: Mesa's software rasterizers, what opengl32.dll
# answers itself when no ICD took the window, and what an OpenCL device that is
# the CPU calls itself.
SOFTWARE_RENDERERS = ("llvmpipe", "softpipe", "swiftshader", "gdi generic")
# The renders each probe judged, which it leaves beside itself as bitmaps for a
# person to open (docs/testing.md); pulled into <work>/images.
VULKAN_PICTURES = ("vulkan-draw", "vulkan-present", "vulkan-resized")
GL_PICTURES = ("opengl-draw", "opengl-share", "opengl-recreated", "opengl-compat")
# Feature 047's three D3D probes, each through the layer beside it: DXVK's
# d3d9.dll and d3d11.dll, vkd3d-proton's d3d12.dll. One summary and one step
# list each, and the steps are the probe's own order - a probe that stops
# printing one has not proven it, whatever its summary counts.
D3D_SUMMARY = re.compile(rb"^rvkd3d(9|11|12)probe: (\d+) passed, (\d+) failed\r?$", re.M)
D3D_STEP = re.compile(rb"^ok d3d(9|11|12)\.(\w+)(?: (\S+))?\r?$", re.M)
D3D_STEPS = {"9": ("device", "adapter_ids", "clear", "readback", "pixels", "present"),
             "11": ("device", "feature_level", "adapter_luid", "target", "readback", "pixels", "present"),
             "12": ("device", "feature_level", "adapter_luid", "queue", "target", "clear", "fence", "readback",
                    "pixels")}
# Which layer each probe's DLLs come from, and what the probe is called.
D3D_LAYERS = {"9": "dxvk", "11": "dxvk", "12": "vkd3d-proton"}
D3D_PICTURES = ("d3d9-clear", "d3d11-clear", "d3d12-clear")
# What provision.ps1's heapchecks stage counts: page heap's own verdict on the
# three probes (feature 047, FR-006).
HEAP_CHECKS = re.compile(r"^heapchecks: (\d+)\r?$", re.M)
HEAP_EVENT = re.compile(r"^heapcheck: (\d+) (.*?)\r?$", re.M)
# Where the CTS and its logs live in the guest, and what a case list is called
# there (feature 048, US1).
CTS_DIRECTORY = GUEST_DIRECTORY + "\\cts"
CTS_CASES = CTS_DIRECTORY + "\\cases.txt"
# Where the frames an application run is judged against live (feature 048,
# US2). A reference is the device's scanout, committed, so what a run compares
# is what a monitor would have shown.
REFERENCES = ROOT / "specs" / "048-vulkan-conformance" / "references"
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
SCHTASKS = "C:\\Windows\\System32\\schtasks.exe"
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
# What an --all-gpus run tells provision.ps1 each host GPU is called, one
# "<vendor>\t<device>\t<name>" line each, hex in lowercase.
ADAPTER_NAMES = GUEST_DIRECTORY + "\\adapters.txt"
# virtio-gpu-rvk-pci's subsystem, 5256 by 1af4: DXGI's SubSysId, and the part of
# a PNPDeviceID the device always keeps.
RVK_SUBSYSTEM = 0x52561AF4


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
    parser.add_argument("--renderer", default="",
                        help="run each device's contexts in this rvk-renderer (docs/renderer.md); the executor "
                             "is loaded there instead of in QEMU")
    parser.add_argument("--host-gpu", default="auto",
                        help="the device's host-gpu property: auto, or a Vulkan device UUID (tools/host/list_gpus.py)")
    parser.add_argument("--all-gpus", action="store_true",
                        help="one virtio-gpu-rvk-pci per host GPU (tools/host/list_gpus.py, CPU devices left "
                             "out), and a check that Device Manager, WMI and DXGI name each with the host's "
                             "name and PCI IDs")
    parser.add_argument("--vulkaninfo", metavar="FILE",
                        help="--all-gpus: read the host's GPUs from captured vulkaninfo output instead of "
                             "running vulkaninfo")
    parser.add_argument("--sysfs", default=list_gpus.SYSFS_PCI, help=argparse.SUPPRESS)
    parser.add_argument("--driver", default=str(ROOT / "build" / "driver" / "rvkgpu.sys"))
    parser.add_argument("--d3d-runtime", default=str(ROOT / "build-d3d" / "lib"),
                        help="the directory the executor loads DXVK and vkd3d-proton from (RVK_D3D_PATH)")
    parser.add_argument("--inf", default=str(ROOT / "src" / "km" / "rvkgpu.inf"))
    parser.add_argument("--test", default=str(ROOT / "build" / "guest" / "rvktest.exe"))
    parser.add_argument("--inspect", default=str(ROOT / "build" / "guest" / "rvkinspect.exe"),
                        help="the trace inspector (docs/trace.md), pushed to every run so tools/mcp/rvk_inspect.py "
                             "can read a hung or crashed program")
    parser.add_argument("--icd", default=str(ROOT / "build" / "guest" / "rvkvk.dll"),
                        help="the Vulkan ICD the package installs beside rvkgpu.sys (feature 045)")
    parser.add_argument("--icd-manifest", default=str(ROOT / "src" / "um" / "vulkan" / "rvkvk.json"))
    parser.add_argument("--gl-icd", default=str(ROOT / "build" / "guest" / "rvkgl.dll"),
                        help="the OpenGL ICD the package names (feature 050)")
    parser.add_argument("--cl-icd", default=str(ROOT / "build" / "guest" / "rvkcl.dll"),
                        help="the OpenCL ICD the package names (feature 052)")
    parser.add_argument("--umd", default=str(ROOT / "build" / "guest" / "rvkumd.dll"),
                        help="the Direct3D user-mode driver the package names (feature 055)")
    parser.add_argument("--provision", default=str(ROOT / "tools" / "guest" / "provision.ps1"))
    parser.add_argument("--firmware", default="/usr/share/edk2/x64/OVMF_CODE.4m.fd")
    parser.add_argument("--firmware-vars", default="/usr/share/edk2/x64/OVMF_VARS.4m.fd")
    parser.add_argument("--memory", type=int, default=4096, help="MiB")
    parser.add_argument("--cpus", type=int, default=4)
    parser.add_argument("--outputs", type=int, default=1, help="the device's max_outputs")
    parser.add_argument("--record", action="store_true",
                        help="write a wire recording to recording.rvk in the work directory, which "
                             "build/rvkrecord prints and replays (feature 065)")
    parser.add_argument("--record-limit", type=int, default=256 * 1024 * 1024, metavar="BYTES",
                        help="the recording's ceiling; it stops there rather than growing past it")
    parser.add_argument("--hostmem", type=int, default=8192, metavar="MIB",
                        help="the device's host window, in MiB; 0 gives it none, and every chunk a "
                             "process asks for is guest memory (docs/host-window.md)")
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
    parser.add_argument("--opengl-discovery", action="store_true",
                        help="run rvkglprobe.exe --discovery through the system opengl32.dll and record what it "
                             "loaded, instead of the suite (feature 050, T008)")
    parser.add_argument("--opengl", action="store_true",
                        help="run rvkglprobe.exe's steps through the system opengl32.dll on the host GPU instead of "
                             "the suite; needs --executor naming the real library (feature 050, T043)")
    parser.add_argument("--gl-probe", default=str(ROOT / "build" / "guest" / "rvkglprobe.exe"))
    parser.add_argument("--opencl", action="store_true",
                        help="run rvkclprobe.exe's steps through the Khronos OpenCL loader on the host GPU "
                             "instead of the suite; needs the fetched loader and --executor naming the real "
                             "library (feature 052)")
    parser.add_argument("--cl-probe", default=str(ROOT / "build" / "guest" / "rvkclprobe.exe"))
    parser.add_argument("--opencl-loader", default=str(ROOT / "third_party" / "opencl-loader"),
                        help="where tools/fetch_opencl_loader.py put the loader's source")
    parser.add_argument("--opencl-pins", default=str(ROOT / "tools" / "opencl_loader_pins.txt"))
    parser.add_argument("--d3d", action="store_true",
                        help="run the three D3D probes through the pinned DXVK and vkd3d-proton beside them, on "
                             "the host GPU, instead of the suite; needs the fetched layers, the fetched Vulkan "
                             "runtime and --executor naming the real library (feature 047)")
    parser.add_argument("--cts", action="store_true",
                        help="run the pinned Vulkan CTS subset in the guest on the host GPU instead of the "
                             "suite, shard by shard; needs the fetched CTS and --executor naming the real "
                             "library (feature 048)")
    parser.add_argument("--cts-dir", default=str(ROOT / "third_party" / "vulkan-cts"),
                        help="where tools/fetch_cts.py put the CTS build")
    parser.add_argument("--cts-pins", default=str(ROOT / "tools" / "cts_pins.txt"))
    parser.add_argument("--cts-subset", default=str(ROOT / "tools" / "cts_subset.txt"),
                        help="the patterns whose cases this runs (tools/check_cts_subset.py holds them to "
                             "what the ICD reports)")
    parser.add_argument("--cts-shard", type=int, default=200,
                        help="cases per guest step: a shard is one step, so a shard that runs past "
                             "--step-timeout fails the run rather than reporting")
    parser.add_argument("--cts-restarts", type=int, default=2,
                        help="how many times a shard that died in a case is picked up after it")
    parser.add_argument("--cts-host", default=None,
                        help="a .qpa of the same subset run natively on the host GPU; the verdict is the "
                             "comparison with it (FR-002)")
    parser.add_argument("--cts-ratchet", default=str(ROOT / "tools" / "cts_ratchet.txt"))
    parser.add_argument("--submit-ratchet", default=str(ROOT / "tools" / "submit_ratchet.txt"))
    parser.add_argument("--frames", action="store_true",
                        help="capture the device's scanout after each application run and compare it with the "
                             "reference committed for it (feature 048, US2 and FR-004)")
    parser.add_argument("--frame-tolerance", type=int, default=2,
                        help="the per-channel step a GPU may round a captured frame by")
    parser.add_argument("--references", default=str(REFERENCES),
                        help="where the committed reference frames are")
    parser.add_argument("--page-heap", action="store_true",
                        help="run the D3D probes with full page heap on each of them, and report what Windows "
                             "logged for it either way (feature 047, FR-006); needs --d3d")
    parser.add_argument("--d3d9-probe", default=str(ROOT / "build" / "guest" / "rvkd3d9probe.exe"))
    parser.add_argument("--d3d11-probe", default=str(ROOT / "build" / "guest" / "rvkd3d11probe.exe"))
    parser.add_argument("--d3d12-probe", default=str(ROOT / "build" / "guest" / "rvkd3d12probe.exe"))
    parser.add_argument("--layers", default=str(ROOT / "third_party"),
                        help="where tools/fetch_layers.py put dxvk/ and vkd3d-proton/")
    parser.add_argument("--layer-pins", default=str(ROOT / "tools"),
                        help="where the layers' pins files are: <layer>_pins.txt each")
    parser.add_argument("--vulkan-runtime", default=str(ROOT / "third_party" / "vulkan-runtime"),
                        help="where tools/fetch_vulkan_runtime.py put vulkan-1.dll and vulkaninfo.exe")
    parser.add_argument("--vulkan-pins", default=str(ROOT / "tools" / "vulkan_runtime_pins.txt"))
    parser.add_argument("--stress-minutes", type=float, default=0.0,
                        help="minutes of fences awaited while the blank's interrupt is masked and unmasked "
                             "(feature 039, US3); 0 leaves the long run out")
    parser.add_argument("--accel", default="kvm")
    parser.add_argument("--no-cpu-pm", dest="cpu_pm", action="store_false",
                        help="let an idle vCPU exit to the host (QEMU's default) instead of -overcommit cpu-pm=on")
    parser.add_argument("--boot-timeout", type=float, default=900.0)
    parser.add_argument("--step-timeout", type=float, default=600.0)
    parser.add_argument("--agent-timeout", type=float, default=30.0)
    parser.add_argument("--on-panic", choices=("reboot", "pause"), default="reboot",
                        help="reboot: let the guest write its dump and reboot (default); "
                             "pause: stop the guest at the bugcheck for --gdb")
    parser.add_argument("--hold", action="store_true",
                        help="on failure, keep the guest running until Enter is pressed")
    parser.add_argument("--no-collect", action="store_true",
                        help="do not pull the guest's diagnostics before holding it: a guest kept for interactive "
                             "work is handed over the moment its steps are done")
    parser.add_argument("--keep", action="store_true",
                        help="keep the guest running until Enter is pressed whatever the verdict, to inspect it "
                             "(tools/mcp/rvk_inspect.py)")
    parser.add_argument("--gdb", type=int, metavar="PORT", help="QEMU's gdbstub on 127.0.0.1:PORT")
    parser.add_argument("--vnc", type=int, metavar="DISPLAY", help="the guest's display on 127.0.0.1:5900+DISPLAY")
    # A console's GL context comes from the display backend and never from the
    # device, so -display none is a host on which the device offers no dma-buf
    # scanout at all and the guest keeps the 2D path (feature 058,
    # docs/kb/06-display.md). egl-headless is the backend that gives one and
    # still answers screendump: it imports the dma-buf, blits it and reads it
    # back into the console's surface.
    parser.add_argument("--display", default="none", choices=("none", "egl-headless", "gtk", "sdl"),
                        help="the QEMU display backend (default: none). egl-headless gives the console a GL "
                             "context, which is what lets the device scan out a dma-buf; gtk and sdl open a "
                             "window on the host desktop, and closing it powers the guest off")
    parser.add_argument("--relative-mouse", action="store_true",
                        help="with gtk or sdl, a relative mouse the window grabs on a click (Ctrl+Alt+G lets go) "
                             "instead of the absolute tablet, and on gtk the pointer kept visible while grabbed: "
                             "what a game that locks the mouse for camera look needs")
    parser.add_argument("--net", action="store_true",
                        help="the guest on the internet through QEMU's user-mode NAT, on an e1000e NIC, which "
                             "Windows drives with its inbox driver (default: no network)")
    parser.add_argument("--audio", action="store_true",
                        help="an Intel HD Audio controller and codec playing to the host's PipeWire, which Windows "
                             "drives with its inbox driver (default: no sound)")
    parser.add_argument("--perfetto", action="store_true",
                        help="record every host scope as a timeline event (RVK_PERFETTO=1); "
                             "tools/perfetto_trace.py <qemu pid> writes it for ui.perfetto.dev")
    parser.add_argument("--trace", action="append", default=[], metavar="PATTERN",
                        help="enable QEMU trace events matching PATTERN into qemu.log, e.g. 'virtio_gpu_*'")
    parser.add_argument("--no-verifier", action="store_true",
                        help="install the driver without Driver Verifier. The verifier interposes on every "
                             "DDI call, so a run measuring what it changes has to be able to take it away; "
                             "refused with --power-cycles, whose pool-leak check is a verifier reading")
    parsed = parser.parse_args()
    if not parsed.image:
        parser.error("no image: pass --image or set AV_GUEST_IMAGE")
    if parsed.opengl_discovery and (parsed.rvk or parsed.vulkan or parsed.opengl):
        parser.error("--opengl-discovery is its own boot: pass it without --rvk, --vulkan and --opengl")
    if parsed.all_gpus and parsed.host_gpu != "auto":
        parser.error("--all-gpus binds a device to every host GPU: pass it without --host-gpu")
    if parsed.no_verifier and parsed.power_cycles:
        parser.error("--no-verifier leaves nothing to read: --power-cycles judges the pool allocations "
                     "Driver Verifier tracked, and without it every cycle reports none")
    return parsed


def display_option(args):
    """The display backend and its options. A window draws with GL, as gtk
    must to scale the guest's frame on the GPU. Grabbing a relative mouse, gtk
    hides the host's pointer and shows the guest's only when the guest next
    changes its shape, which Windows does rarely: show-cursor keeps one in
    view. gtk has no pointer lock on Wayland, though - it warps the host's
    pointer after the guest's, and XWayland answers each warp with movement -
    so a relative mouse on a Wayland host is SDL's, whose relative mode locks
    the pointer (SDL_VIDEODRIVER=wayland)."""
    if args.display == "gtk":
        return "gtk,gl=on,show-cursor=on" if args.relative_mouse else "gtk,gl=on"
    if args.display == "sdl":
        return "sdl,gl=on"
    return args.display


def qemu_command(args, work, disk, rescue, gpus=()):
    """QEMU for a run; a rescue boot leaves the device out so the driver never
    loads. With `gpus`, one device per host GPU, each bound by its UUID; the
    first keeps the id screendumps and the hostmem query name."""
    suffix = "-rescue" if rescue else ""
    device = []
    for index, host_gpu in enumerate([gpu.uuid for gpu in gpus] or [args.host_gpu]):
        name = DISPLAY_DEVICE if index == 0 else f"{DISPLAY_DEVICE}{index}"
        properties = [f"virtio-gpu-rvk-pci,id={name}", f"host-gpu={host_gpu}", f"executor={args.executor}",
                      f"max_outputs={args.outputs}", f"vectors={FENCE_VECTORS}"]
        # The region exists only where it is asked for, and the capset's
        # host-visible bit follows the device's own hostmem size, so a run without
        # it is a run against an adapter that must fall back (feature 041, FR-001).
        if args.hostmem:
            properties.append(f"hostmem={args.hostmem}M")
        if args.renderer:
            properties.append(f"renderer={args.renderer}")
        # A desktop someone uses is on the first GPU; the rest render and drive
        # no monitor, as a hybrid machine's second GPU does, so the desktop's
        # GPU is DXGI's first adapter and no window is composed across GPUs.
        if index > 0 and args.display in ("gtk", "sdl"):
            properties.append("display=off")
        # What crossed the wire, written beside qemu.log so a run that rendered
        # wrong can be printed and replayed afterwards (feature 065, FR-001).
        # One file, the first device's: two devices cannot write one recording.
        if args.record and not rescue and index == 0:
            properties.append(f"record={work / RECORDING}")
            properties.append(f"record-limit={args.record_limit}")
        if not rescue:
            device += ["-device", ",".join(properties)]
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
        # Without GL, gtk scales the guest's frame to a HiDPI window in
        # software - bilinear, in cairo, on QEMU's main loop, on every redraw -
        # which the device's work shares; with it the GPU draws the window.
        "-vga", "none", "-display", display_option(args),
        "-nic", "user,model=e1000e" if args.net else "none",
    ]
    if args.audio:
        command += ["-audiodev", "pipewire,id=audio", "-device", "ich9-intel-hda",
                    "-device", "hda-output,audiodev=audio"]
    # An idle vCPU halts inside the guest instead of exiting to the host: a
    # completion's interrupt then reaches a vCPU still on its core, not a host
    # thread the scheduler must wake first. A guest that waits on every frame
    # pays that wake on each one - the Direct3D 12 samples ran 11 to 23% faster
    # without it. The vCPU threads hold their cores while the guest idles.
    if args.cpu_pm and args.accel == "kvm":
        command += ["-overcommit", "cpu-pm=on"]
    if args.vnc is not None:
        command += ["-vnc", f"127.0.0.1:{args.vnc}"]
    # A window someone uses: an absolute pointer, so the guest's cursor is
    # where the host's is rather than a grabbed relative mouse - unless a game
    # asked for one. A game that locks the mouse reads raw movement, and an
    # absolute pointer's is a jump to where the host's cursor is, which swings
    # its camera far past what the hand moved.
    if args.display in ("gtk", "sdl"):
        command += ["-device", "qemu-xhci"] + ([] if args.relative_mouse else ["-device", "usb-tablet"])
    if args.gdb is not None:
        command += ["-gdb", f"tcp:127.0.0.1:{args.gdb}"]
    for pattern in args.trace:
        command += ["-trace", f"enable={pattern}"]
    return command


def fingerprint(path):
    """Size and modification time: hashing a guest image would take minutes."""
    status = os.stat(path)
    return status.st_size, status.st_mtime_ns


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
RECORDING = "recording.rvk"
"""What --record writes in the work directory, beside qemu.log (feature 065)."""
RECORDING_COST = "recording-cost.txt"
"""Where the rvk scenario keeps each side of SC-002's comparison. It outlives a
run, because the two sides are two boots: one with --record and one without."""
# The device's three sources and eight messages its contexts' fence queues
# share, matching rvkgpu.inf's MessageNumberLimit (feature 043).
FENCE_VECTORS = 11
"""The device id screendump names when it asks for a head other than the first."""
HOSTMEM_BAR = 4
"""Where virtio-gpu-pci registers the host window, at offset 0 of the
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
# How long a display step waits for a console to have a surface at all. A
# blob scanout gives one only at the driver's first flip, which is a DPC
# after the mode commit rather than part of it (feature 058).
SCANOUT_WAIT_SECONDS = 30.0
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

    def capture(self, qmp, head=0, wait=0.0):
        """Writes the dump of `head`; raises StepFailed when QEMU refuses or
        writes nothing.

        `wait` seconds of "no surface" are taken as "not yet". A console
        showing a dma-buf has no surface until the guest's first flip puts one
        there - stock 2D scanout makes one at the mode commit, a blob scanout
        only when the driver's DPC has an address to name (feature 058).
        """
        arguments = {"filename": str(self.path)}
        if head != 0:
            arguments.update({"device": DISPLAY_DEVICE, "head": head})
        deadline = time.monotonic() + wait
        while True:
            try:
                qmp.execute("screendump", arguments)
                break
            except AgentError as error:
                if "no surface" not in str(error) or time.monotonic() >= deadline:
                    raise StepFailed("screendump", str(error))
                time.sleep(0.5)
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


class ConsoleProgram:
    """A program run in the session a person is logged into.

    The guest agent is a service, so everything it starts runs on the window
    station Service-0x0-3e7$, where a window never becomes visible - feature 046
    measured it: ShowWindow leaves IsWindowVisible false, and BitBlt of such a
    window's device context fails with ERROR_INVALID_HANDLE. A frame presented
    onto a window and read back off it therefore has to be made in the console
    session, which a scheduled task with /it runs in. The task names a .cmd
    file rather than a command line, because schtasks parses /tr itself and a
    redirection in it is an invalid option.
    """

    task = "rvkconsole"
    """Created with /f and deleted after each run, so a stale one is replaced."""
    script = GUEST_DIRECTORY + "\\console.cmd"
    poll_seconds = 2.0

    def __init__(self, agent, timeout):
        self.agent = agent
        self.timeout = timeout

    def console_user(self):
        """Who is logged into the console session; a run needs one, since a
        signed-out machine has no session to present in."""
        code, out, error = self.agent.run(
            POWERSHELL, ["-NoProfile", "-Command", "(Get-CimInstance Win32_ComputerSystem).UserName"], self.timeout)
        user = out.decode(errors="replace").strip()
        if code != 0 or not user:
            raise StepFailed("console", "no one is logged into the console session: "
                                        f"exit {code}, {error.decode(errors='replace').strip()[-200:]}")
        return user

    def schtasks(self, arguments, step):
        code, out, error = self.agent.run(SCHTASKS, arguments, self.timeout)
        if code != 0:
            raise StepFailed(step, f"schtasks {arguments[0]} exited {code}: "
                                   f"{(out + error).decode(errors='replace').strip()[-200:]}")
        return out.decode(errors="replace")

    def finished(self, step):
        """The task's state and its program's exit code, once it has one."""
        deadline = time.monotonic() + self.timeout
        while True:
            listing = self.schtasks(["/query", "/tn", self.task, "/fo", "list", "/v"], step)
            running = re.search(r"^Status:\s+Running\s*$", listing, re.M)
            result = re.search(r"^Last Result:\s+(-?\d+)\s*$", listing, re.M)
            if running is None and result is not None:
                return int(result.group(1))
            if time.monotonic() >= deadline:
                raise StepFailed(step, f"{self.task} was still running after {self.timeout} s")
            time.sleep(self.poll_seconds)

    def run(self, path, guest_output, step):
        """`path` run in the console session, its output left in `guest_output`:
        the exit code its task recorded."""
        user = self.console_user()
        self.agent.push(f"@echo off\r\n{path} > {guest_output} 2>&1\r\n".encode(), self.script)
        self.schtasks(["/create", "/tn", self.task, "/tr", self.script, "/sc", "once", "/st", "23:59",
                       "/ru", user, "/it", "/f"], step)
        try:
            self.schtasks(["/run", "/tn", self.task], step)
            return self.finished(step)
        finally:
            code, _, _ = self.agent.run(SCHTASKS, ["/delete", "/tn", self.task, "/f"], self.timeout)
            if code != 0:
                print(f"guest: {step}: the {self.task} task was left behind (schtasks /delete exited {code})",
                      flush=True)


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
        self.relay = AgentRelay(self.work / "agent-relay.sock", lambda: self.agent)
        self.log = []
        self.colours = 0
        self.heads = 0
        self.unexpected_dump = None
        self.sleep_states = None
        # The CTS files a --cts run pushes, once its pins were honoured.
        self.cts_files = []
        # What a --d3d run pushes, by layer, once its pins were honoured.
        self.layer_files = {}
        # The host GPUs an --all-gpus run gave a device each, in QEMU's order.
        self.host_gpus = []

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
            required += [self.args.driver, self.args.inf, self.args.icd, self.args.icd_manifest,
                         self.args.gl_icd, self.args.cl_icd, self.args.test, self.args.inspect]
        if self.args.vulkan:
            required.append(self.args.probe)
        if self.args.opengl_discovery or self.args.opengl:
            required.append(self.args.gl_probe)
        if self.args.opencl:
            required += [self.args.cl_probe, str(self.opencl_loader_dll())]
        if self.args.d3d:
            required += [self.args.d3d9_probe, self.args.d3d11_probe, self.args.d3d12_probe]
        for path in required:
            if not os.path.isfile(path):
                raise StepFailed("prepare", f"{path} does not exist")
        if self.args.vulkan:
            self.vulkan_runtime()
        if self.args.opencl:
            self.opencl_loader()
        if self.args.cts:
            self.cts_build()
        if self.args.page_heap and not self.args.d3d:
            raise StepFailed("prepare", "--page-heap is what the D3D probes run under: it needs --d3d")
        if self.args.d3d:
            # The layers call the loader, so a --d3d run needs the pinned
            # runtime as much as a --vulkan one.
            self.vulkan_runtime()
            self.layer_files = self.d3d_layers()
        if self.args.all_gpus:
            self.host_gpus = self.every_host_gpu()
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

    def opencl_loader_dll(self):
        """The Khronos loader a --opencl run pushes: the cross-built OpenCL.dll
        the guest programs link against, which the build leaves beside the
        probe. The pinned tree is the source it was built from and never
        reaches the guest."""
        return pathlib.Path(self.args.cl_probe).parent / "OpenCL.dll"

    def opencl_loader(self):
        """The loader source a --opencl run was built from, as it was pinned: a
        tree nobody fetched, or one that is not the pinned one, is refused
        before anything boots - the same as the Vulkan runtime, and for the same
        reason."""
        try:
            _url, digest = cl_pins_from(pathlib.Path(self.args.opencl_pins))
        except OSError as error:
            raise StepFailed("prepare", f"--opencl: no pins: {error}")
        except ClRefused as refused:
            raise StepFailed("prepare", f"--opencl: {refused}")
        into = pathlib.Path(self.args.opencl_loader)
        problems = cl_problems_on_disk(into, digest)
        if problems:
            listed = ", ".join(f"{name} {problem}" for name, problem in sorted(problems.items()))
            raise StepFailed("prepare", f"--opencl needs the pinned OpenCL loader in {into} ({listed}): "
                                        "run tools/fetch_opencl_loader.py")

    def d3d_layers(self):
        """The DLLs a --d3d run pushes, as they were pinned: a layer nobody
        fetched, or one that is not the pinned one, is refused before anything
        boots - the same as the Vulkan runtime, and for the same reason. The
        pins themselves are what says which files there are, so a layer whose
        pins are empty is refused naming the fetch to run.

        Answers the files to push, as (local path, name) by layer."""
        pushed = {}
        for layer in sorted(set(D3D_LAYERS.values())):
            pins = LayerPins(pathlib.Path(self.args.layer_pins) / f"{layer.replace('-', '_')}_pins.txt")
            try:
                pins.read()
            except LayerRefused as refused:
                raise StepFailed("prepare", f"--d3d: {layer}: {refused}: run tools/fetch_layers.py {layer}")
            if not pins.files:
                raise StepFailed("prepare", f"--d3d: {layer}: its pins name no files: "
                                            f"run tools/fetch_layers.py {layer}")
            into = pathlib.Path(self.args.layers) / layer
            problems = LayerFetch(layer, pins, into, into).problems()
            # The source tree is what the requirement gate reads, not what a
            # run pushes: a fetched release with no source is still a run.
            problems.pop("source", None)
            if problems:
                listed = ", ".join(f"{name} {problem}" for name, problem in sorted(problems.items()))
                raise StepFailed("prepare", f"--d3d needs the pinned {layer} in {into} ({listed}): "
                                            f"run tools/fetch_layers.py {layer}")
            pushed[layer] = [(into / name, name) for name in sorted(pins.files)]
        return pushed

    def cts_build(self):
        """The CTS a --cts run pushes, as it was pinned: a build nobody fetched
        is refused before anything boots, naming the fetch - never a run that
        reports conformance it did not measure."""
        pins = CtsPins(pathlib.Path(self.args.cts_pins))
        try:
            pins.read()
        except LayerRefused as refused:
            raise StepFailed("prepare", f"--cts: {refused}: run tools/fetch_cts.py")
        into = pathlib.Path(self.args.cts_dir)
        problems = CtsFetch(pins, into, into).problems()
        if problems:
            listed = ", ".join(f"{name} {problem}" for name, problem in sorted(problems.items()))
            raise StepFailed("prepare", f"--cts needs the pinned CTS in {into} ({listed}): "
                                        "run tools/fetch_cts.py")
        self.cts_files = [(into / name, name) for name in sorted(pins.files)]

    def every_host_gpu(self):
        """The host's GPUs as list_gpus.py reads them, CPU devices left out:
        llvmpipe is not a GPU a guest would be shown, and it has no PCI
        function to name it."""
        if self.args.vulkaninfo:
            try:
                text = pathlib.Path(self.args.vulkaninfo).read_text()
            except OSError as error:
                raise StepFailed("prepare", f"--vulkaninfo {self.args.vulkaninfo}: {error.strerror}")
        else:
            try:
                text = subprocess.run(["vulkaninfo"], capture_output=True, text=True, check=True).stdout
            except (OSError, subprocess.CalledProcessError) as error:
                raise StepFailed("prepare", f"--all-gpus: vulkaninfo failed: {error}")
        try:
            gpus = [gpu for gpu in list_gpus.parse(text) if gpu.kind != "cpu"]
        except ValueError as error:
            raise StepFailed("prepare", f"--all-gpus: vulkaninfo output: {error}")
        if not gpus:
            raise StepFailed("prepare", "--all-gpus: the host has no Vulkan GPU that is not a CPU")
        for gpu in gpus:
            vendor, device, revision = gpu.pci_identity(self.args.sysfs)
            self.say(f"host gpu: {gpu.uuid} {vendor:04x}:{device:04x} rev {revision:02x} "
                     f"{gpu.adapter_name()!r}")
        return gpus

    def adapter_names(self):
        """What provision.ps1 names each host GPU's INF model with."""
        lines = []
        for gpu in self.host_gpus:
            vendor, device, _revision = gpu.pci_identity(self.args.sysfs)
            lines.append(f"{vendor:04x}\t{device:04x}\t{gpu.adapter_name()}\n")
        path = self.work / "adapter-names.txt"
        path.write_text("".join(lines), encoding="utf-8")
        return path

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
        if self.args.record and not rescue:
            self.say(f"recording what crosses the wire into {self.work / RECORDING}; "
                     f"build/rvkrecord print it")
        output = open(self.qemu_log, "wb")
        environment = dict(os.environ, QEMU_MODULE_DIR=os.path.dirname(os.path.abspath(self.args.qemu)))
        # Where the executor library loads the host's Direct3D from: DXVK and
        # vkd3d-proton as tools/build_d3d_runtime.sh installs them. A checkout
        # without them runs every other sub-API (src/device/shim/d3d_runtime.hpp).
        if os.path.isdir(self.args.d3d_runtime) and "RVK_D3D_PATH" not in environment:
            environment["RVK_D3D_PATH"] = os.path.abspath(self.args.d3d_runtime)
        # DXVK writes its logs to the working directory unless told otherwise.
        environment.setdefault("DXVK_LOG_PATH", str(self.work))
        if self.args.perfetto:
            environment["RVK_PERFETTO"] = "1"
        # What the test executor is told to do to the host's work: a stall
        # longer than TdrDelay is what dxgkrnl times out on (feature 040, US3).
        if self.args.stall_ms:
            environment["RVK_ECHO_STALL_MS"] = str(self.args.stall_ms)
        if self.args.fault_after:
            environment["RVK_ECHO_FAULT_AFTER"] = str(self.args.fault_after)
        if self.args.stall_ms or self.args.fault_after:
            environment["RVK_ECHO_ONLY_NTH"] = str(self.args.stall_context)
        self.qemu = subprocess.Popen(qemu_command(self.args, self.work, self.disk, rescue, self.host_gpus),
                                     stdout=output, stderr=subprocess.STDOUT, env=environment)
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
                      (self.args.gl_icd, PACKAGE_DIRECTORY + "\\rvkgl.dll"),
                      (self.args.cl_icd, PACKAGE_DIRECTORY + "\\rvkcl.dll"),
                      (self.args.umd, PACKAGE_DIRECTORY + "\\rvkumd.dll"),
                      (self.args.test, GUEST_DIRECTORY + "\\rvktest.exe"),
                      (self.args.inspect, GUEST_DIRECTORY + "\\rvkinspect.exe")]
            if self.host_gpus:
                files.append((self.adapter_names(), ADAPTER_NAMES))
        if self.args.vulkan:
            # The loader beside the programs that import it, where Windows
            # looks first.
            runtime = pathlib.Path(self.args.vulkan_runtime)
            files += [(runtime / "vulkan-1.dll", GUEST_DIRECTORY + "\\vulkan-1.dll"),
                      (runtime / "vulkaninfo.exe", GUEST_DIRECTORY + "\\vulkaninfo.exe"),
                      (self.args.probe, GUEST_DIRECTORY + "\\rvkvkprobe.exe")]

        if self.args.opengl_discovery or self.args.opengl:
            files.append((self.args.gl_probe, GUEST_DIRECTORY + "\\rvkglprobe.exe"))

        if self.args.opencl:
            # The loader beside the probe that imports it, where Windows looks
            # first, so the run is the fetched loader's rather than whatever
            # another runtime left in System32.
            files += [(self.opencl_loader_dll(), GUEST_DIRECTORY + "\\OpenCL.dll"),
                      (self.args.cl_probe, GUEST_DIRECTORY + "\\rvkclprobe.exe")]

        if self.args.cts:
            # The CTS and the loader it calls, in a directory of its own: it
            # writes its logs beside itself and a run pulls them from there.
            runtime = pathlib.Path(self.args.vulkan_runtime)
            files.append((runtime / "vulkan-1.dll", CTS_DIRECTORY + "\\vulkan-1.dll"))
            files += [(local, CTS_DIRECTORY + "\\" + name) for local, name in self.cts_files]

        if self.args.d3d:
            # The layers' DLLs and the loader they call, beside the probes that
            # import them: Windows looks in the program's own directory first,
            # so this is what makes the run the layer's rather than the
            # system's.
            runtime = pathlib.Path(self.args.vulkan_runtime)
            files.append((runtime / "vulkan-1.dll", GUEST_DIRECTORY + "\\vulkan-1.dll"))
            for _layer, pushed in sorted(self.layer_files.items()):
                files += [(local, GUEST_DIRECTORY + "\\" + name) for local, name in pushed]
            files += [(self.args.d3d9_probe, GUEST_DIRECTORY + "\\rvkd3d9probe.exe"),
                      (self.args.d3d11_probe, GUEST_DIRECTORY + "\\rvkd3d11probe.exe"),
                      (self.args.d3d12_probe, GUEST_DIRECTORY + "\\rvkd3d12probe.exe")]

        def send():
            # The CTS keeps its build and its logs in a directory of its own,
            # and only a --cts run has one.
            directories = PACKAGE_DIRECTORY + (f",{CTS_DIRECTORY}" if self.args.cts else "")
            make = f"New-Item -ItemType Directory -Force -Path {directories}"
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
        """US4: Windows attaches one monitor to every output each device
        reports, and the check stage counted them."""
        monitors = re.search(r"monitors=(\d+)", output)
        if monitors is None:
            raise StepFailed("check", "the check stage did not count the guest's monitors")
        # A window's run gives every GPU but the first display=off: those
        # render and drive no monitor (qemu_command).
        windowed = self.args.display in ("gtk", "sdl")
        devices = 1 if windowed else max(len(self.host_gpus), 1)
        if int(monitors.group(1)) != self.args.outputs * devices:
            raise StepFailed("check", f"the guest has {monitors.group(1)} monitors, not "
                                      f"{self.args.outputs * devices}")
        adapters = re.search(r"adapters=(\d+)", output)
        if self.host_gpus and (adapters is None or int(adapters.group(1)) != devices):
            found = adapters.group(1) if adapters else "no"
            raise StepFailed("check", f"the guest has {found} rvkgpu adapters, not {devices}")
        self.say(f"check ok: {monitors.group(1)} monitor(s)")

    def expected_adapters(self):
        """(vendor, device, revision, name) for each device an --all-gpus run
        started: what Windows must say it is."""
        return [(*gpu.pci_identity(self.args.sysfs), gpu.adapter_name()) for gpu in self.host_gpus]

    def adapters(self):
        """Every host GPU shown by its host's name and PCI identity, in the two
        places an application looks: WMI's Win32_VideoController (Name and
        PNPDeviceID) and DXGI's EnumAdapters1 (Description, VendorId, DeviceId,
        SubSysId, Revision). Each adapter Windows lists is matched once, so two
        of the same GPU need two adapters."""
        output = self.stage_output("adapters", saved="adapters.txt")
        rows = [line.rstrip("\r").split("|") for line in output.splitlines()]
        failed = [row[1] for row in rows if row[0] == "dxgi-error"]
        if failed:
            raise StepFailed("adapters", f"DXGI could not list adapters: {'; '.join(failed)}")
        wmi = [row for row in rows if row[0] == "wmi" and len(row) >= 6]
        dxgi = [row for row in rows if row[0] == "dxgi" and len(row) >= 9]
        missing = []
        for vendor, device, revision, name in self.expected_adapters():
            pnp = f"PCI\\VEN_{vendor:04X}&DEV_{device:04X}&SUBSYS_{RVK_SUBSYSTEM:08X}&REV_{revision:02X}\\"
            shown = next((row for row in wmi if row[1].upper().startswith(pnp) and row[5] == name), None)
            if shown is None:
                missing.append(f"WMI has no {pnp[:-1]} named {name!r}")
            else:
                wmi.remove(shown)
                self.say(f"adapters: WMI {name!r} {shown[1]}, AdapterRAM {shown[2]}, "
                         f"AdapterCompatibility {shown[3]!r}")
            ids = (f"{vendor:04x}", f"{device:04x}", f"{RVK_SUBSYSTEM:08x}", f"{revision:02x}")
            listed = next((row for row in dxgi if tuple(row[1:5]) == ids and row[8] == name), None)
            if listed is None:
                missing.append(f"DXGI has no {':'.join(ids[:2])} subsystem {ids[2]} rev {ids[3]} named {name!r}")
            else:
                dxgi.remove(listed)
                self.say(f"adapters: DXGI {name!r} {':'.join(ids[:2])} rev {ids[3]}, "
                         f"DedicatedVideoMemory {listed[5]}")
        if missing:
            raise StepFailed("adapters", "; ".join(missing) + "; see adapters.txt")
        self.say(f"adapters ok: {len(self.host_gpus)} host GPU(s) named and identified in WMI and DXGI")

    def window(self):
        """Whether the driver set up the host window it hands processes their
        write-back chunks from (docs/host-window.md). Without one every chunk
        is guest memory, which works but is not what a run with --hostmem is
        measuring."""
        opened = WINDOW_LINE.search(self.driver_log())
        if not self.args.hostmem:
            self.say("window ok: this device has none, so every chunk is guest memory")
            return
        if opened is None:
            raise StepFailed("window", "the driver set up no host window: the capset's host-visible bit is "
                                       "clear, or CapsetExchange failed at StartDevice")
        status = int(opened.group(1), 16)
        if status != 0:
            raise StepFailed("window", f"the driver could not set up the host window: status 0x{status:08x}")
        described = ", ".join(f"{size // MEGABYTE} MiB at {base:#x}" for base, size in self.hostmem_bars())
        self.say(f"window ok: {described}")

    def hostmem_bars(self):
        """Where the guest programmed each device's hostmem BAR, and how much of
        it there is, from QEMU's own view of the bus. With --all-gpus there is
        one device per host GPU, and the suite's adapter may be any of them."""
        try:
            buses = self.qmp.execute("query-pci")
        except AgentError as error:
            raise StepFailed("window", str(error))
        regions = [region for bus in buses for device in bus.get("devices", [])
                   if device.get("qdev_id", "").startswith(DISPLAY_DEVICE)
                   for region in device.get("regions", []) if region.get("bar") == HOSTMEM_BAR]
        if not regions:
            raise StepFailed("window", f"QEMU maps no BAR {HOSTMEM_BAR} on the device: the window the driver "
                                       "set up is nowhere in the guest's physical address space")
        return [(region["address"], region["size"]) for region in regions]

    def stage_output(self, name, saved=None, extra=()):
        """The stage's own output, for a step that reads what it said."""
        code, out, error = self.provision(name, extra)
        text = (out + error).decode(errors="replace")
        if saved is not None:
            (self.work / saved).write_bytes(out + error)
        if code not in (0, REBOOT_REQUIRED):
            raise StepFailed(name, f"exited {code}: {text.strip()[-400:]}")
        return text

    def stage(self, name, saved=None, extra=()):
        """Runs one provisioning stage: whether it asked for a reboot."""
        code, out, error = self.provision(name, extra)
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
        self.report_submit(out)
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

    def report_submit(self, report):
        """US3: what a submission cost each way on this boot, and the ratio
        against the record. The costs are said, never held: they are the
        host's, and the ratio is the driver's (research.md §5)."""
        ratio = SUBMIT_RATIO.search(report)
        doorbell_ns = SUBMIT_DOORBELL_NS.search(report)
        kernel_ns = SUBMIT_KERNEL_NS.search(report)
        if ratio is None or doorbell_ns is None or kernel_ns is None:
            self.say("submit skipped: the suite measured no submission cost")
            return
        connected = SUBMIT_DOORBELL.search(report)
        if connected is None or connected.group(1) != b"connected":
            self.say(f"submit skipped: this machine gave the adapter no hardware queue, so only the kernel "
                     f"path ran ({int(kernel_ns.group(1))} ns a submission); nothing is measured against the "
                     "record")
            return
        measured = int(ratio.group(1))
        self.say(f"submit ok: {int(doorbell_ns.group(1))} ns through the doorbell, "
                 f"{int(kernel_ns.group(1))} ns through the kernel, ratio {measured / 100:.2f}x")
        record = SubmitRatchet(pathlib.Path(self.args.submit_ratchet))
        try:
            recorded = record.recorded
        except ValueError as refused:
            raise StepFailed("submit.ratchet", str(refused)) from refused
        lost = record.lost(measured)
        if lost:
            raise StepFailed("submit.ratchet", f"the doorbell path is worth {measured / 100:.2f}x the kernel "
                                               f"one here and {recorded / 100:.2f}x was recorded")
        if not recorded:
            self.say(f"submit.ratchet: nothing recorded yet; commit `{record.line(measured).strip()}` into "
                     f"{self.args.submit_ratchet} in the change that earned it")
            return
        gained = record.gained(measured)
        self.say(f"submit.ratchet ok: {recorded / 100:.2f}x kept" +
                 (f", {gained / 100:.2f}x gained; commit `{record.line(measured).strip()}`" if gained else ""))

    def display(self):
        """The frame the host scans out of the guest's primary (feature 038, US1).

        What a screendump can prove today is that the host reads *our* resource:
        the dump is the committed mode's size, and it is no longer QEMU's "no
        output" placeholder. What it cannot prove is that the picture is a
        desktop: the primary's pages are still all zeros. Feature 046 measured
        why (docs/kb/07-usermode.md): with no D3D user-mode driver, DWM composes
        with WARP and never presents through DxgkDdiPresent - the two presents a
        boot makes name no allocations - so nothing writes the primary. The
        desktop waits for guest GPU work to execute on the host (features 049
        and 055-057). The driver's own log carries
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
        than a scanout of ours. A console with no surface at all shows nothing
        of ours either, which is the same answer."""
        try:
            dump = Screendump(self.work / f"screen{head}.ppm").capture(self.qmp, head)
        except StepFailed as refusal:
            if "no surface" not in str(refusal):
                raise
            return True
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
        dump = Screendump(self.work / f"screen{head}.ppm").capture(self.qmp, head, SCANOUT_WAIT_SECONDS)
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
        self.recording_cost(out)

    def recording_cost(self, report):
        """SC-002: what a wire recording costs a no-op round trip. No boot can
        say it alone - one runs with --record and one without - so each keeps
        its warm median beside qemu.log and whichever completes the pair
        subtracts. Said, never judged: the threshold a run would be failed
        against is a number this comparison exists to produce."""
        median = RVK_WARM_MEDIAN.search(report)
        if median is None:
            self.say("recording cost skipped: the rvk run timed no warm round trip")
            return
        measured = int(median.group(1))
        side = "on" if self.args.record else "off"
        record = RecordingCost(self.work / RECORDING_COST)
        try:
            record.keep(side, measured)
        except ValueError as refused:
            raise StepFailed("recording.cost", str(refused)) from refused
        cost = record.cost()
        if cost is None:
            missing = "without" if self.args.record else "with"
            self.say(f"recording cost: a no-op round trip took {measured} us with recording {side}; the same "
                     f"run {missing} --record completes SC-002's pair in {self.work / RECORDING_COST}")
            return
        held = record.measured
        self.say(f"recording cost ok: a no-op round trip took {held['off']} us with recording off and "
                 f"{held['on']} us with it on; " +
                 (f"a recording costs {cost} us" if cost > 0 else
                  "a recording costs nothing this pair could measure"))

    def vulkan(self):
        """Feature 045: the Khronos loader finds rvkvk.dll through the adapter's
        VulkanDriverName alone, vulkaninfo lists the one device that gives it,
        and the probe proves a device, its work and its loss through the loader
        on the host GPU the executor runs on."""
        self.say(f"vulkan: executor {self.args.executor}, host-gpu {self.args.host_gpu}")
        self.khronos_key()
        listed = self.vulkaninfo()
        steps = self.probe()
        self.pictures("vulkan", VULKAN_PICTURES)
        self.frame("vulkan")
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
        values the steps printed, by step.

        It runs in the console session: its present steps make a window and read
        the presented frame back off it, which the agent's own session cannot do
        (ConsoleProgram)."""
        console = ConsoleProgram(self.agent, self.args.step_timeout)
        report = GUEST_DIRECTORY + "\\vulkan.txt"
        code = self.step("rvkvkprobe",
                         lambda: console.run(GUEST_DIRECTORY + "\\rvkvkprobe.exe", report, "rvkvkprobe"))
        self.step("rvkvkprobe.pull", lambda: self.agent.pull(report, self.work / "vulkan.txt"))
        out = (self.work / "vulkan.txt").read_bytes()
        summary = PROBE_SUMMARY.search(out)
        if summary is None:
            raise StepFailed("vulkan", f"no summary line (exit {code}); see vulkan.txt")
        failed = int(summary.group(2))
        if failed != 0 or code != 0:
            raise StepFailed("vulkan", f"{failed} step(s) failed, exit {code}; see vulkan.txt")
        steps = {name.decode(): value.decode() for name, value in PROBE_STEP.findall(out)}
        missing = [name for name in PROBE_STEPS if name not in steps]
        if missing:
            raise StepFailed("vulkan", f"the probe reported no {', '.join(missing)}; see vulkan.txt")
        self.say(f"vulkan ok: {int(summary.group(1))} passed")
        return steps

    def d3d(self):
        """FR-005: each of the three D3D probes through the layer beside it,
        every step of its report, and the frame it cleared pulled back.

        They run in the console session: a D3D9 or D3D11 device is made on a
        window, and a window of the agent's own session is not one a layer can
        present on (ConsoleProgram)."""
        programs = [f"rvkd3d{api}probe.exe" for api in ("9", "11", "12")]
        if self.args.page_heap:
            self.page_heap(programs, enable=True)
        try:
            for api in ("9", "11", "12"):
                self.d3d_probe(api)
        finally:
            # The flag comes off first, so a machine is left as it was found
            # whatever the probes did; then what Windows logged is read. A heap
            # event takes precedence over a failed step, because it explains
            # one.
            if self.args.page_heap:
                self.page_heap(programs, enable=False)
                self.heap_checks()
        self.pictures("d3d", D3D_PICTURES)
        self.frame("d3d")

    def page_heap(self, programs, enable):
        """FR-006: full page heap on each probe, and off again afterwards - a
        machine left with it on would run every later boot's probes under it."""
        state = "on" if enable else "off"
        extra = ["-Programs", ",".join(programs)] + (["-Enable"] if enable else [])
        self.step(f"pageheap.{state}", lambda: self.provision("pageheap", extra))
        self.say(f"pageheap {state}: {len(programs)} program(s)")

    def heap_checks(self):
        """What Windows logged while the probes ran under page heap: every
        event, and the count. FR-006 expects none, and a run that found some
        says what they were rather than only that it failed - whether the
        corruption the spec looked for is reproducible here is the answer
        either way."""
        _code, out, _error = self.provision("heapchecks")
        text = out.decode(errors="replace")
        for identifier, message in HEAP_EVENT.findall(text):
            self.say(f"heapcheck: event {identifier}: {message[:160]}")
        counted = HEAP_CHECKS.search(text)
        if counted is None:
            raise StepFailed("heapchecks", "the guest reported no count; see the run's output")
        found = int(counted.group(1))
        if found != 0:
            raise StepFailed("heapchecks", f"{found} heap event(s) while the probes ran under page heap")
        self.say("heapchecks ok: no heap event under page heap")

    def d3d_probe(self, api):
        """One probe: run, pulled back, and judged against the steps its API
        must report."""
        console = ConsoleProgram(self.agent, self.args.step_timeout)
        name = f"rvkd3d{api}probe"
        report = f"{GUEST_DIRECTORY}\\d3d{api}.txt"
        code = self.step(name, lambda: console.run(f"{GUEST_DIRECTORY}\\{name}.exe", report, name))
        self.step(f"{name}.pull", lambda: self.agent.pull(report, self.work / f"d3d{api}.txt"))
        out = (self.work / f"d3d{api}.txt").read_bytes()
        summary = next((found for found in D3D_SUMMARY.finditer(out) if found.group(1).decode() == api), None)
        if summary is None:
            raise StepFailed(f"d3d{api}", f"no summary line (exit {code}); see d3d{api}.txt")
        failed = int(summary.group(3))
        if failed != 0 or code != 0:
            raise StepFailed(f"d3d{api}", f"{failed} step(s) failed, exit {code}; see d3d{api}.txt")
        steps = {step.decode(): (value or b"").decode()
                 for found, step, value in D3D_STEP.findall(out) if found.decode() == api}
        missing = [step for step in D3D_STEPS[api] if step not in steps]
        if missing:
            raise StepFailed(f"d3d{api}", f"the probe reported no {', '.join(missing)}; see d3d{api}.txt")
        self.say(f"d3d{api} ok: {int(summary.group(2))} passed through {D3D_LAYERS[api]}")
        return steps

    def cts(self):
        """US1: the subset run in the guest, shard by shard, and what it says
        against the host's run and against what the guest passed before.

        The CTS makes windows - the WSI groups are half the point - so it runs
        in the console session, as the Vulkan probe does."""
        cases = self.cts_cases()
        shards = CtsShards(self.work / "cts" / "cases.txt", max(1, self.args.cts_shard))
        logs = []
        for number, shard in enumerate(shards.shards()):
            logs += self.cts_shard(number, shards, shard)
        self.say(f"cts: {len(cases)} case(s) in {len(shards.shards())} shard(s), {len(logs)} log(s)")
        return self.cts_verdict(logs)

    def cts_cases(self):
        """The cases this subset names, out of the list the CTS itself printed:
        one dEQP group is thousands of cases and the list changes with every
        release, so the guest's own build says what there is to run."""
        printed = f"{CTS_DIRECTORY}\\caselist.txt"
        command = f"{CTS_DIRECTORY}\\deqp-vk.exe --deqp-runmode=stdout-caselist > {printed} 2>&1"
        self.step("cts.caselist", lambda: self.agent.run(CMD, ["/c", command], self.args.step_timeout))
        (self.work / "cts").mkdir(exist_ok=True)
        self.step("cts.caselist.pull", lambda: self.agent.pull(printed, self.work / "cts" / "caselist.txt"))
        listed = []
        for line in (self.work / "cts" / "caselist.txt").read_text(errors="replace").splitlines():
            # dEQP prints "TEST: <name>" for a case and "GROUP: ..." for a
            # group; anything else is the build telling us about itself.
            if line.startswith("TEST: "):
                listed.append(line[len("TEST: "):].strip())
        subset = CtsSubset(pathlib.Path(self.args.cts_subset))
        try:
            cases = subset.matching(listed)
        except ValueError as refused:
            raise StepFailed("cts.caselist", str(refused))
        if not cases:
            raise StepFailed("cts.caselist", f"the subset matched none of the {len(listed)} case(s) the CTS "
                                             "listed; see cts/caselist.txt")
        (self.work / "cts" / "cases.txt").write_text("".join(f"{name}\n" for name in cases))
        return cases

    def cts_shard(self, number, shards, shard):
        """One shard, and the runs it took: a shard whose log ends inside a
        case is picked up after that case, because the case that killed the
        process is not a case to run again."""
        logs = []
        remaining = shard
        for attempt in range(self.args.cts_restarts + 1):
            name = f"shard{number}" if attempt == 0 else f"shard{number}.r{attempt}"
            log = self.cts_run(name, remaining)
            logs.append(log)
            results = QpaResults(log)
            if results.unfinished is None:
                return logs
            self.say(f"cts: {name} died in {results.unfinished}")
            remaining = shards.after(remaining, results.unfinished)
            if not remaining:
                return logs
            # A guest that bugchecked is not a guest to run the next shard on.
            if self.panicked_while_booting() is not None:
                self.reach_guest(True)
        raise StepFailed(f"cts.shard{number}", f"died in a case {self.args.cts_restarts + 1} time(s) with "
                                               f"{len(remaining)} case(s) unrun; see cts/shard{number}*.qpa")

    def cts_run(self, name, cases):
        """One run of the CTS over `cases`, its log pulled back."""
        listed = f"{CTS_DIRECTORY}\\{name}.cases.txt"
        self.step(f"cts.{name}.cases",
                  lambda: self.agent.push("".join(f"{case}\n" for case in cases).encode(), listed))
        log = f"{CTS_DIRECTORY}\\{name}.qpa"
        console = ConsoleProgram(self.agent, self.args.step_timeout)
        command = (f"{CTS_DIRECTORY}\\deqp-vk.exe --deqp-caselist-file={listed} "
                   f"--deqp-log-filename={log} --deqp-log-images=disable")
        # A case that hangs is the CTS's own to give up on, and a run that
        # answers nothing is still a log to read: the exit code is recorded,
        # never the verdict.
        code = self.step(f"cts.{name}", lambda: console.run(command, f"{CTS_DIRECTORY}\\{name}.txt", name))
        self.say(f"cts: {name} exited {code}")
        local = self.work / "cts" / f"{name}.qpa"
        self.step(f"cts.{name}.pull", lambda: self.agent.pull(log, local))
        return local

    def cts_verdict(self, logs):
        """What the run answers: the ratchet always, and the comparison with a
        host run when one was named (FR-002)."""
        whole = self.work / "cts" / "guest.qpa"
        whole.write_text("".join(log.read_text(errors="replace") for log in logs))
        results = QpaResults(whole)
        self.say(f"cts: {len(results.cases)} case(s) answered, {len(results.passing())} passed")
        record = CtsRatchet(pathlib.Path(self.args.cts_ratchet))
        lost = record.lost(results)
        for name in lost:
            self.say(f"cts: {name} passed before and does not now")
        if lost:
            raise StepFailed("cts.ratchet", f"{len(lost)} case(s) lost of {len(record.names)} recorded")
        gained = record.gained(results)
        self.say(f"cts.ratchet ok: {len(record.names)} kept, {len(gained)} gained")
        if self.args.cts_host is None:
            return results
        host = QpaResults(pathlib.Path(self.args.cts_host))
        compared = SuiteComparison(results, host)
        for name in compared.guest_only_failures:
            self.say(f"cts: {name} passes on the host and does not in the guest")
        if compared.unjudged:
            raise StepFailed("cts.compare", f"{len(compared.unjudged)} case(s) ran on one side only; "
                                            "the two runs are not the same subset")
        if compared.guest_only_failures:
            raise StepFailed("cts.compare", f"{len(compared.guest_only_failures)} guest-only failure(s) of "
                                            f"{compared.compared} compared")
        self.say(f"cts ok: {compared.compared} compared, 0 guest-only failures, "
                 f"{len(compared.matching_failures)} matching failure(s)")
        return results

    def frame(self, name):
        """US2: the device's scanout after an application ran, against the
        reference committed for it.

        The scanout rather than a guest screenshot (FR-004), because a guest
        screenshot goes through the guest's own composition - which is the
        thing under test. A frame with no reference committed is kept and said
        so: the first run of an application is where a reference comes from,
        and a claim that it matched one would be a claim nobody made.
        """
        if not self.args.frames:
            return None
        frames = self.work / "frames"
        frames.mkdir(exist_ok=True)
        captured = frames / f"{name}.ppm"
        self.step(f"frames.{name}", lambda: Screendump(captured).capture(self.qmp))
        reference = pathlib.Path(self.args.references) / f"{name}.ppm"
        if not reference.is_file():
            self.say(f"frames: no reference committed for {name}; the capture is {captured}")
            return None
        frame = FrameReference(captured, reference, self.args.frame_tolerance)
        unreadable = frame.unreadable() or frame.mismatched_size()
        if unreadable is not None:
            raise StepFailed(f"frames.{name}", unreadable)
        differs = frame.differs()
        if differs != 0:
            difference = frame.write_difference(frames / f"{name}-difference.ppm")
            raise StepFailed(f"frames.{name}", f"{differs} pixel(s) outside a step of "
                                               f"{self.args.frame_tolerance}; see {captured}, {reference} "
                                               f"and {difference}")
        self.say(f"frames.{name} ok: the scanout is the reference")
        return captured

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
        # The verifier is part of what the install decides, so the run says which
        # driver it is about to measure before it measures anything.
        if self.args.no_verifier:
            self.say("install: no Driver Verifier, by --no-verifier")
        if self.stage("install", extra=("-NoVerifier",) if self.args.no_verifier else ()):
            self.reboot("install")
        self.check_monitors(self.stage_output("check", saved="device.txt"))
        if self.host_gpus:
            self.adapters()
        self.window()
        # The display and power scenarios are the echo run's; these boots are
        # for the work the real executor does (features 044 and 045).
        if self.args.opengl_discovery:
            self.opengl_discovery()
            return
        scenarios = [scenario for chosen, scenario in ((self.args.rvk, self.rvk), (self.args.vulkan, self.vulkan),
                                                        (self.args.opengl, self.opengl),
                                                        (self.args.opencl, self.opencl),
                                                        (self.args.d3d, self.d3d),
                                                        (self.args.cts, self.cts)) if chosen]
        if scenarios:
            self.real_executor(scenarios)
            return
        if self.args.quick:
            self.say("quick: the display, power, cycle, upgrade, sleep and hibernate scenarios are left to the gate")
        else:
            self.whole_machine()
        if self.injecting():
            self.tdr()
        else:
            self.suite()
            self.vsync()

    def real_executor(self, scenarios):
        """The scenarios that need the real executor share one boot - each is a
        process of its own and leaves the machine as it found it - and each runs
        whatever an earlier one answered, so one boot reports them all; the run
        fails naming every scenario that failed."""
        failed_steps = []
        for scenario in scenarios:
            try:
                scenario()
            except StepFailed as failed:
                self.say(f"{failed.step} failed: {failed.reason}")
                failed_steps.append(failed.step)
        if failed_steps:
            raise StepFailed("scenarios", f"{len(failed_steps)} of {len(scenarios)} failed: {', '.join(failed_steps)}")

    def opengl_discovery(self):
        """Feature 050, T008-T009: what the system opengl32.dll does for a window
        a session-0 program makes - whether the adapter's ICD is what it loads -
        recorded rather than judged, since the answer decides how the ICD is
        built (research.md §9). Only a probe that made no report fails."""
        report = GUEST_DIRECTORY + "\\discovery.txt"
        command = f"{GUEST_DIRECTORY}\\rvkglprobe.exe --discovery > {report} 2>&1"
        code, _out, _error = self.step("discovery.run", lambda: self.agent.run(CMD, ["/c", command],
                                                                             self.args.step_timeout))
        self.step("discovery.pull", lambda: self.agent.pull(report, self.work / "discovery.txt"))
        text = (self.work / "discovery.txt").read_text(errors="replace")
        if code != 0:
            raise StepFailed("discovery", f"the probe exited {code} (its window could not be made); "
                                          "see discovery.txt")
        facts = dict(DISCOVERY_LINE.findall(text))
        if "rvkgl_loaded" not in facts:
            raise StepFailed("discovery", "the probe reported no rvkgl_loaded line")
        loaded = "loaded" if facts["rvkgl_loaded"].startswith("yes") else "not loaded"
        opened = DISCOVERY_OPEN.search(text)
        if opened is not None and opened.group(1) == "ok":
            adapter = f"the DC opened an adapter (LUID {int(opened.group(2), 16):#x})"
        elif opened is not None:
            adapter = f"the DC opened no adapter (status {int(opened.group(3)):#010X})".replace("0X", "0x")
        else:
            adapter = "the probe did not open the DC's adapter"
        self.say(f"discovery ok: rvkgl.dll {loaded}, {adapter}")

    def opengl(self):
        """Feature 050, SC-001 to SC-003: every step of rvkglprobe.exe through the
        system opengl32.dll, on a renderer that is the host GPU's. The report is
        written to a guest file and pulled, as vulkaninfo's is."""
        report = GUEST_DIRECTORY + "\\opengl.txt"
        command = f"{GUEST_DIRECTORY}\\rvkglprobe.exe > {report} 2>&1"
        code, _out, _error = self.step("opengl.run", lambda: self.agent.run(CMD, ["/c", command],
                                                                          self.args.step_timeout))
        self.step("opengl.pull", lambda: self.agent.pull(report, self.work / "opengl.txt"))
        text = (self.work / "opengl.txt").read_text(errors="replace")
        summary = GL_SUMMARY.search(text)
        if summary is None:
            raise StepFailed("opengl", f"no summary in the probe's report, exit {code}; see opengl.txt")
        failed = int(summary.group(2))
        if failed != 0 or code != 0:
            raise StepFailed("opengl", f"{failed} step(s) failed, exit {code}; see opengl.txt")
        steps = dict(GL_STEP.findall(text))
        missing = [name for name in GL_STEPS if name not in steps]
        if missing:
            raise StepFailed("opengl", f"the probe reported no {', '.join(missing)}; see opengl.txt")
        notes = dict(GL_NOTE.findall(text))
        renderer = notes.get("renderer", "")
        if not renderer or renderer == "(none)":
            raise StepFailed("opengl", "the probe named no renderer")
        if any(software in renderer.lower() for software in SOFTWARE_RENDERERS):
            raise StepFailed("opengl", f"the renderer {renderer} is not a host GPU")
        self.say(f"opengl ok: {int(summary.group(1))} passed, renderer {renderer}, "
                 f"version {notes.get('version', '(none)')}")
        self.pictures("opengl", GL_PICTURES)

    def opencl(self):
        """Feature 052, SC-001 to SC-003: every step of rvkclprobe.exe through the
        Khronos OpenCL loader, on a device that is the host GPU's. The report is
        written to a guest file and pulled, as the OpenGL probe's is."""
        report = GUEST_DIRECTORY + "\\opencl.txt"
        command = f"{GUEST_DIRECTORY}\\rvkclprobe.exe > {report} 2>&1"
        code, _out, _error = self.step("opencl.run", lambda: self.agent.run(CMD, ["/c", command],
                                                                          self.args.step_timeout))
        self.step("opencl.pull", lambda: self.agent.pull(report, self.work / "opencl.txt"))
        text = (self.work / "opencl.txt").read_text(errors="replace")
        summary = CL_SUMMARY.search(text)
        if summary is None:
            raise StepFailed("opencl", f"no summary in the probe's report, exit {code}; see opencl.txt")
        failed = int(summary.group(2))
        if failed != 0 or code != 0:
            raise StepFailed("opencl", f"{failed} step(s) failed, exit {code}; see opencl.txt")
        steps = dict(CL_STEP.findall(text))
        missing = [name for name in CL_STEPS if name not in steps]
        if missing:
            raise StepFailed("opencl", f"the probe reported no {', '.join(missing)}; see opencl.txt")
        notes = dict(CL_NOTE.findall(text))
        device = notes.get("device_name", "")
        if not device or device == "(none)":
            raise StepFailed("opencl", "the probe named no device")
        # A software device is the loader answering with a CPU runtime: the
        # guest never reached the host GPU, whatever every step said.
        if any(software in device.lower() for software in SOFTWARE_RENDERERS):
            raise StepFailed("opencl", f"the device {device} is not a host GPU")
        self.say(f"opencl ok: {int(summary.group(1))} passed, device {device}")

    def pictures(self, step, names):
        """The probe's renders into <work>/images: a probe whose steps all
        passed wrote each one, so a missing picture fails the run."""
        images = self.work / "images"
        images.mkdir(exist_ok=True)

        def pull():
            for name in names:
                self.agent.pull(f"{GUEST_DIRECTORY}\\{name}.bmp", images / f"{name}.bmp")

        self.step(f"{step}.images", pull)
        self.say(f"{step}.images ok: {len(names)} in {images}")

    def whole_machine(self):
        """What only a whole guest session answers, and what makes a run long:
        the display, its modes and power, the adapter cycled and upgraded, and
        the machine slept and hibernated."""
        if self.args.display == "egl-headless":
            self.scanout_blob()
        else:
            self.display()
            self.mode_switch()
            self.display_power()
        self.cycle()
        self.upgrade()
        self.suspend()
        self.hibernate()

    def scanout_blob(self):
        """The display scenarios, for a console that can take a dma-buf.

        Every screendump-based check is unavailable here, and not because
        anything failed: a console showing a dma-buf has no DisplaySurface at
        all - `qemu_console_surface` answers NULL for `SCANOUT_DMABUF`
        (`ui/console.c`), so `qmp_screendump` answers "no surface" for this
        scanout as it would for stock virtio-gpu-gl's. What can be read instead
        is the device's own account of it, which is stronger about the thing
        this run is for: that the frame reached the host with no copy.
        """
        shown = "a head shows a guest blob"
        for _attempt in range(POWER_ATTEMPTS):
            if shown in self.qemu_log.read_text(errors="replace"):
                break
            time.sleep(self.args.power_poll)
        else:
            raise StepFailed("scanout", "no head ever showed a blob: the guest kept the 2D path, or the "
                                        "device refused every SET_SCANOUT_BLOB (see qemu.log)")
        log = self.qemu_log.read_text(errors="replace")
        for refusal in ("shows no dma-buf on the head asked for", "makes no dma-buf over a blob's memory"):
            if refusal in log:
                raise StepFailed("scanout", f"the device refused a scanout: {refusal}")
        driver = self.driver_log()
        if "rvkgpu: scanout blob" in driver:
            raise StepFailed("scanout", "the driver could not carry a frame: see rvkgpu.log")
        self.say("scanout ok: a head shows the guest's primary as a dma-buf, and no frame is copied")

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
        # The relay goes with the connection: a held guest's agent socket is free
        # for tools/guest/guest_shell.py and tools/mcp/rvk_inspect.py directly.
        self.relay.stop()
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
        print("  press Enter (or close the guest's window) to power the guest off", flush=True)
        try:
            while self.qemu.poll() is None:
                if select.select([sys.stdin], [], [], 1.0)[0]:
                    sys.stdin.readline()
                    break
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
        elif not self.args.prepare_image and not self.args.no_collect:
            self.collect_evidence(panicked)
        if (failure is not None and self.args.hold) or self.args.keep:
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
        self.relay.stop()
        self.stop_qemu()
        for leftover in ("overlay.qcow2", "vars.fd"):
            (self.work / leftover).unlink(missing_ok=True)

    def run(self):
        self.work.mkdir(parents=True, exist_ok=True)
        watched = not self.args.prepare_image and os.path.isfile(self.args.image)
        before = fingerprint(self.args.image) if watched else None
        failure = None
        try:
            self.relay.start()
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
