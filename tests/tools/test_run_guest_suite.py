"""tools/guest/run_guest_suite.py against a fake QEMU with a scripted guest:
every step, reboot, timeout and misbehaving agent; bugchecks named, their dumps
pulled after a reboot or from a rescue boot; image preparation; holding a guest
for debugging; and the promise that the base image is never written and the
overlay never outlives the run."""
import hashlib
import json
import os
import pathlib
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
import unittest

from tool_run import ROOT

sys.path.insert(0, str(ROOT / "tools" / "guest"))
from run_guest_suite import Screendump, StepFailed, describe_panic, luid_low_part, vulkaninfo_devices  # noqa: E402

sys.path.insert(0, str(ROOT / "tools" / "mcp"))
import rvk_inspect  # noqa: E402

sys.path.insert(0, str(ROOT / "tools"))
from fetch_opencl_loader import CMAKE_CONFIG, WANTED  # noqa: E402

RUNNER = ROOT / "tools" / "guest" / "run_guest_suite.py"
FAKES = ROOT / "tests" / "tools" / "fake_guest"
ONE_MONITOR = "display: dwm=1 services=rvkgpu monitors=1\n"
"""What the check stage says about a guest with one output."""
PASSING_REPORT = "ok adapter.find\nrvktest: 23 passed, 0 failed\n"
SLEEP_STATES = "sleep: standby=yes hibernate=yes\n"
"""What a machine that offers both says (feature 040, US1)."""
COLLECTED = "file C:\\rvk\\diag\\system.txt\r\ndump C:\\Windows\\LiveKernelReports\\WATCHDOG\\WD-1.dmp\r\n"


def live_dump(code):
    """A kernel dump's first bytes as the fake guest serves them, one byte per
    character: DUMP_HEADER64's signature and the bugcheck code it was taken for."""
    head = bytearray(0x40)
    head[:8] = b"PAGEDU64"
    struct.pack_into("<I", head, 0x38, code)
    return bytes(head).decode("latin-1")


BLACK_SCREEN = live_dump(0x1A8)
"""VIDEO_DXGKRNL_BLACK_SCREEN_LIVEDUMP: expected until something renders."""
GUEST_FILES = {"C:\\rvk\\diag\\system.txt": "System events\n",
               "C:\\Windows\\LiveKernelReports\\WATCHDOG\\WD-1.dmp": BLACK_SCREEN,
               "C:\\Windows\\MEMORY.DMP": live_dump(0xD1)}
HOST_VISIBLE_REPORT = "ok adapter.find\nok host_visible.pattern 4096\r\nrvktest: 24 passed, 0 failed\n"
"""What a suite that wrote a pattern through a mapped view reports (feature 041)."""
TIMEOUT_EVENTS = "timeout: 2026-09-13T10:00:00 Display driver rvkgpu stopped responding and has successfully "\
                 "recovered.\r\ntimeouts: 1\r\n"
"""What the guest says event 4101 looks like (feature 040, US3)."""
WITH_MEMORY_DUMP = COLLECTED + "dump C:\\Windows\\MEMORY.DMP\r\n"
ADAPTER_LUID = 0xD3A1
"""The rvkgpu adapter's LUID LowPart in these scripts; vulkaninfo prints the
LUID structure's eight bytes, LowPart first and little-endian."""


VULKAN_PICTURES = ("C:\\rvk\\vulkan-draw.bmp", "C:\\rvk\\vulkan-present.bmp", "C:\\rvk\\vulkan-resized.bmp")
GL_PICTURES = ("opengl-draw", "opengl-share", "opengl-recreated", "opengl-compat")


def vulkaninfo_gpu(index, luid_bytes="a1d30000-00000000", name="rvk (NVIDIA GeForce RTX 4090)"):
    """One physical device as vulkaninfo's full text output prints it
    (Vulkan-Tools DumpGpu and DumpVkPhysicalDeviceIDProperties): a GPUn header,
    then its property structures, the LUID only while deviceLUIDValid."""
    luid = f"\tdeviceLUID      = {luid_bytes}\r\n" if luid_bytes is not None else ""
    return (f"GPU{index}:\r\nVkPhysicalDeviceProperties:\r\n---------------------------\r\n"
            f"\tapiVersion        = 1.3.290 (4206882)\r\n\tdeviceName        = {name}\r\n\r\n"
            "VkPhysicalDeviceIDProperties:\r\n-----------------------------\r\n"
            "\tdeviceUUID      = 7b7ad2c5-4219-8c80-c4fd-a64b8775aedb\r\n"
            "\tdriverUUID      = 414d442d-5749-4e2d-4452-560000000000\r\n"
            f"{luid}\tdeviceNodeMask  = 1\r\n\tdeviceLUIDValid = {'true' if luid_bytes else 'false'}\r\n\r\n")


VULKANINFO_HEAD = ("==========\r\nVULKANINFO\r\n==========\r\n\r\nVulkan Instance Version: 1.4.357\r\n\r\n"
                   "Device Groups:\r\n==============\r\nGroup 0:\r\n\tProperties:\r\n"
                   "\t\tphysicalDevices: count = 1\r\n\t\t\trvk (ID: 0)\r\n\r\n"
                   "Device Properties and Extensions:\r\n=================================\r\n")
VULKANINFO = VULKANINFO_HEAD + vulkaninfo_gpu(0)
PROBE_REPORT = (f"ok vulkan.instance\r\nok vulkan.adapter_luid {ADAPTER_LUID}\r\n"
                f"ok vulkan.physical_device {ADAPTER_LUID}\r\nok vulkan.device\r\nok vulkan.compute\r\n"
                "ok vulkan.draw\r\nok vulkan.lost\r\nok vulkan.recreated\r\nok vulkan.surface 4096\r\n"
                "ok vulkan.present 4096\r\nok vulkan.pacing 60\r\nok vulkan.resized 9216\r\n"
                "rvkvkprobe: 12 passed, 0 failed\r\n")
"""What rvkvkprobe.exe prints when every step of features 045 and 046 passed."""
D3D_REPORTS = {
    "9": ("ok d3d9.device\r\nok d3d9.adapter_ids\r\nok d3d9.clear\r\nok d3d9.readback\r\n"
          "ok d3d9.pixels 4096\r\nok d3d9.present\r\nrvkd3d9probe: 6 passed, 0 failed\r\n"),
    "11": ("ok d3d11.device\r\nok d3d11.feature_level 45056\r\nok d3d11.adapter_luid\r\nok d3d11.target\r\n"
           "ok d3d11.readback\r\nok d3d11.pixels 4096\r\nok d3d11.present\r\n"
           "rvkd3d11probe: 7 passed, 0 failed\r\n"),
    "12": ("ok d3d12.device\r\nok d3d12.feature_level 49152\r\nok d3d12.adapter_luid\r\nok d3d12.queue\r\n"
           "ok d3d12.target\r\nok d3d12.clear\r\nok d3d12.fence\r\nok d3d12.readback\r\n"
           "ok d3d12.pixels 4096\r\nrvkd3d12probe: 9 passed, 0 failed\r\n"),
}
"""What each D3D probe prints when every step of feature 047's US1 and US2 passed."""
D3D_PICTURES = ("C:\\rvk\\d3d9-clear.bmp", "C:\\rvk\\d3d11-clear.bmp", "C:\\rvk\\d3d12-clear.bmp")
KEY_ABSENT = "ERROR: The system was unable to find the specified registry key or value.\r\n"
ICD_LOG = "C:\\ProgramData\\rvk\\rvkvk-4242.log"


class GuestSuiteRun(unittest.TestCase):
    def setUp(self):
        self.scratch = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)
        self.image = self.scratch / "windows.qcow2"
        self.image.write_bytes(b"prepared image")
        self.os_stat = os.stat(self.image)
        for name in ("rvkgpu.sys", "rvkgpu.pdb", "rvkvk.dll", "rvkvk.json", "rvktest.exe", "rvkvkprobe.exe",
                     "rvkinspect.exe", "rvkgl.dll", "rvkglprobe.exe", "rvkcl.dll", "rvkclprobe.exe",
                     "OpenCL.dll", "rvkd3d9probe.exe", "rvkd3d11probe.exe",
                     "rvkd3d12probe.exe", "OVMF_VARS.fd", "OVMF_CODE.fd"):
            (self.scratch / name).write_bytes(name.encode() * 100)
        self.work = self.scratch / "work"
        self.journal = self.scratch / "journal.txt"
        self.environment = dict(os.environ, FAKE_GUEST_JOURNAL=str(self.journal),
                                FAKE_GUEST_SCRIPT=str(self.scratch / "script.json"))

    def script(self, **behaviour):
        behaviour.setdefault("stages", {"sign": [3010], "install": [3010], "check": [0]})
        behaviour.setdefault("outputs", {"check": ONE_MONITOR +
                                                  "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                                         "sleepstates": SLEEP_STATES,
                                         "rvktest": PASSING_REPORT, "collect": COLLECTED})
        behaviour.setdefault("files", GUEST_FILES)
        (self.scratch / "script.json").write_text(json.dumps(behaviour))

    def run_suite(self, *extra, environment=None, stdin=""):
        arguments = ["--image", self.image, "--work", self.work, "--power-poll", "0.1",
                     "--qemu", FAKES / "fake_qemu.py",
                     "--qemu-img", FAKES / "fake_qemu_img.py", "--executor", "/nowhere/librvk_echo_executor.so",
                     "--driver", self.scratch / "rvkgpu.sys", "--test", self.scratch / "rvktest.exe",
                     "--inspect", self.scratch / "rvkinspect.exe",
                     "--icd", self.scratch / "rvkvk.dll", "--icd-manifest", self.scratch / "rvkvk.json",
                     "--gl-icd", self.scratch / "rvkgl.dll", "--cl-icd", self.scratch / "rvkcl.dll",
                     "--firmware", self.scratch / "OVMF_CODE.fd", "--firmware-vars", self.scratch / "OVMF_VARS.fd",
                     "--boot-timeout", "10", "--step-timeout", "5", "--agent-timeout", "2", *extra]
        return subprocess.run([sys.executable, "-B", str(RUNNER), *map(str, arguments)], capture_output=True,
                              text=True, env=environment or self.environment, input=stdin, timeout=120)

    def journal_lines(self):
        return self.journal.read_text().splitlines() if self.journal.exists() else []

    def stage_calls(self):
        return [line.split("-Stage ", 1)[1] for line in self.journal_lines() if "-Stage" in line]

    def assert_clean(self):
        self.assertFalse((self.work / "overlay.qcow2").exists())
        self.assertFalse((self.work / "vars.fd").exists())
        after = os.stat(self.image)
        self.assertEqual((after.st_size, after.st_mtime_ns), (self.os_stat.st_size, self.os_stat.st_mtime_ns))
        self.assertTrue((self.work / "verdict.txt").exists())

    def test_a_passing_guest_runs_every_step_through_both_reboots_then_collects(self):
        self.script()
        result = self.run_suite()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        lines = self.journal_lines()
        argv = lines[0]
        self.assertIn("virtio-gpu-rvk-pci,id=gpu,host-gpu=auto,executor=/nowhere/librvk_echo_executor.so,"
                      "max_outputs=1,vectors=11", argv)
        self.assertIn("isa-debugcon,iobase=0x402,chardev=debugcon", argv)
        self.assertIn("name=org.qemu.guest_agent.0", argv)
        self.assertIn(",hv-crash", argv)
        self.assertIn("-action panic=none", argv)
        self.assertIn("-msg timestamp=on", argv)
        self.assertNotIn("-gdb", argv)
        events = [line.split(" ")[0] if line.startswith(("exec", "file", "pull")) else line for line in lines[1:]]
        self.assertEqual(self.stage_calls(),
                         ["sign", "install", "check", "modes", "setmode -Width 1024 -Height 768",
                          "setmode -Width 2560 -Height 1600", "setmode -Width 1280 -Height 800",
                          "monitortimeout -Minutes 1", "monitortimeout -Minutes 0",
                          "sleepstates", "check", "collect"])
        self.assertEqual(events.count("file"), 9)
        self.assertEqual(events.count("pull"), 2)
        self.assertIn("file C:\\rvk\\package\\rvkgpu.sys 1000", lines)
        self.assertIn("file C:\\rvk\\package\\rvkgpu.inf", "\n".join(lines))
        # The INF names the ICD whatever the run is for, and pnputil refuses a
        # package missing a file it names (feature 045).
        self.assertIn("file C:\\rvk\\package\\rvkvk.dll 900", lines)
        self.assertIn("file C:\\rvk\\package\\rvkvk.json 1000", lines)
        # The OpenGL ICD is in the package too, which names it (feature 050).
        self.assertIn("file C:\\rvk\\package\\rvkgl.dll 900", lines)
        # The inspector is in every run, so a hung or crashed program can be
        # read from the host whatever the run is for (docs/trace.md).
        self.assertIn("file C:\\rvk\\rvkinspect.exe 1400", lines)
        self.assertNotIn("vulkaninfo", "\n".join(lines))
        order = [line for line in lines
                 if line in ("reboot", "powerdown") or (line.startswith("exec") and ("-Stage" in line or "rvktest" in line))]
        self.assertEqual([o if o in ("reboot", "powerdown") else o.split()[-1] for o in order],
                         ["sign", "reboot", "install", "reboot", "check", "modes", "768", "1600", "800",
                          "1", "0", "sleepstates", "check", "C:\\rvk\\rvktest.exe", "collect", "powerdown"])
        self.assertEqual((self.work / "report.txt").read_text(), PASSING_REPORT)
        self.assertIn("service=rvkgpu", (self.work / "device.txt").read_text())
        self.assertIn("rvkgpu: DriverEntry", (self.work / "rvkgpu.log").read_text())
        self.assertEqual((self.work / "diag" / "system.txt").read_text(), "System events\n")
        self.assertEqual((self.work / "diag" / "dumps" / "WD-1.dmp").read_bytes(), BLACK_SCREEN.encode("latin-1"))
        self.assertIn("guest: dumps ok: 1 live dump(s), every one VIDEO_DXGKRNL_BLACK_SCREEN_LIVEDUMP", result.stdout)
        self.assertEqual(sorted(p.name for p in (self.work / "symbols").iterdir()), ["rvkgpu.pdb", "rvkgpu.sys"])
        self.assertIn("guest: report ok: 23 passed", result.stdout)
        self.assertIn("guest: vsync ok: 60.00 blanks a second", result.stdout)
        self.assertIn("guest: collect: 2 files in", result.stdout)
        self.assertFalse((self.work / "panic.txt").exists())
        self.assert_clean()

    def test_the_cycle_scenario_disables_and_enables_the_adapter(self):
        """Feature 040 US2: what pnputil does to an adapter, N times, and the
        adapter back each time."""
        self.script(stages={"sign": [3010], "install": [3010], "check": [0], "cycle": [0]},
                    outputs={"check": ONE_MONITOR, "sleepstates": SLEEP_STATES,
                             # The guest ends its lines the Windows way.
                             "cycle": "cycle: 50 done\r\npool: Current Pool Allocations: 0\r\n",
                             "rvktest": PASSING_REPORT, "collect": COLLECTED})
        result = self.run_suite("--power-cycles", "50")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("cycle -Minutes 50", self.stage_calls())
        self.assertIn("guest: cycle ok: 50 disable and enable cycles", result.stdout)
        self.assertIn("Current Pool Allocations", (self.work / "cycle.txt").read_text())
        self.assert_clean()

    def test_the_upgrade_scenario_installs_a_higher_version_without_a_reboot(self):
        """Feature 040 US2: a package that arrives over the running one."""
        self.script(stages={"sign": [3010], "install": [3010], "check": [0], "upgrade": [0]},
                    outputs={"check": ONE_MONITOR, "sleepstates": SLEEP_STATES,
                             "upgrade": "upgrade: installed 0.35.1.0 running 0.35.1.0 problem 0\r\n",
                             "rvktest": PASSING_REPORT, "collect": COLLECTED})
        result = self.run_suite("--upgrades", "1")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("upgrade -Minutes 1", self.stage_calls())
        self.assertIn("guest: upgrade ok: version 0.35.1.0 running, no reboot", result.stdout)
        self.assertNotIn("reboot", self.journal_lines()[-3:])
        self.assert_clean()

    def test_an_upgrade_the_adapter_did_not_take_fails_the_run(self):
        self.script(stages={"sign": [3010], "install": [3010], "check": [0], "upgrade": [1]},
                    outputs={"check": ONE_MONITOR, "sleepstates": SLEEP_STATES,
                             "upgrade": "upgrade: installed 0.35.1.0 running 0.35.0.0 problem 0\r\n",
                             "rvktest": PASSING_REPORT, "collect": COLLECTED})
        result = self.run_suite("--upgrades", "1")
        self.assertEqual(result.returncode, 1)
        self.assertIn("upgrade failed", result.stdout)
        self.assert_clean()

    def test_a_cycle_the_guest_could_not_finish_fails_the_run(self):
        self.script(stages={"sign": [3010], "install": [3010], "check": [0], "cycle": [1]},
                    outputs={"check": ONE_MONITOR, "sleepstates": SLEEP_STATES,
                             "cycle": "cycle: 7 of 50 left problem 43\n",
                             "rvktest": PASSING_REPORT, "collect": COLLECTED})
        result = self.run_suite("--power-cycles", "50")
        self.assertEqual(result.returncode, 1)
        self.assertIn("cycle failed", result.stdout)
        self.assert_clean()

    def test_the_executor_controls_reach_qemus_environment(self):
        """Feature 040 US3: a real TDR needs the host to stall, and the test
        executor is told to from outside - no rebuild, no test-only command in
        the protocol (FR-006)."""
        self.script(stages={"sign": [3010], "install": [3010], "check": [0], "timeouts": [0]},
                    outputs={"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                             "rvktest": PASSING_REPORT, "collect": COLLECTED, "timeouts": TIMEOUT_EVENTS})
        result = self.run_suite("--stall-ms", "5000", "--fault-after", "3", "--stall-context", "2")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        environment = json.loads((self.work / "environment.json").read_text())
        self.assertEqual(environment.get("RVK_ECHO_STALL_MS"), "5000")
        self.assertEqual(environment.get("RVK_ECHO_FAULT_AFTER"), "3")
        self.assertEqual(environment.get("RVK_ECHO_ONLY_NTH"), "2")
        self.assert_clean()

    def test_s3_suspends_through_the_agent_and_wakes_through_qmp(self):
        """Feature 040 US1: the adapter is still there after the guest slept."""
        self.script()
        result = self.run_suite()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        journal = self.journal_lines()
        self.assertIn("guest-suspend-ram", journal)
        self.assertIn("system_wakeup", journal)
        self.assertIn("guest: suspend ok: the adapter came back from S3", result.stdout)
        self.assertTrue((self.work / "device-after-suspend.txt").exists())
        self.assert_clean()

    def test_a_machine_that_offers_no_s3_is_skipped_with_its_reason(self):
        self.script(outputs={"check": ONE_MONITOR, "sleepstates": "sleep: standby=no hibernate=no\n",
                             "rvktest": PASSING_REPORT, "collect": COLLECTED})
        result = self.run_suite("--hibernate")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("guest: suspend skipped: this machine offers no S3", result.stdout)
        self.assertIn("guest: hibernate skipped: this machine offers no S4", result.stdout)
        self.assertNotIn("guest-suspend-ram", self.journal_lines())
        self.assert_clean()

    def test_s4_leaves_the_machine_off_and_the_overlay_boots_again(self):
        """The sleep states are asked again after the stage turns hibernation
        on: what the guest said before it is no longer what it would say."""
        self.script(stages={"sign": [3010], "install": [3010], "check": [0], "hibernate": [0],
                            "sleepstates": [0]})
        result = self.run_suite("--hibernate")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        journal = self.journal_lines()
        self.assertIn("guest-suspend-disk", journal)
        self.assertIn("guest: hibernate ok: the adapter came back from S4, 1 of 1", result.stdout)
        self.assertIn("hibernate", " ".join(self.stage_calls()))
        self.assertEqual(self.stage_calls().count("sleepstates"), 2)
        self.assert_clean()

    def test_every_resume_runs_the_suite_again(self):
        """SC-001 asks for five S4 resumes with a passing rvktest, not five
        machines that come back: the suite runs after each one."""
        self.script(stages={"sign": [3010], "install": [3010], "check": [0], "hibernate": [0],
                            "sleepstates": [0]})
        result = self.run_suite("--hibernate", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.journal_lines().count("guest-suspend-disk"), 3)
        for cycle in (1, 2, 3):
            self.assertIn(f"guest: hibernate ok: the adapter came back from S4, {cycle} of 3", result.stdout)
        # Once after each resume, and once for the run itself.
        self.assertEqual(result.stdout.count("guest: report ok: 23 passed"), 4)
        self.assert_clean()

    def test_an_rvk_run_asks_for_the_rvk_scenarios_and_records_its_executor_and_latency(self):
        """Feature 044: the rvk scenarios run instead of the suite, and the verdict
        says which executor did the work and what a no-op cost."""
        report = "ok transport.nop\nok transport.latency 812\nok vulkan.instance\nrvktest: 12 passed, 0 failed\n"
        self.script(outputs={"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                             "rvktest": report, "collect": COLLECTED})
        result = self.run_suite("--rvk")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("guest: rvk: executor /nowhere/librvk_echo_executor.so, host-gpu auto", result.stdout)
        self.assertIn("guest: rvk ok: 12 passed", result.stdout)
        self.assertIn("guest: rvk latency: a transport no-op round trip took 812 us", result.stdout)
        self.assertNotIn("guest: report ok", result.stdout)
        self.assertNotIn("guest: display ok", result.stdout)
        self.assertIn("rvktest.exe rvk", self.journal.read_text())
        self.assertIn("rvk latency", (self.work / "verdict.txt").read_text())

    def test_an_rvk_run_on_a_named_host_gpu_binds_the_device_to_it_and_says_so(self):
        """Feature 045, T020: the host-visible import is proven on each host GPU,
        so a run can name the one the device binds to; auto stays the default."""
        report = "ok transport.nop\nrvktest: 2 passed, 0 failed\n"
        self.script(outputs={"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                             "rvktest": report, "collect": COLLECTED})
        uuid = "7b7ad2c5-4219-8c80-c4fd-a64b8775aedb"
        result = self.run_suite("--rvk", "--host-gpu", uuid)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(f"host-gpu={uuid}", self.journal.read_text())
        self.assertIn(f"guest: rvk: executor /nowhere/librvk_echo_executor.so, host-gpu {uuid}", result.stdout)

    def test_a_quick_run_installs_checks_and_runs_the_suite_without_the_slow_scenarios(self):
        """Feature 044: an iteration run leaves the display, power and sleep
        scenarios - minutes of waiting - to the gate."""
        self.script()
        result = self.run_suite("--quick")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("guest: quick: the display, power, cycle, upgrade, sleep and hibernate scenarios", result.stdout)
        self.assertIn("guest: report ok: 23 passed", result.stdout)
        self.assertNotIn("guest: display ok", result.stdout)
        self.assertNotIn("guest: monitortimeout", result.stdout)
        self.assertNotIn("guest: sleepstates", result.stdout)

    def test_an_rvk_run_whose_no_op_is_past_the_threshold_fails(self):
        """SC-002: a transport that has stopped being interrupt-driven passes every
        step and is still a regression."""
        report = "ok transport.nop\nok transport.latency 60000\nrvktest: 2 passed, 0 failed\n"
        self.script(outputs={"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                             "rvktest": report, "collect": COLLECTED})
        result = self.run_suite("--rvk")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("guest: rvk failed: a no-op took 60000 us, past the 50000 us threshold", result.stdout)

    def rvk_run(self, *extra, median="ok transport.warm_median 640\n"):
        """An rvk run whose transport scenario timed its warm rounds."""
        report = "ok transport.nop\nok transport.latency 812\n" + median + "rvktest: 12 passed, 0 failed\n"
        self.script(outputs={"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                             "rvktest": report, "collect": COLLECTED})
        return self.run_suite("--rvk", *extra)

    def test_two_rvk_runs_make_the_cost_of_a_recording_a_number(self):
        """SC-002: the first boot can only keep its own round trip, and the one
        that completes the pair is the one that says what a recording cost."""
        first = self.rvk_run()
        self.assertEqual(first.returncode, 0, first.stdout + first.stderr)
        self.assertIn("guest: recording cost: a no-op round trip took 640 us with recording off; the same run "
                      f"with --record completes SC-002's pair in {self.work / 'recording-cost.txt'}", first.stdout)
        second = self.rvk_run("--record", median="ok transport.warm_median 712\n")
        self.assertEqual(second.returncode, 0, second.stdout + second.stderr)
        self.assertIn("guest: recording cost ok: a no-op round trip took 640 us with recording off and 712 us "
                      "with it on; a recording costs 72 us", second.stdout)
        self.assertIn("off 640\non 712\n", (self.work / "recording-cost.txt").read_text())
        self.assertIn("recording cost ok", (self.work / "verdict.txt").read_text())
        self.assert_clean()

    def test_a_recording_no_slower_than_no_recording_is_not_called_a_cost(self):
        """The two boots are minutes apart on a machine doing other things, so
        the run with the recording on comes out faster often enough that
        printing a negative cost would be printing noise as a measurement."""
        self.assertEqual(self.rvk_run("--record", median="ok transport.warm_median 640\n").returncode, 0)
        result = self.rvk_run(median="ok transport.warm_median 700\n")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("guest: recording cost ok: a no-op round trip took 700 us with recording off and 640 us "
                      "with it on; a recording costs nothing this pair could measure", result.stdout)

    def test_an_rvk_run_that_timed_no_warm_round_trip_measures_no_cost(self):
        """A run whose warm rounds failed reported a latency and nothing to
        compare; it must not keep the cold round trip as if it were one."""
        result = self.rvk_run(median="")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("guest: recording cost skipped: the rvk run timed no warm round trip", result.stdout)
        self.assertFalse((self.work / "recording-cost.txt").exists())

    def test_a_cost_record_that_is_not_two_round_trips_fails_the_run(self):
        """A hand-edited record is evidence of something; a run that silently
        overwrote it would take the comparison with it."""
        self.work.mkdir(parents=True, exist_ok=True)
        (self.work / "recording-cost.txt").write_text("off 640\noff 700\n")
        result = self.rvk_run()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("guest: recording.cost failed", result.stdout)
        self.assertIn("a second `off`; the record holds one of each", result.stdout)
        self.assertEqual((self.work / "recording-cost.txt").read_text(), "off 640\noff 700\n")

    def test_an_rvk_run_with_a_failed_step_fails(self):
        self.script(outputs={"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                             "rvktest": "FAIL opencl.sum status=4 63\nrvktest: 11 passed, 1 failed\n",
                             "collect": COLLECTED}, rvktest=1)
        result = self.run_suite("--rvk")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("guest: rvk failed: 1 step(s) failed, exit 1; see rvk.txt", result.stdout)

    def test_an_rvk_run_whose_program_prints_no_summary_fails(self):
        self.script(outputs={"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                             "rvktest": "", "collect": COLLECTED})
        result = self.run_suite("--rvk")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("guest: rvk failed: no summary line", result.stdout)

    def vulkan_runtime(self, **contents):
        """A fetched runtime and the pins it was fetched by, as
        tools/fetch_vulkan_runtime.py leaves them; `contents` replaces a file's
        bytes after pinning, or with None leaves it out."""
        runtime = self.scratch / "vulkan-runtime"
        runtime.mkdir(exist_ok=True)
        pins = ["archive file:///nowhere/runtime.zip " + "0" * 64]
        for name in ("vulkan-1.dll", "vulkaninfo.exe"):
            pinned = name.encode() * 50
            pins.append(f"{name} x64/{name} {hashlib.sha256(pinned).hexdigest()}")
            replaced = contents.get(name.replace("-", "_").replace(".", "_"), pinned)
            if replaced is not None:
                (runtime / name).write_bytes(replaced)
        (self.scratch / "pins.txt").write_text("\n".join(pins) + "\n")
        return ("--vulkan-runtime", runtime, "--vulkan-pins", self.scratch / "pins.txt",
                "--probe", self.scratch / "rvkvkprobe.exe")

    def vulkan_script(self, **behaviour):
        outputs = {"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                   "reg": KEY_ABSENT, "vulkaninfo": VULKANINFO, "rvkvkprobe": PROBE_REPORT,
                   "collect": COLLECTED + f"file {ICD_LOG}\r\n"}
        outputs.update(behaviour.pop("outputs", {}))
        exits = {"reg": 1}
        exits.update(behaviour.pop("exits", {}))
        pictures = behaviour.pop("pictures", {name: "BM vulkan" for name in VULKAN_PICTURES})
        self.script(outputs=outputs, files=dict(GUEST_FILES, **{ICD_LOG: "rvkvk: loader interface negotiation\r\n"},
                                                **pictures),
                    **exits, **behaviour)

    def test_a_probe_needs_a_console_session_and_a_task_that_finishes(self):
        """Feature 046: the probe's present steps make a window, which only the
        session a person is logged into can show. A machine no one is logged
        into has nowhere to run it, and a task that never finishes is a failure
        naming itself rather than a run that hangs."""
        self.vulkan_script(console_user="")
        nobody = self.run_suite("--vulkan", "--executor", "/nowhere/librvk_executor.so", *self.vulkan_runtime())
        self.assertEqual(nobody.returncode, 1)
        self.assertIn("guest: console failed: no one is logged into the console session", nobody.stdout)
        self.assertNotIn("rvkconsole", "".join(self.journal_lines()))
        self.assert_clean()
        self.vulkan_script(console_running=1000)
        stuck = self.run_suite("--vulkan", "--executor", "/nowhere/librvk_executor.so", *self.vulkan_runtime(),
                               "--step-timeout", "1")
        self.assertEqual(stuck.returncode, 1)
        self.assertIn("guest: rvkvkprobe failed: rvkconsole was still running after 1.0 s", stuck.stdout)
        # Whatever it answered, the task is not left behind on the machine.
        self.assertIn("exec C:\\Windows\\System32\\schtasks.exe /delete /tn rvkconsole /f", self.journal_lines())
        self.assert_clean()

    def test_a_vulkan_run_finds_the_icd_through_the_adapter_alone_and_runs_the_probe(self):
        """Feature 045, SC-001 to SC-003: no global Khronos key, vulkaninfo lists
        exactly the one device whose LUID is the adapter's, the probe's steps
        all pass, and the ICD's log is in diag/."""
        self.vulkan_script()
        result = self.run_suite("--vulkan", "--executor", "/nowhere/librvk_executor.so", *self.vulkan_runtime())
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        lines = self.journal_lines()
        for pushed in ("C:\\rvk\\vulkan-1.dll 600", "C:\\rvk\\vulkaninfo.exe 700", "C:\\rvk\\rvkvkprobe.exe 1400",
                       "C:\\rvk\\package\\rvkvk.dll 900", "C:\\rvk\\package\\rvkvk.json 1000"):
            self.assertIn(f"file {pushed}", lines)
        programs = [line.split()[1] for line in lines if line.startswith("exec") and "powershell" not in line]
        self.assertEqual(programs, ["C:\\Windows\\System32\\reg.exe", "C:\\Windows\\System32\\cmd.exe",
                                    "C:\\rvk\\vulkaninfo.exe",
                                    "C:\\Windows\\System32\\schtasks.exe", "C:\\Windows\\System32\\schtasks.exe",
                                    "C:\\rvk\\rvkvkprobe.exe", "C:\\Windows\\System32\\schtasks.exe",
                                    "C:\\Windows\\System32\\schtasks.exe"])
        # The probe's window has to be made where a window can be seen: the
        # agent's own session has no visible desktop, so the probe runs in the
        # console session through a scheduled task with /it (feature 046).
        self.assertIn("file C:\\rvk\\console.cmd 59", lines)
        self.assertIn("exec C:\\Windows\\System32\\schtasks.exe /create /tn rvkconsole /tr C:\\rvk\\console.cmd "
                      "/sc once /st 23:59 /ru guest\\tester /it /f", lines)
        self.assertIn("exec C:\\Windows\\System32\\schtasks.exe /run /tn rvkconsole", lines)
        self.assertIn("exec C:\\Windows\\System32\\schtasks.exe /delete /tn rvkconsole /f", lines)
        self.assertIn("pull C:\\rvk\\vulkan.txt", lines)
        # vulkaninfo's full listing is written to a guest file and pulled: the
        # agent's captured output stalled the program on a live guest.
        self.assertIn("exec C:\\Windows\\System32\\cmd.exe /c C:\\rvk\\vulkaninfo.exe "
                      "> C:\\rvk\\vulkaninfo.txt 2>&1", lines)
        self.assertIn("pull C:\\rvk\\vulkaninfo.txt", lines)
        self.assertIn("exec C:\\Windows\\System32\\reg.exe query HKLM\\SOFTWARE\\Khronos\\Vulkan\\Drivers", lines)
        self.assertEqual(self.stage_calls(), ["sign", "install", "check", "collect"])
        self.assertIn("guest: vulkan: executor /nowhere/librvk_executor.so, host-gpu auto", result.stdout)
        self.assertIn("guest: khronos ok: no HKLM\\SOFTWARE\\Khronos\\Vulkan\\Drivers key", result.stdout)
        self.assertIn(f"guest: vulkaninfo ok: one physical device, LUID LowPart {ADAPTER_LUID}", result.stdout)
        self.assertIn("guest: vulkan ok: 12 passed", result.stdout)
        self.assertIn(f"guest: luid ok: the adapter, the probe's physical device and vulkaninfo's all "
                      f"{ADAPTER_LUID}", result.stdout)
        self.assertNotIn("guest: report ok", result.stdout)
        self.assertEqual((self.work / "vulkaninfo.txt").read_text(newline=""), VULKANINFO)
        self.assertEqual((self.work / "vulkan.txt").read_text(newline=""), PROBE_REPORT)
        self.assertEqual((self.work / "images" / "vulkan-draw.bmp").read_text(), "BM vulkan")
        # The window-system steps' pictures come back too (feature 046).
        self.assertEqual((self.work / "images" / "vulkan-present.bmp").read_text(), "BM vulkan")
        self.assertEqual((self.work / "images" / "vulkan-resized.bmp").read_text(), "BM vulkan")
        self.assertIn(f"guest: vulkan.images ok: 3 in {self.work / 'images'}", result.stdout)
        self.assertEqual((self.work / "diag" / "rvkvk-4242.log").read_text(newline=""),
                         "rvkvk: loader interface negotiation\r\n")
        self.assertIn("vulkaninfo ok", (self.work / "verdict.txt").read_text())
        self.assert_clean()

    def test_a_khronos_key_that_lists_no_rvk_driver_is_left_alone(self):
        listed = ("\r\nHKEY_LOCAL_MACHINE\\SOFTWARE\\Khronos\\Vulkan\\Drivers\r\n"
                  "    C:\\Windows\\System32\\DriverStore\\FileRepository\\nv_dispi.inf_amd64\\nv-vk64.json"
                  "    REG_DWORD    0x0\r\n")
        self.vulkan_script(outputs={"reg": listed}, exits={"reg": 0})
        result = self.run_suite("--vulkan", *self.vulkan_runtime())
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("guest: khronos ok: the key lists no rvk driver", result.stdout)

    def test_an_rvk_driver_in_the_global_khronos_key_fails_before_vulkaninfo_runs(self):
        """FR-001: a global value would make the loader list the device twice,
        and would hide whether the adapter's own value works."""
        listed = ("\r\nHKEY_LOCAL_MACHINE\\SOFTWARE\\Khronos\\Vulkan\\Drivers\r\n"
                  "    C:\\Windows\\System32\\DriverStore\\FileRepository\\rvkgpu.inf_amd64_1\\rvkvk.json"
                  "    REG_DWORD    0x0\r\n")
        self.vulkan_script(outputs={"reg": listed}, exits={"reg": 0})
        result = self.run_suite("--vulkan", *self.vulkan_runtime())
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("guest: khronos failed: the global key lists C:\\Windows\\System32\\DriverStore\\"
                      "FileRepository\\rvkgpu.inf_amd64_1\\rvkvk.json", result.stdout)
        self.assertNotIn("vulkaninfo.exe", "\n".join(line for line in self.journal_lines() if line.startswith("exec")))
        self.assertIn("collect", self.stage_calls())

    def test_a_registry_query_that_fails_for_another_reason_is_not_taken_for_an_absent_key(self):
        self.vulkan_script(outputs={"reg": "ERROR: Invalid syntax.\r\nType \"REG QUERY /?\" for usage.\r\n"})
        result = self.run_suite("--vulkan", *self.vulkan_runtime())
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("guest: khronos failed: reg query exited 1: ERROR: Invalid syntax.", result.stdout)

    def test_vulkaninfo_must_list_exactly_one_physical_device_with_the_adapter_s_luid(self):
        cases = {
            "two": (VULKANINFO + vulkaninfo_gpu(1, "0b5e0100-00000000", "llvmpipe"), 0,
                    "vulkaninfo failed: 2 physical devices listed, not one: GPU0 LUID LowPart 54177, "
                    "GPU1 LUID LowPart 89611"),
            "none": (VULKANINFO_HEAD, 0, "vulkaninfo failed: 0 physical devices listed, not one"),
            "invalid": (VULKANINFO_HEAD + vulkaninfo_gpu(0, None), 0,
                        "vulkaninfo failed: GPU0 has no valid deviceLUID"),
            "other": (VULKANINFO_HEAD + vulkaninfo_gpu(0, "a2d30000-00000000"), 0,
                      "luid failed: vulkaninfo's physical device has LUID LowPart 54178, not the adapter's 54177"),
            "unreadable": (VULKANINFO_HEAD + vulkaninfo_gpu(0, "a1d3"), 0,
                           "vulkaninfo failed: GPU0 has a deviceLUID that is not eight bytes: a1d3"),
            "exit": (VULKANINFO, 1, "vulkaninfo failed: exited 1; see vulkaninfo.txt"),
        }
        for case, (listing, code, reason) in cases.items():
            with self.subTest(case):
                self.vulkan_script(outputs={"vulkaninfo": listing}, exits={"vulkaninfo": code})
                result = self.run_suite("--vulkan", *self.vulkan_runtime())
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(f"guest: {reason}", result.stdout)
                self.assertEqual((self.work / "vulkaninfo.txt").read_text(newline=""), listing)

    def test_the_probe_s_report_is_judged_step_by_step(self):
        cases = {
            "failed": (PROBE_REPORT.replace("ok vulkan.draw", "FAIL vulkan.draw VK_ERROR_DEVICE_LOST")
                       .replace("12 passed, 0", "11 passed, 1"), 1,
                       "vulkan failed: 1 step(s) failed, exit 1; see vulkan.txt"),
            "exit": (PROBE_REPORT, 3, "vulkan failed: 0 step(s) failed, exit 3; see vulkan.txt"),
            "summary": (PROBE_REPORT.replace("rvkvkprobe: 12 passed, 0 failed\r\n", ""), 0,
                        "vulkan failed: no summary line (exit 0)"),
            "skipped": (PROBE_REPORT.replace("ok vulkan.lost\r\nok vulkan.recreated\r\n", "")
                        .replace("12 passed", "10 passed"), 0,
                        "vulkan failed: the probe reported no lost, recreated; see vulkan.txt"),
            "no present": (PROBE_REPORT.replace("ok vulkan.present 4096\r\nok vulkan.pacing 60\r\n", "")
                           .replace("12 passed", "10 passed"), 0,
                           "vulkan failed: the probe reported no present, pacing; see vulkan.txt"),
            "rvktest": (PROBE_REPORT.replace("rvkvkprobe:", "rvktest:"), 0, "vulkan failed: no summary line"),
            "mismatch": (PROBE_REPORT.replace(f"physical_device {ADAPTER_LUID}", "physical_device 7"), 0,
                         f"luid failed: the probe's physical device has LUID LowPart 7, not the adapter's "
                         f"{ADAPTER_LUID}"),
            "no value": (PROBE_REPORT.replace(f"adapter_luid {ADAPTER_LUID}", "adapter_luid"), 0,
                         "luid failed: the probe printed adapter_luid '', not a decimal LowPart"),
        }
        for case, (report, code, reason) in cases.items():
            with self.subTest(case):
                self.vulkan_script(outputs={"rvkvkprobe": report}, exits={"rvkvkprobe": code})
                result = self.run_suite("--vulkan", *self.vulkan_runtime())
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(f"guest: {reason}", result.stdout)
                self.assertEqual((self.work / "vulkan.txt").read_text(newline=""), report)

    DISCOVERY = ("discovery register_class ok value=0x000000000000C0DE error=0\r\n"
                 "discovery open_adapter_from_hdc ok value=0x0000000000006431 error=0\r\n"
                 "discovery set_pixel_format ok value=0x0000000000000001 error=0\r\n"
                 "discovery rvkgl_loaded yes value=0x0000000000000000 error=0\r\n"
                 "discovery loaded rvkgl.dll value=0x00007FFA10000000 error=0\r\n")

    def discovery_script(self, **behaviour):
        outputs = {"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                   "rvkglprobe": self.DISCOVERY,
                   "collect": COLLECTED + "file C:\\ProgramData\\rvk\\rvkgl-4242.log\r\n"}
        outputs.update(behaviour.pop("outputs", {}))
        self.script(outputs=outputs,
                    files=dict(GUEST_FILES, **{"C:\\ProgramData\\rvk\\rvkgl-4242.log":
                                               "DrvValidateVersion 0x0000000000000002\r\n"}),
                    **behaviour)

    def test_an_opengl_discovery_run_records_what_opengl32_did_and_the_icd_log(self):
        """Feature 050, T008: the probe runs through the system opengl32.dll with
        its report written to a guest file and pulled; the run records which ICD
        loaded and the adapter the window's DC opened, and keeps rvkgl's log."""
        self.discovery_script()
        result = self.run_suite("--opengl-discovery", "--gl-probe", self.scratch / "rvkglprobe.exe")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        lines = self.journal_lines()
        self.assertIn("file C:\\rvk\\rvkglprobe.exe 1400", lines)
        self.assertIn("file C:\\rvk\\package\\rvkgl.dll 900", lines)
        self.assertIn("exec C:\\Windows\\System32\\cmd.exe /c C:\\rvk\\rvkglprobe.exe --discovery "
                      "> C:\\rvk\\discovery.txt 2>&1", lines)
        self.assertIn("pull C:\\rvk\\discovery.txt", lines)
        self.assertEqual(self.stage_calls(), ["sign", "install", "check", "collect"])
        self.assertEqual((self.work / "discovery.txt").read_text(newline=""), self.DISCOVERY)
        self.assertIn("guest: discovery ok: rvkgl.dll loaded, the DC opened an adapter (LUID 0x6431)", result.stdout)
        self.assertEqual((self.work / "diag" / "rvkgl-4242.log").read_text(newline=""),
                         "DrvValidateVersion 0x0000000000000002\r\n")
        self.assertNotIn("rvkvkprobe", "\n".join(lines))
        self.assert_clean()

    def test_an_opengl_discovery_run_reports_another_icd_or_a_refused_open_and_fails_only_without_a_report(self):
        other = ("discovery open_adapter_from_hdc failed value=0x0000000000000000 error=3221225485\r\n"
                 "discovery rvkgl_loaded no value=0x0000000000000000 error=0\r\n")
        self.discovery_script(outputs={"rvkglprobe": other})
        result = self.run_suite("--opengl-discovery", "--gl-probe", self.scratch / "rvkglprobe.exe")
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("guest: discovery ok: rvkgl.dll not loaded, the DC opened no adapter (status 0xC000000D)",
                      result.stdout)
        self.discovery_script(outputs={"rvkglprobe": ""})
        silent = self.run_suite("--opengl-discovery", "--gl-probe", self.scratch / "rvkglprobe.exe")
        self.assertEqual(silent.returncode, 1)
        self.assertIn("guest: discovery failed: the probe reported no rvkgl_loaded line", silent.stdout)
        self.discovery_script(rvkglprobe=1)
        failed = self.run_suite("--opengl-discovery", "--gl-probe", self.scratch / "rvkglprobe.exe")
        self.assertEqual(failed.returncode, 1)
        self.assertIn("guest: discovery failed: the probe exited 1 (its window could not be made); "
                      "see discovery.txt", failed.stdout)

    def test_opengl_discovery_is_its_own_boot(self):
        self.discovery_script()
        for other in ("--rvk", "--vulkan", "--opengl"):
            refused = self.run_suite("--opengl-discovery", other)
            self.assertEqual(refused.returncode, 2)
            self.assertIn("--opengl-discovery is its own boot", refused.stderr)
        missing = self.run_suite("--opengl-discovery", "--gl-probe", self.scratch / "noprobe.exe")
        self.assertEqual(missing.returncode, 1)
        self.assertIn("guest: prepare failed: " + str(self.scratch / "noprobe.exe") + " does not exist", missing.stdout)

    GL_REPORT = ("ok opengl.pixel_format 1\r\n"
                 "ok opengl.context\r\n"
                 "note opengl.version 4.6 (Compatibility Profile) Mesa 26.2.2\r\n"
                 "note opengl.renderer Mesa Intel(R) Iris(R) Xe Graphics (ADL GT2)\r\n"
                 "ok opengl.version 46\r\n"
                 "ok opengl.draw 4096\r\n"
                 "ok opengl.share 4\r\n"
                 "ok opengl.busy 170\r\n"
                 "ok opengl.compat 4096\r\n"
                 "ok opengl.interval 59\r\n"
                 "ok opengl.resized 32\r\n"
                 "ok opengl.attribs 8342\r\n"
                 "ok opengl.lost 33365\r\n"
                 "ok opengl.recreated 4096\r\n"
                 "rvkglprobe: 12 passed, 0 failed\r\n")

    def opengl_script(self, report=None, code=0, pictures=None):
        outputs = {"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                   "rvkglprobe": self.GL_REPORT if report is None else report,
                   "collect": COLLECTED + "file C:\\ProgramData\\rvk\\rvkgl-4242.log\r\n"}
        if pictures is None:
            pictures = {f"C:\\rvk\\{name}.bmp": f"BM {name}" for name in GL_PICTURES}
        self.script(outputs=outputs, rvkglprobe=code,
                    files=dict(GUEST_FILES, **{"C:\\ProgramData\\rvk\\rvkgl-4242.log": ""}, **pictures))

    CL_REPORT = ("ok opencl.platform_count 1\r\n"
                 "ok opencl.platforms 1\r\n"
                 "ok opencl.platform 1\r\n"
                 "ok opencl.device_ids 1\r\n"
                 "ok opencl.device 1\r\n"
                 "note opencl.device_name rvk (NVIDIA GeForce RTX 4090)\r\n"
                 "ok opencl.luid\r\n"
                 "ok opencl.context\r\n"
                 "ok opencl.queue\r\n"
                 "ok opencl.buffers\r\n"
                 "ok opencl.write\r\n"
                 "ok opencl.write_second\r\n"
                 "ok opencl.program\r\n"
                 "ok opencl.build\r\n"
                 "ok opencl.kernel\r\n"
                 "ok opencl.arguments\r\n"
                 "ok opencl.run\r\n"
                 "ok opencl.finish\r\n"
                 "ok opencl.read\r\n"
                 "ok opencl.sum 0\r\n"
                 "ok opencl.map 0\r\n"
                 "ok opencl.unmap\r\n"
                 "ok opencl.svm\r\n"
                 "ok opencl.user_event\r\n"
                 "ok opencl.wait\r\n"
                 "ok opencl.callback 1\r\n"
                 "ok opencl.release\r\n"
                 "rvkclprobe: 26 passed, 0 failed\r\n")
    """What rvkclprobe.exe prints when every step of feature 052 passed, in the
    probe's own order (src/guest/suite/cl_probe.cpp)."""

    def opencl_loader(self, pinned=True, missing=()):
        """A fetched OpenCL loader source tree and the pins it was fetched by, as
        tools/fetch_opencl_loader.py leaves them: what a --opencl run's OpenCL.dll
        was cross-built from, and what the run refuses to boot without.
        `missing` names files the pins name and disk does not."""
        into = self.scratch / "opencl-loader"
        digest = "2" * 64
        for name in WANTED + (CMAKE_CONFIG,):
            path = into / name
            path.parent.mkdir(parents=True, exist_ok=True)
            if name in missing:
                path.unlink(missing_ok=True)
            else:
                path.write_text(f"/* {name} */\n")
        marker = into / "PINNED"
        if pinned:
            marker.write_text(digest + "\n")
        else:
            marker.unlink(missing_ok=True)
        (self.scratch / "cl_pins.txt").write_text(f"loader file:///nowhere/opencl-loader.tar.gz {digest}\n")
        return ("--opencl-loader", into, "--opencl-pins", self.scratch / "cl_pins.txt",
                "--cl-probe", self.scratch / "rvkclprobe.exe")

    def opencl_script(self, report=None, code=0):
        outputs = {"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                   "rvkclprobe": self.CL_REPORT if report is None else report,
                   "collect": COLLECTED + "file C:\\ProgramData\\rvk\\rvkcl-4242.log\r\n"}
        self.script(outputs=outputs, rvkclprobe=code,
                    files=dict(GUEST_FILES, **{"C:\\ProgramData\\rvk\\rvkcl-4242.log": ""}))

    def d3d_layers(self, files=("d3d9.dll", "d3d11.dll", "dxgi.dll"),
                   vkd3d=("d3d12.dll", "d3d12core.dll"), missing=(), empty_pins=False, no_files=False):
        """A fetched DXVK and vkd3d-proton, pinned to what is on disk: what a
        --d3d run needs before it boots anything. `missing` names files the
        pins name and disk does not, and `empty_pins` leaves a pins file as it
        is in the repository - named but naming nothing."""
        layers = self.scratch / "layers"
        tools = self.scratch / "tools"
        tools.mkdir(exist_ok=True)
        for layer, names in (("dxvk", files), ("vkd3d-proton", vkd3d)):
            into = layers / layer
            into.mkdir(parents=True, exist_ok=True)
            (into / "source").mkdir(exist_ok=True)
            (into / "source" / "device.c").write_text("/* what the requirement gate reads */\n")
            pins = [f"source file:///nowhere/{layer}-source.tar.gz " + "0" * 64,
                    f"release file:///nowhere/{layer}.tar.gz " + "1" * 64]
            for name in names:
                pinned = (layer + name).encode() * 40
                pins.append(f"file {name} x64/{name} {hashlib.sha256(pinned).hexdigest()}")
                if name in missing:
                    (into / name).unlink(missing_ok=True)
                else:
                    (into / name).write_bytes(pinned)
            if empty_pins:
                text = "# empty on purpose\n"
            elif no_files:
                text = "\n".join(pins[:2]) + "\n"
            else:
                text = "\n".join(pins) + "\n"
            (tools / f"{layer.replace('-', '_')}_pins.txt").write_text(text)
        return ("--layers", layers, "--layer-pins", tools,
                "--d3d9-probe", self.scratch / "rvkd3d9probe.exe",
                "--d3d11-probe", self.scratch / "rvkd3d11probe.exe",
                "--d3d12-probe", self.scratch / "rvkd3d12probe.exe")

    def d3d_script(self, **behaviour):
        outputs = {"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                   "rvkd3d9probe": D3D_REPORTS["9"], "rvkd3d11probe": D3D_REPORTS["11"],
                   "rvkd3d12probe": D3D_REPORTS["12"], "collect": COLLECTED}
        outputs.update(behaviour.pop("outputs", {}))
        pictures = behaviour.pop("pictures", {name: "BM d3d" for name in D3D_PICTURES})
        self.script(outputs=outputs, files=dict(GUEST_FILES, **pictures), **behaviour)

    def test_a_d3d_run_proves_every_step_of_all_three_layers(self):
        """Feature 047, FR-005: the three probes run in the console session with
        the layers' DLLs beside them, every step of each report passes, and the
        frame each cleared is pulled back as a picture."""
        self.d3d_script()
        result = self.run_suite("--d3d", "--executor", "/nowhere/librvk_executor.so",
                                *self.vulkan_runtime(), *self.d3d_layers())
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        lines = self.journal_lines()
        # The layers' DLLs and the loader they call, beside the probes.
        for name in ("d3d9.dll", "d3d11.dll", "dxgi.dll", "d3d12.dll", "d3d12core.dll"):
            self.assertTrue(any(line.startswith(f"file C:\\rvk\\{name} ") for line in lines),
                            f"{name} was not pushed: {lines}")
        self.assertTrue(any(line.startswith("file C:\\rvk\\vulkan-1.dll ") for line in lines))
        for api in ("9", "11", "12"):
            self.assertTrue(any(line.startswith(f"file C:\\rvk\\rvkd3d{api}probe.exe ") for line in lines))
            self.assertIn(f"pull C:\\rvk\\d3d{api}.txt", lines)
            self.assertEqual((self.work / f"d3d{api}.txt").read_text(newline=""), D3D_REPORTS[api])
        self.assertIn("guest: d3d9 ok: 6 passed through dxvk", result.stdout)
        self.assertIn("guest: d3d11 ok: 7 passed through dxvk", result.stdout)
        self.assertIn("guest: d3d12 ok: 9 passed through vkd3d-proton", result.stdout)
        # It ran in the console session, as a window needs.
        self.assertTrue(any("schtasks.exe /create /tn rvkconsole" in line for line in lines))
        for picture in ("d3d9-clear", "d3d11-clear", "d3d12-clear"):
            self.assertEqual((self.work / "images" / f"{picture}.bmp").read_text(), "BM d3d")
        self.assertIn(f"guest: d3d.images ok: 3 in {self.work / 'images'}", result.stdout)
        self.assertEqual(self.stage_calls(), ["sign", "install", "check", "collect"])
        self.assert_clean()

    def test_a_d3d_run_under_page_heap_reports_what_windows_logged(self):
        """Feature 047, FR-006: every probe runs with full page heap on it, the
        Application log is read afterwards, and the flag is taken off the
        machine again. A run that found a heap event names it and fails."""
        self.d3d_script(stages={"sign": [3010], "install": [3010], "check": [0], "pageheap": [0],
                                "heapchecks": [0]},
                        outputs={"check": ONE_MONITOR +
                                          "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                                 "rvkd3d9probe": D3D_REPORTS["9"], "rvkd3d11probe": D3D_REPORTS["11"],
                                 "rvkd3d12probe": D3D_REPORTS["12"], "collect": COLLECTED,
                                 "pageheap": "pageheap: rvkd3d9probe.exe on\r\n",
                                 "heapchecks": "heapchecks: 0\r\n"})
        result = self.run_suite("--d3d", "--page-heap", "--executor", "/nowhere/librvk_executor.so",
                                *self.vulkan_runtime(), *self.d3d_layers())
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("guest: pageheap on: 3 program(s)", result.stdout)
        self.assertIn("guest: heapchecks ok: no heap event under page heap", result.stdout)
        self.assertIn("guest: pageheap off: 3 program(s)", result.stdout)
        # On, then off: the machine is left as it was found.
        programs = "rvkd3d9probe.exe,rvkd3d11probe.exe,rvkd3d12probe.exe"
        self.assertEqual(self.stage_calls(),
                         ["sign", "install", "check", f"pageheap -Programs {programs} -Enable",
                          f"pageheap -Programs {programs}", "heapchecks", "collect"])
        self.assert_clean()

    def test_a_heap_event_under_page_heap_fails_the_run_naming_it(self):
        """What page heap catching a write past an allocation looks like: the
        event is reported and the run fails, and the flag still comes off."""
        self.d3d_script(stages={"sign": [3010], "install": [3010], "check": [0], "pageheap": [0],
                                "heapchecks": [0]},
                        outputs={"check": ONE_MONITOR +
                                          "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                                 "rvkd3d9probe": D3D_REPORTS["9"], "rvkd3d11probe": D3D_REPORTS["11"],
                                 "rvkd3d12probe": D3D_REPORTS["12"], "collect": COLLECTED,
                                 "pageheap": "pageheap: rvkd3d9probe.exe on\r\n",
                                 "heapchecks": ("heapcheck: 1000 Faulting application rvkd3d11probe.exe "
                                                "exception code 0xc0000374\r\nheapchecks: 1\r\n")})
        result = self.run_suite("--d3d", "--page-heap", "--executor", "/nowhere/librvk_executor.so",
                                *self.vulkan_runtime(), *self.d3d_layers())
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: heapcheck: event 1000: Faulting application rvkd3d11probe.exe", result.stdout)
        self.assertIn("heapchecks failed: 1 heap event(s) while the probes ran under page heap", result.stdout)
        self.assertTrue(any(call.startswith("pageheap") and not call.endswith("-Enable")
                            for call in self.stage_calls()),
                        f"the flag was not taken off: {self.stage_calls()}")

    def test_page_heap_without_the_d3d_run_is_refused(self):
        """It is what the D3D probes run under; on its own it would turn a flag
        on and run nothing."""
        self.d3d_script()
        result = self.run_suite("--page-heap", "--executor", "/nowhere/librvk_executor.so")
        self.assertEqual(result.returncode, 1)
        self.assertIn("--page-heap is what the D3D probes run under: it needs --d3d", result.stdout)
        self.assertEqual(self.journal_lines(), [])

    def test_a_d3d_run_without_the_pinned_layers_is_refused_before_qemu(self):
        """The layers are two archives and their hashes; a run that cannot have
        them says which fetch to run and boots nothing."""
        self.d3d_script()
        empty = self.run_suite("--d3d", "--executor", "/nowhere/librvk_executor.so",
                               *self.vulkan_runtime(), *self.d3d_layers(empty_pins=True))
        self.assertEqual(empty.returncode, 1)
        # The pins file in the repository is comments alone, which is what this
        # is: it names no archive, so there is nothing to check against.
        self.assertIn("no source pin: run tools/fetch_layers.py dxvk", empty.stdout)
        self.assertEqual(self.journal_lines(), [])

        self.d3d_script()
        nameless = self.run_suite("--d3d", "--executor", "/nowhere/librvk_executor.so",
                                  *self.vulkan_runtime(), *self.d3d_layers(no_files=True))
        self.assertEqual(nameless.returncode, 1)
        self.assertIn("its pins name no files: run tools/fetch_layers.py dxvk", nameless.stdout)
        self.assertEqual(self.journal_lines(), [])

        self.d3d_script()
        gone = self.run_suite("--d3d", "--executor", "/nowhere/librvk_executor.so",
                              *self.vulkan_runtime(), *self.d3d_layers(missing=("d3d11.dll",)))
        self.assertEqual(gone.returncode, 1)
        self.assertIn("--d3d needs the pinned dxvk", gone.stdout)
        self.assertIn("d3d11.dll missing", gone.stdout)
        self.assertEqual(self.journal_lines(), [])

        # A layer file that is not what it was pinned to is not the layer.
        self.d3d_script()
        layers = self.d3d_layers()
        (self.scratch / "layers" / "dxvk" / "d3d9.dll").write_bytes(b"something else")
        tampered = self.run_suite("--d3d", "--executor", "/nowhere/librvk_executor.so",
                                  *self.vulkan_runtime(), *layers)
        self.assertEqual(tampered.returncode, 1)
        self.assertIn("d3d9.dll sha256", tampered.stdout)
        self.assertEqual(self.journal_lines(), [])

    def test_a_d3d_run_fails_a_failed_missing_or_unsummarised_step(self):
        """What a layer that half works looks like: a failed step, a step the
        probe never printed, and a report with no summary at all."""
        cases = {
            "failed step": ({"rvkd3d11probe": D3D_REPORTS["11"].replace(
                "ok d3d11.pixels 4096", "FAIL d3d11.pixels status=6 0").replace(
                "7 passed, 0 failed", "6 passed, 1 failed")},
                "d3d11 failed: 1 step(s) failed, exit 0; see d3d11.txt"),
            "missing step": ({"rvkd3d12probe": D3D_REPORTS["12"].replace("ok d3d12.fence\r\n", "").replace(
                "9 passed", "8 passed")},
                "d3d12 failed: the probe reported no fence; see d3d12.txt"),
            "no summary": ({"rvkd3d9probe": D3D_REPORTS["9"].replace(
                "rvkd3d9probe: 6 passed, 0 failed\r\n", "")},
                "d3d9 failed: no summary line (exit 0); see d3d9.txt"),
            "another probe's summary": ({"rvkd3d9probe": D3D_REPORTS["9"].replace(
                "rvkd3d9probe: 6 passed, 0 failed", "rvkd3d11probe: 6 passed, 0 failed")},
                "d3d9 failed: no summary line (exit 0); see d3d9.txt"),
        }
        for case, (outputs, reason) in cases.items():
            with self.subTest(case):
                self.d3d_script(outputs=outputs)
                result = self.run_suite("--d3d", "--executor", "/nowhere/librvk_executor.so",
                                        *self.vulkan_runtime(), *self.d3d_layers())
                self.assertEqual(result.returncode, 1)
                self.assertIn(reason, result.stdout)

    def cts_build(self, files=("deqp-vk.exe",), missing=(), empty_pins=False):
        build = self.scratch / "vulkan-cts"
        build.mkdir(exist_ok=True)
        pins = [f"archive file:///nowhere/cts.zip {'0' * 64}"]
        for name in files:
            pinned = name.encode() * 30
            pins.append(f"file {name} inside/{name} {hashlib.sha256(pinned).hexdigest()}")
            if name in missing:
                (build / name).unlink(missing_ok=True)
            else:
                (build / name).write_bytes(pinned)
        text = "# empty on purpose\n" if empty_pins else "\n".join(pins) + "\n"
        (self.scratch / "cts_pins.txt").write_text(text)
        (self.scratch / "cts_ratchet.txt").write_text("")
        return ("--cts-dir", build, "--cts-pins", self.scratch / "cts_pins.txt",
                "--cts-ratchet", self.scratch / "cts_ratchet.txt",
                "--cts-subset", ROOT / "tools" / "cts_subset.txt")

    def cts_script(self, logs=None, **behaviour):
        # What deqp-vk prints is what the caselist redirection captures: the
        # runner asks the guest's own build what cases there are.
        outputs = {"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                   "deqp-vk": CTS_CASELIST, "collect": COLLECTED}
        outputs.update(behaviour.pop("outputs", {}))
        files = dict(logs or {"C:\\rvk\\cts\\shard0.qpa": qpa(("dEQP-VK.api.smoke.triangle", "Pass"),
                                                              ("dEQP-VK.api.smoke.create_sampler", "Pass"),
                                                              ("dEQP-VK.memory.budget.query", "Pass"))})
        files.update(behaviour.pop("files", {}))
        self.script(outputs=outputs, files=dict(GUEST_FILES, **files), **behaviour)

    def run_cts(self, *extra, **kwargs):
        return self.run_suite("--cts", "--executor", "/nowhere/librvk_executor.so",
                              *self.vulkan_runtime(), *self.cts_build(**kwargs), *extra)

    def test_a_cts_run_runs_what_the_subset_names_of_what_the_cts_listed(self):
        self.cts_script()
        result = self.run_cts("--cts-shard", "8")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        lines = self.journal_lines()
        # The build and the loader beside it, in a directory of its own.
        self.assertTrue(any(line.startswith("file C:\\rvk\\cts\\deqp-vk.exe ") for line in lines))
        self.assertTrue(any(line.startswith("file C:\\rvk\\cts\\vulkan-1.dll ") for line in lines))
        # The case list came from the guest's own build.
        self.assertTrue(any("--deqp-runmode=stdout-caselist" in line for line in lines))
        self.assertIn("pull C:\\rvk\\cts\\caselist.txt", lines)
        # The three cases the subset names, and not the one it does not.
        cases = (self.work / "cts" / "cases.txt").read_text()
        self.assertIn("dEQP-VK.api.smoke.triangle", cases)
        self.assertIn("dEQP-VK.memory.budget.query", cases)
        self.assertNotIn("dEQP-VK.pipeline.enormous.case", cases)
        self.assertIn("guest: cts: 3 case(s) in 1 shard(s), 1 log(s)", result.stdout)
        self.assertIn("guest: cts: 3 case(s) answered, 3 passed", result.stdout)
        self.assertIn("guest: cts.ratchet ok: 0 kept, 3 gained", result.stdout)
        self.assertTrue((self.work / "cts" / "guest.qpa").exists())
        self.assert_clean()

    def test_a_shard_that_died_in_a_case_is_picked_up_after_it(self):
        """A crash, a TDR or a bugcheck leaves the log open at the case that
        caused it; the run continues from the next one, and the case that died
        is recorded rather than dropped."""
        self.cts_script(logs={
            "C:\\rvk\\cts\\shard0.qpa": qpa(("dEQP-VK.api.smoke.triangle", "Pass"), ended=False) +
                                        "#beginTestCaseResult dEQP-VK.api.smoke.create_sampler\r\n",
            "C:\\rvk\\cts\\shard0.r1.qpa": qpa(("dEQP-VK.memory.budget.query", "Pass")),
        })
        result = self.run_cts("--cts-shard", "8")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("guest: cts: shard0 died in dEQP-VK.api.smoke.create_sampler", result.stdout)
        lines = self.journal_lines()
        # The second run was given the case after the one that died, and never
        # the one that died.
        self.assertTrue(any(line.startswith("file C:\\rvk\\cts\\shard0.r1.cases.txt ") for line in lines),
                        lines)
        resumed = (self.work / "cts" / "shard0.r1.qpa").read_text()
        self.assertIn("dEQP-VK.memory.budget.query", resumed)
        self.assertIn("guest: cts: 3 case(s) answered, 2 passed", result.stdout)
        # And the case that killed it is in the merged log as a crash.
        whole = (self.work / "cts" / "guest.qpa").read_text()
        self.assertIn("dEQP-VK.api.smoke.create_sampler", whole)

    def test_a_shard_with_no_restarts_left_fails_naming_what_went_unrun(self):
        """A resumed shard always shrinks, so the restart limit is reached only
        by a guest that keeps dying - and then what it did not run is named
        rather than counted as passed."""
        dying = qpa(ended=False) + "#beginTestCaseResult dEQP-VK.api.smoke.triangle\r\n"
        self.cts_script(logs={"C:\\rvk\\cts\\shard0.qpa": dying})
        result = self.run_cts("--cts-shard", "8", "--cts-restarts", "0")
        self.assertEqual(result.returncode, 1)
        self.assertIn("cts.shard0 failed: died in a case 1 time(s) with 2 case(s) unrun", result.stdout)

    def test_a_run_that_lost_a_case_the_guest_used_to_pass_fails(self):
        self.cts_script(logs={"C:\\rvk\\cts\\shard0.qpa": qpa(("dEQP-VK.api.smoke.triangle", "Pass"),
                                                              ("dEQP-VK.api.smoke.create_sampler", "Fail"),
                                                              ("dEQP-VK.memory.budget.query", "Pass"))})
        extra = self.cts_build()
        (self.scratch / "cts_ratchet.txt").write_text("dEQP-VK.api.smoke.create_sampler\n")
        result = self.run_suite("--cts", "--executor", "/nowhere/librvk_executor.so",
                                *self.vulkan_runtime(), *extra, "--cts-shard", "8")
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: cts: dEQP-VK.api.smoke.create_sampler passed before and does not now",
                      result.stdout)
        self.assertIn("cts.ratchet failed: 1 case(s) lost of 1 recorded", result.stdout)

    def test_the_verdict_is_the_comparison_with_the_hosts_run(self):
        """FR-002: a case that passes on the host and not in the guest fails the
        run; one that fails on both does not."""
        self.cts_script(logs={"C:\\rvk\\cts\\shard0.qpa": qpa(("dEQP-VK.api.smoke.triangle", "Pass"),
                                                              ("dEQP-VK.api.smoke.create_sampler", "Fail"),
                                                              ("dEQP-VK.memory.budget.query", "Fail"))})
        host = self.scratch / "host.qpa"
        host.write_text(qpa(("dEQP-VK.api.smoke.triangle", "Pass"),
                            ("dEQP-VK.api.smoke.create_sampler", "Pass"),
                            ("dEQP-VK.memory.budget.query", "Fail")))
        failed = self.run_cts("--cts-shard", "8", "--cts-host", host)
        self.assertEqual(failed.returncode, 1)
        self.assertIn("guest: cts: dEQP-VK.api.smoke.create_sampler passes on the host and does not in the "
                      "guest", failed.stdout)
        self.assertIn("cts.compare failed: 1 guest-only failure(s) of 3 compared", failed.stdout)

        # The same run against a host that failed it too passes, and says how
        # many failures matched.
        host.write_text(qpa(("dEQP-VK.api.smoke.triangle", "Pass"),
                            ("dEQP-VK.api.smoke.create_sampler", "Fail"),
                            ("dEQP-VK.memory.budget.query", "Fail")))
        self.cts_script(logs={"C:\\rvk\\cts\\shard0.qpa": qpa(("dEQP-VK.api.smoke.triangle", "Pass"),
                                                              ("dEQP-VK.api.smoke.create_sampler", "Fail"),
                                                              ("dEQP-VK.memory.budget.query", "Fail"))})
        passed = self.run_cts("--cts-shard", "8", "--cts-host", host)
        self.assertEqual(passed.returncode, 0, passed.stdout + passed.stderr)
        self.assertIn("guest: cts ok: 3 compared, 0 guest-only failures, 2 matching failure(s)", passed.stdout)

    def test_a_cts_run_without_the_pinned_build_is_refused_before_qemu(self):
        self.cts_script()
        empty = self.run_cts(empty_pins=True)
        self.assertEqual(empty.returncode, 1)
        self.assertIn("--cts: ", empty.stdout)
        self.assertIn("run tools/fetch_cts.py", empty.stdout)
        self.assertEqual(self.journal_lines(), [])

        self.cts_script()
        gone = self.run_cts(missing=("deqp-vk.exe",))
        self.assertEqual(gone.returncode, 1)
        self.assertIn("--cts needs the pinned CTS", gone.stdout)
        self.assertIn("deqp-vk.exe missing", gone.stdout)
        self.assertEqual(self.journal_lines(), [])

    def test_a_subset_that_matches_nothing_the_cts_listed_is_a_failure(self):
        """A subset and a build that disagree is not a run of anything, and a
        pass here would be a conformance claim nobody measured."""
        self.cts_script(outputs={"deqp-vk": "TEST: dEQP-VK.nothing.this.names\r\n"})
        result = self.run_cts()
        self.assertEqual(result.returncode, 1)
        self.assertIn("the subset matched none of the", result.stdout)

    def test_a_frame_with_no_reference_is_kept_and_said_so(self):
        """Feature 048, US2: the first run of an application is where a
        reference comes from, and a run that claimed to match one nobody
        committed would be a claim nobody made."""
        self.vulkan_script()
        references = self.scratch / "references"
        references.mkdir()
        result = self.run_suite("--vulkan", "--frames", "--references", references,
                                "--executor", "/nowhere/librvk_executor.so", *self.vulkan_runtime())
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("guest: frames: no reference committed for vulkan", result.stdout)
        captured = self.work / "frames" / "vulkan.ppm"
        self.assertTrue(captured.exists())
        self.assertTrue(captured.read_bytes().startswith(b"P6"))

    def test_a_frame_that_is_not_the_reference_fails_and_keeps_the_difference(self):
        """FR-004: what is compared is the device's scanout, so a run that
        composited something else is a failure with three files to look at."""
        self.vulkan_script()
        references = self.scratch / "references"
        references.mkdir()
        # A reference of the size the fake scans out, and nothing like its
        # pixels.
        (references / "vulkan.ppm").write_bytes(b"P6\n1280 800\n255\n" + bytes(1280 * 800 * 3))
        result = self.run_suite("--vulkan", "--frames", "--references", references,
                                "--executor", "/nowhere/librvk_executor.so", *self.vulkan_runtime())
        self.assertEqual(result.returncode, 1)
        self.assertIn("pixel(s) outside a step of 2", result.stdout)
        self.assertTrue((self.work / "frames" / "vulkan-difference.ppm").exists())

    def test_a_reference_of_another_size_is_named_rather_than_compared(self):
        self.vulkan_script()
        references = self.scratch / "references"
        references.mkdir()
        (references / "vulkan.ppm").write_bytes(b"P6\n4 2\n255\n" + bytes(4 * 2 * 3))
        result = self.run_suite("--vulkan", "--frames", "--references", references,
                                "--executor", "/nowhere/librvk_executor.so", *self.vulkan_runtime())
        self.assertEqual(result.returncode, 1)
        self.assertIn("the capture is 1280x800 and the reference is 4x2", result.stdout)

    def test_an_opengl_run_proves_every_step_through_the_system_opengl32(self):
        """Feature 050, T043: the probe runs with its report written to a guest
        file and pulled; every one of the nine steps passes, the renderer is a
        GPU's, and rvkgl's log is kept."""
        self.opengl_script()
        result = self.run_suite("--opengl", "--executor", "/nowhere/librvk_executor.so",
                                "--gl-probe", self.scratch / "rvkglprobe.exe")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        lines = self.journal_lines()
        self.assertIn("file C:\\rvk\\rvkglprobe.exe 1400", lines)
        self.assertIn("file C:\\rvk\\package\\rvkgl.dll 900", lines)
        self.assertIn("exec C:\\Windows\\System32\\cmd.exe /c C:\\rvk\\rvkglprobe.exe "
                      "> C:\\rvk\\opengl.txt 2>&1", lines)
        self.assertIn("pull C:\\rvk\\opengl.txt", lines)
        self.assertEqual(self.stage_calls(), ["sign", "install", "check", "collect"])
        self.assertEqual((self.work / "opengl.txt").read_text(newline=""), self.GL_REPORT)
        self.assertIn("guest: opengl ok: 12 passed, renderer Mesa Intel(R) Iris(R) Xe Graphics (ADL GT2), "
                      "version 4.6 (Compatibility Profile) Mesa 26.2.2", result.stdout)
        self.assertIn("opengl ok", (self.work / "verdict.txt").read_text())
        self.assertTrue((self.work / "diag" / "rvkgl-4242.log").exists())
        # The renders the probe judged, as pictures a person can open.
        for name in GL_PICTURES:
            self.assertEqual((self.work / "images" / f"{name}.bmp").read_text(), f"BM {name}")
            self.assertIn(f"pull C:\\rvk\\{name}.bmp", lines)
        self.assertIn(f"guest: opengl.images ok: {len(GL_PICTURES)} in {self.work / 'images'}", result.stdout)
        self.assertNotIn("--discovery", "\n".join(lines))
        self.assert_clean()

    def test_an_opengl_run_fails_a_failed_missing_or_software_step(self):
        report = self.GL_REPORT
        cases = {
            "failed step": (report.replace("ok opengl.share 4", "fail opengl.share 0").replace(
                "12 passed, 0 failed", "11 passed, 1 failed"), 1,
                "opengl failed: 1 step(s) failed, exit 1; see opengl.txt"),
            "missing step": (report.replace("ok opengl.busy 170\r\n", "").replace("12 passed", "11 passed"), 0,
                             "opengl failed: the probe reported no busy; see opengl.txt"),
            "no summary": (report.replace("rvkglprobe: 12 passed, 0 failed\r\n", ""), 0,
                           "opengl failed: no summary in the probe's report, exit 0; see opengl.txt"),
            "software": (report.replace("Mesa Intel(R) Iris(R) Xe Graphics (ADL GT2)", "llvmpipe (LLVM 19.1.0)"), 0,
                         "opengl failed: the renderer llvmpipe (LLVM 19.1.0) is not a host GPU"),
            "no renderer": (report.replace("note opengl.renderer Mesa Intel(R) Iris(R) Xe Graphics (ADL GT2)\r\n",
                                           ""), 0,
                            "opengl failed: the probe named no renderer"),
        }
        for case, (text, code, reason) in cases.items():
            with self.subTest(case):
                self.opengl_script(text, code)
                result = self.run_suite("--opengl", "--executor", "/nowhere/librvk_executor.so",
                                        "--gl-probe", self.scratch / "rvkglprobe.exe")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(f"guest: {reason}", result.stdout)

    def test_a_passing_probe_without_its_pictures_fails(self):
        """A probe that judged a render wrote its picture; a missing one means
        the pixels were judged from something no one can look at."""
        self.opengl_script(pictures={"C:\\rvk\\opengl-draw.bmp": "BM"})
        result = self.run_suite("--opengl", "--executor", "/nowhere/librvk_executor.so",
                                "--gl-probe", self.scratch / "rvkglprobe.exe")
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("guest: opengl.images failed: guest agent error: no file C:\\rvk\\opengl-share.bmp",
                      result.stdout)
        self.vulkan_script(pictures={})
        result = self.run_suite("--vulkan", "--executor", "/nowhere/librvk_executor.so", *self.vulkan_runtime())
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("guest: vulkan.images failed: guest agent error: no file C:\\rvk\\vulkan-draw.bmp",
                      result.stdout)

    def test_an_opengl_run_needs_the_probe(self):
        self.opengl_script()
        missing = self.run_suite("--opengl", "--executor", "/nowhere/librvk_executor.so",
                                 "--gl-probe", self.scratch / "noprobe.exe")
        self.assertEqual(missing.returncode, 1)
        self.assertIn("guest: prepare failed: " + str(self.scratch / "noprobe.exe") + " does not exist", missing.stdout)

    def test_an_opencl_run_proves_every_step_through_the_khronos_loader(self):
        """Feature 052, T057: the probe runs beside the cross-built loader with its
        report written to a guest file and pulled; every one of the probe's steps
        passes, and the device it named is a host GPU's rather than a CPU
        runtime's."""
        self.opencl_script()
        result = self.run_suite("--opencl", "--executor", "/nowhere/librvk_executor.so", *self.opencl_loader())
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        lines = self.journal_lines()
        # The loader the guest loads is the cross-built one beside the probe,
        # never the pinned source tree it was built from.
        self.assertIn("file C:\\rvk\\OpenCL.dll 1000", lines)
        self.assertIn("file C:\\rvk\\rvkclprobe.exe 1400", lines)
        self.assertIn("file C:\\rvk\\package\\rvkcl.dll 900", lines)
        self.assertIn("exec C:\\Windows\\System32\\cmd.exe /c C:\\rvk\\rvkclprobe.exe "
                      "> C:\\rvk\\opencl.txt 2>&1", lines)
        self.assertIn("pull C:\\rvk\\opencl.txt", lines)
        self.assertEqual(self.stage_calls(), ["sign", "install", "check", "collect"])
        self.assertEqual((self.work / "opencl.txt").read_text(newline=""), self.CL_REPORT)
        self.assertIn("guest: opencl ok: 26 passed, device rvk (NVIDIA GeForce RTX 4090)", result.stdout)
        self.assertIn("opencl ok", (self.work / "verdict.txt").read_text())
        self.assertTrue((self.work / "diag" / "rvkcl-4242.log").exists())
        # The probe renders nothing, so no picture is asked of it.
        self.assertFalse((self.work / "images").exists())
        self.assertNotIn("rvkglprobe", "\n".join(lines))
        self.assert_clean()

    def test_an_opencl_run_fails_a_failed_missing_or_software_step(self):
        report = self.CL_REPORT
        cases = {
            "failed step": (report.replace("ok opencl.sum 0", "fail opencl.sum 64").replace(
                "26 passed, 0 failed", "25 passed, 1 failed"), 1,
                "opencl failed: 1 step(s) failed, exit 1; see opencl.txt"),
            "missing step": (report.replace("ok opencl.unmap\r\n", "").replace("26 passed", "25 passed"), 0,
                             "opencl failed: the probe reported no unmap; see opencl.txt"),
            "no summary": (report.replace("rvkclprobe: 26 passed, 0 failed\r\n", ""), 0,
                           "opencl failed: no summary in the probe's report, exit 0; see opencl.txt"),
            "software": (report.replace("rvk (NVIDIA GeForce RTX 4090)", "Intel(R) Core(TM) i9, SwiftShader"), 0,
                         "opencl failed: the device Intel(R) Core(TM) i9, SwiftShader is not a host GPU"),
            "no device": (report.replace("note opencl.device_name rvk (NVIDIA GeForce RTX 4090)\r\n", ""), 0,
                          "opencl failed: the probe named no device"),
        }
        for case, (text, code, reason) in cases.items():
            with self.subTest(case):
                self.opencl_script(text, code)
                result = self.run_suite("--opencl", "--executor", "/nowhere/librvk_executor.so",
                                        *self.opencl_loader())
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(f"guest: {reason}", result.stdout)
                self.assertEqual((self.work / "opencl.txt").read_text(newline=""), text)

    def test_an_opencl_run_without_the_pinned_loader_the_dll_or_the_probe_is_refused_before_qemu(self):
        """The loader is cross-built from a pinned tree, so a run whose tree
        nobody fetched would be reporting a loader nobody pinned."""
        self.opencl_script()
        unfetched = self.run_suite("--opencl", *self.opencl_loader(pinned=False))
        self.assertEqual(unfetched.returncode, 1)
        self.assertIn(f"guest: prepare failed: --opencl needs the pinned OpenCL loader in "
                      f"{self.scratch / 'opencl-loader'} (PINNED missing): run tools/fetch_opencl_loader.py",
                      unfetched.stdout)
        gutted = self.run_suite("--opencl", *self.opencl_loader(missing=("loader/icd.c",)))
        self.assertEqual(gutted.returncode, 1)
        self.assertIn("(loader/icd.c missing)", gutted.stdout)
        unpinned = self.run_suite("--opencl", *self.opencl_loader(), "--opencl-pins", self.scratch / "absent.txt")
        self.assertEqual(unpinned.returncode, 1)
        self.assertIn("guest: prepare failed: --opencl: no pins: ", unpinned.stdout)
        (self.scratch / "cl_pins.txt").write_text("loader file:///nowhere/loader.tar.gz\n")
        malformed = self.run_suite("--opencl", "--opencl-loader", self.scratch / "opencl-loader",
                                   "--opencl-pins", self.scratch / "cl_pins.txt",
                                   "--cl-probe", self.scratch / "rvkclprobe.exe")
        self.assertEqual(malformed.returncode, 1)
        self.assertIn("guest: prepare failed: --opencl: ", malformed.stdout)
        (self.scratch / "OpenCL.dll").unlink()
        unbuilt = self.run_suite("--opencl", *self.opencl_loader())
        self.assertEqual(unbuilt.returncode, 1)
        self.assertIn("guest: prepare failed: " + str(self.scratch / "OpenCL.dll") + " does not exist",
                      unbuilt.stdout)
        missing = self.run_suite("--opencl", *self.opencl_loader(), "--cl-probe", self.scratch / "noprobe.exe")
        self.assertEqual(missing.returncode, 1)
        self.assertIn("guest: prepare failed: " + str(self.scratch / "noprobe.exe") + " does not exist",
                      missing.stdout)
        self.assertEqual(self.journal_lines(), [])
        self.assert_clean()

    def test_a_vulkan_run_without_the_pinned_runtime_or_the_probe_is_refused_before_qemu(self):
        self.vulkan_script()
        absent = self.run_suite("--vulkan", *self.vulkan_runtime(vulkaninfo_exe=None))
        self.assertEqual(absent.returncode, 1)
        self.assertIn(f"guest: prepare failed: --vulkan needs the pinned Vulkan runtime in "
                      f"{self.scratch / 'vulkan-runtime'} (vulkaninfo.exe missing): run tools/fetch_vulkan_runtime.py",
                      absent.stdout)
        tampered = self.run_suite("--vulkan", *self.vulkan_runtime(vulkan_1_dll=b"not the loader"))
        self.assertEqual(tampered.returncode, 1)
        self.assertIn("(vulkan-1.dll sha256 ", tampered.stdout)
        unpinned = self.run_suite("--vulkan", *self.vulkan_runtime(), "--vulkan-pins", self.scratch / "absent.txt")
        self.assertEqual(unpinned.returncode, 1)
        self.assertIn("guest: prepare failed: --vulkan: no pins: ", unpinned.stdout)
        unbuilt = self.run_suite("--vulkan", *self.vulkan_runtime(), "--probe", self.scratch / "absent-probe.exe")
        self.assertEqual(unbuilt.returncode, 1)
        self.assertIn("absent-probe.exe does not exist", unbuilt.stdout)
        self.assertEqual(self.journal_lines(), [])
        self.assert_clean()

    def real_executor_script(self, rvk_report=None, gl_report=None):
        """One boot's answers for --rvk --vulkan --opengl together."""
        rvk = rvk_report or "ok transport.nop\nok transport.latency 812\nrvktest: 12 passed, 0 failed\n"
        outputs = {"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                   "reg": KEY_ABSENT, "vulkaninfo": VULKANINFO, "rvkvkprobe": PROBE_REPORT, "rvktest": rvk,
                   "rvkglprobe": gl_report or self.GL_REPORT, "collect": COLLECTED}
        pictures = {name: "BM vulkan" for name in VULKAN_PICTURES}
        pictures.update({f"C:\\rvk\\{name}.bmp": f"BM {name}" for name in GL_PICTURES})
        self.script(outputs=outputs, reg=1, files=dict(GUEST_FILES, **pictures))

    def real_executor_run(self):
        return self.run_suite("--rvk", "--vulkan", "--opengl", "--executor", "/nowhere/librvk_executor.so",
                              *self.vulkan_runtime(), "--gl-probe", self.scratch / "rvkglprobe.exe")

    def test_the_real_executor_scenarios_share_one_boot(self):
        """The rvk scenarios, the Vulkan ICD and the OpenGL ICD all need the real
        executor and none needs a fresh machine, so the gate runs them in one
        boot: one install, each scenario in turn."""
        self.real_executor_script()
        result = self.real_executor_run()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.stage_calls(), ["sign", "install", "check", "collect"])
        order = [line.split(": ", 1)[1] for line in result.stdout.splitlines()
                 if line.startswith(("guest: rvk ok", "guest: vulkan ok", "guest: opengl ok"))]
        self.assertEqual(list(dict.fromkeys(entry.split(" ok")[0] for entry in order)), ["rvk", "vulkan", "opengl"])
        lines = self.journal_lines()
        for pushed in ("C:\\rvk\\rvkvkprobe.exe", "C:\\rvk\\rvkglprobe.exe", "C:\\rvk\\vulkaninfo.exe"):
            self.assertTrue(any(line.startswith(f"file {pushed} ") for line in lines), pushed)
        self.assertTrue((self.work / "rvk.txt").exists())
        self.assertTrue((self.work / "vulkan.txt").exists())
        self.assertTrue((self.work / "opengl.txt").exists())
        self.assert_clean()

    def test_a_failed_scenario_in_the_shared_boot_still_runs_the_others(self):
        self.real_executor_script(rvk_report="FAIL vulkan.instance\nrvktest: 11 passed, 1 failed\n")
        result = self.real_executor_run()
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("guest: rvk failed: 1 step(s) failed", result.stdout)
        self.assertIn("guest: vulkan ok: 12 passed", result.stdout)
        self.assertIn("guest: opengl ok: 12 passed", result.stdout)
        self.assertIn("guest: scenarios failed: 1 of 3 failed: rvk", result.stdout)
        self.assertIn("rvk failed", (self.work / "verdict.txt").read_text())

    def test_opengl_discovery_cannot_share_a_boot(self):
        result = self.run_suite("--rvk", "--opengl-discovery")
        self.assertEqual(result.returncode, 2)
        self.assertIn("--opengl-discovery is its own boot", result.stderr)

    def test_a_run_that_injects_a_stall_runs_the_timeout_scenario_alone(self):
        """US3: with the host holding one context, every other scenario would be
        measuring the stall, so the run asks the guest for the timeout scenario
        and wants dxgkrnl's own 4101 beside the guest's verdict."""
        self.script(stages={"sign": [3010], "install": [3010], "check": [0], "timeouts": [0]},
                    outputs={"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                             "rvktest": PASSING_REPORT, "collect": COLLECTED,
                             "timeouts": TIMEOUT_EVENTS})
        result = self.run_suite("--stall-ms", "5000")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("guest: tdr ok: 23 passed, the held context lost", result.stdout)
        self.assertIn("guest: tdr ok: 1 recovery event(s) 4101 in the System log", result.stdout)
        # The suite and the rate it measures belong to a run with no injection.
        self.assertNotIn("guest: report ok", result.stdout)
        self.assertNotIn("guest: vsync ok", result.stdout)
        environment = json.loads((self.work / "environment.json").read_text())
        self.assertEqual(environment.get("RVK_ECHO_ONLY_NTH"), "1")

    def test_a_stall_that_dxgkrnl_never_timed_out_fails_the_run(self):
        """Without event 4101 nothing was reset, and whatever the guest saw was
        about something else."""
        self.script(stages={"sign": [3010], "install": [3010], "check": [0], "timeouts": [0]},
                    outputs={"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                             "rvktest": PASSING_REPORT, "collect": COLLECTED, "timeouts": "timeouts: 0\r\n"})
        result = self.run_suite("--stall-ms", "5000")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("the System log holds no event 4101", (self.work / "verdict.txt").read_text())

    def test_the_timeouts_live_dump_is_expected_only_where_one_was_injected(self):
        """A TDR live dump is this driver's failure (SC-005) unless the run is
        the one that told the host to stop answering."""
        dumps = dict(GUEST_FILES)
        dumps["C:\\Windows\\LiveKernelReports\\WATCHDOG\\WD-1.dmp"] = live_dump(0x141)
        self.script(stages={"sign": [3010], "install": [3010], "check": [0], "timeouts": [0]},
                    outputs={"check": ONE_MONITOR + "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                             "rvktest": PASSING_REPORT, "collect": COLLECTED, "timeouts": TIMEOUT_EVENTS},
                    files=dumps)
        result = self.run_suite("--stall-ms", "5000")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("guest: dumps ok: 1 live dump(s), every one VIDEO_ENGINE_TIMEOUT_DETECTED - the host was "
                      "told to stop answering one context", result.stdout)

    def test_a_timeout_live_dump_without_an_injection_fails_the_run(self):
        dumps = dict(GUEST_FILES)
        dumps["C:\\Windows\\LiveKernelReports\\WATCHDOG\\WD-1.dmp"] = live_dump(0x141)
        self.script(screendump=[1280, 800], files=dumps)
        result = self.run_suite()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("guest: dumps failed: WD-1.dmp 0x141 VIDEO_ENGINE_TIMEOUT_DETECTED", result.stdout)

    def test_the_stress_window_is_passed_to_the_guest_and_its_numbers_read_back(self):
        """Feature 039 US3: a long run is asked for in minutes, spent in the
        guest, and what it saw comes back on the report's own lines."""
        stressed = ("ok adapter.find\nok stress.fences 74210\nok stress.worst_wait_us 812\n"
                    "rvktest: 30 passed, 0 failed\n")
        self.script(outputs={"check": ONE_MONITOR, "rvktest": stressed, "collect": COLLECTED})
        result = self.run_suite("--stress-minutes", "0.5")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("C:\\rvk\\rvktest.exe 30", "\n".join(self.journal_lines()))
        self.assertIn("guest: stress ok: 74210 fences awaited, worst wait 812 us", result.stdout)
        self.assert_clean()

    def test_the_submission_costs_are_read_back_and_judged_against_the_record(self):
        """Feature 049 US3: both per-submission costs come back on the report's
        own lines, and what the record holds is their ratio."""
        measured = ("ok adapter.find\nnote submit.doorbell connected\r\nok submit.doorbell_ns 400\r\n"
                    "ok submit.kernel_ns 1800\r\nok submit.ratio 450\r\n"
                    "rvktest: 30 passed, 0 failed\n")
        self.script(outputs={"check": ONE_MONITOR, "rvktest": measured, "collect": COLLECTED})
        # Nothing recorded: the run passes and says the line that would record it.
        empty = self.scratch / "submit_ratchet.txt"
        empty.write_text("# nothing yet\n")
        result = self.run_suite("--submit-ratchet", str(empty))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("guest: submit ok: 400 ns through the doorbell, 1800 ns through the kernel, ratio 4.50x",
                      result.stdout)
        self.assertIn("submit.ratchet: nothing recorded yet; commit `ratio 450`", result.stdout)

        # A record this run keeps, and one it falls below.
        kept = self.scratch / "kept.txt"
        kept.write_text("ratio 400\n")
        self.script(outputs={"check": ONE_MONITOR, "rvktest": measured, "collect": COLLECTED})
        result = self.run_suite("--submit-ratchet", str(kept))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("submit.ratchet ok: 4.00x kept, 0.50x gained; commit `ratio 450`", result.stdout)

        lost = self.scratch / "lost.txt"
        lost.write_text("ratio 600\n")
        self.script(outputs={"check": ONE_MONITOR, "rvktest": measured, "collect": COLLECTED})
        result = self.run_suite("--submit-ratchet", str(lost))
        self.assertEqual(result.returncode, 1)
        self.assertIn("submit.ratchet failed: the doorbell path is worth 4.50x the kernel one here and "
                      "6.00x was recorded", result.stdout)

        # A record that is not a ratio stops the run rather than being ignored.
        broken = self.scratch / "broken.txt"
        broken.write_text("four hundred\n")
        self.script(outputs={"check": ONE_MONITOR, "rvktest": measured, "collect": COLLECTED})
        result = self.run_suite("--submit-ratchet", str(broken))
        self.assertEqual(result.returncode, 1)
        self.assertIn("is not a `ratio <hundredths>` line", result.stdout)

    def test_a_machine_without_hardware_queues_measures_no_gain_and_says_so(self):
        """Hardware scheduling is the OS's decision (049 research.md §11): a
        guest that gives out no hardware queue measures the kernel path alone,
        and nothing is judged against the record."""
        only_kernel = ("ok adapter.find\nnote submit.doorbell none\r\nok submit.doorbell_ns 0\r\n"
                       "ok submit.kernel_ns 1800\r\nok submit.ratio 0\r\n"
                       "rvktest: 30 passed, 0 failed\n")
        held = self.scratch / "held.txt"
        held.write_text("ratio 600\n")
        self.script(outputs={"check": ONE_MONITOR, "rvktest": only_kernel, "collect": COLLECTED})
        result = self.run_suite("--submit-ratchet", str(held))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("submit skipped: this machine gave the adapter no hardware queue", result.stdout)
        self.assert_clean()

    def test_a_suite_that_measured_no_submission_cost_says_so(self):
        self.script(outputs={"check": ONE_MONITOR, "rvktest": PASSING_REPORT, "collect": COLLECTED})
        result = self.run_suite()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("submit skipped: the suite measured no submission cost", result.stdout)
        self.assert_clean()

    def test_a_run_that_asked_for_stress_and_got_none_fails(self):
        self.script(outputs={"check": ONE_MONITOR, "rvktest": PASSING_REPORT, "collect": COLLECTED})
        result = self.run_suite("--stress-minutes", "0.5")
        self.assertEqual(result.returncode, 1)
        self.assertIn("stress failed: the suite ran no fence stress", result.stdout)
        self.assert_clean()

    def test_a_stage_that_needs_no_reboot_is_not_rebooted_and_garbage_from_the_agent_is_skipped(self):
        self.script(stages={"sign": [0], "install": [0], "check": [0]}, garbage=True)
        result = self.run_suite()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("reboot", self.journal_lines())
        self.assert_clean()

    def test_a_failing_stage_names_itself_nothing_after_it_runs_and_the_guest_is_still_collected(self):
        self.script(stages={"sign": [0], "install": [1], "check": [0]}, outputs={"install": "pnputil: no match\n"})
        result = self.run_suite()
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: install failed: exited 1: pnputil: no match", result.stdout)
        self.assertFalse(any(line.startswith("exec C:\\rvk\\rvktest.exe") for line in self.journal_lines()))
        self.assertEqual(self.stage_calls(), ["sign", "install", "collect"])
        self.assertIn("guest: collect: 0 files in", result.stdout)
        self.assertEqual(self.journal_lines()[-1], "powerdown")
        self.assertIn("install failed", (self.work / "verdict.txt").read_text())
        self.assert_clean()

    def test_a_collection_that_fails_is_reported_without_changing_the_verdict(self):
        self.script(stages={"sign": [0], "install": [0], "check": [0], "collect": [5]})
        result = self.run_suite()
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("guest: collect failed: exited 5", result.stdout)
        self.script(stages={"sign": [0], "install": [0], "check": [0]},
                    outputs={"check": ONE_MONITOR, "rvktest": PASSING_REPORT,
                             "collect": "file C:\\rvk\\diag\\absent.txt\n"})
        result = self.run_suite()
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("guest: collect.pull failed: guest agent error: no file C:\\rvk\\diag\\absent.txt", result.stdout)
        self.assert_clean()

    def test_a_report_with_failures_or_without_its_summary_fails_the_run(self):
        self.script(rvktest=2, outputs={"check": ONE_MONITOR, "rvktest": "FAIL submission.wait\nrvktest: 21 passed, 2 failed\n"})
        result = self.run_suite()
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: report failed: 2 step(s) failed, exit 2", result.stdout)
        self.assert_clean()
        self.script(outputs={"check": ONE_MONITOR, "rvktest": "ok adapter.find\n"})
        result = self.run_suite()
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: report failed: no summary line (exit 0)", result.stdout)
        self.assert_clean()

    def test_a_live_dump_that_is_not_the_expected_black_screen_fails_the_run(self):
        """SC-005: a display TDR leaves a live kernel dump and recovers, so every
        step can still pass while the driver has just been reset. Only
        VIDEO_DXGKRNL_BLACK_SCREEN_LIVEDUMP is expected - nothing renders into
        the primary until features 042-044 - and anything else fails the run."""
        dumps = dict(GUEST_FILES)
        dumps["C:\\Windows\\LiveKernelReports\\WATCHDOG\\WD-1.dmp"] = live_dump(0x117)
        self.script(files=dumps)
        result = self.run_suite()
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: dumps failed: WD-1.dmp 0x117 VIDEO_TDR_TIMEOUT_DETECTED", result.stdout)
        self.assertIn("guest: report ok: 23 passed", result.stdout)
        self.assert_clean()

    def test_a_dump_the_runner_cannot_read_is_not_taken_for_a_black_screen(self):
        dumps = dict(GUEST_FILES)
        dumps["C:\\Windows\\LiveKernelReports\\WATCHDOG\\WD-1.dmp"] = "truncated"
        self.script(files=dumps)
        result = self.run_suite()
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: dumps failed: WD-1.dmp is not a kernel dump", result.stdout)
        self.assert_clean()

    def test_a_bugcheck_is_named_and_its_kernel_dump_pulled_after_the_guest_reboots(self):
        self.script(panic={"during": "rvktest", "code": 0xD1},
                    outputs={"check": ONE_MONITOR, "rvktest": PASSING_REPORT, "collect": WITH_MEMORY_DUMP})
        result = self.run_suite("--memory-dump")
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: rvktest failed: timed out waiting for an answer", result.stdout)
        named = "bugcheck 0xd1 DRIVER_IRQL_NOT_LESS_OR_EQUAL (0x10, 0x2, 0x0, 0xfffff80)"
        self.assertIn(f"guest: panic: {named}", result.stdout)
        self.assertEqual((self.work / "panic.txt").read_text(), named + "\n")
        self.assertIn("guest: panic.reset ok", result.stdout)
        self.assertEqual(self.stage_calls()[-1], "collect -MemoryDump")
        self.assertEqual((self.work / "diag" / "dumps" / "MEMORY.DMP").read_bytes(), live_dump(0xD1).encode("latin-1"))
        self.assertNotIn("rescue", result.stdout)
        self.assertEqual(self.journal_lines()[-1], "powerdown")
        self.assert_clean()

    def test_a_guest_that_bugchecks_on_every_boot_is_rescued_without_the_device_for_its_dumps(self):
        self.script(panic={"during": "check", "code": 0x3B, "loop": True},
                    outputs={"collect": WITH_MEMORY_DUMP})
        result = self.run_suite("--memory-dump")
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: check failed: timed out waiting for an answer", result.stdout)
        self.assertIn("guest: panic: bugcheck 0x3b SYSTEM_SERVICE_EXCEPTION", result.stdout)
        self.assertIn("guest: collect.agent failed: the guest panicked while booting", result.stdout)
        self.assertIn("guest: rescue: booting the overlay without virtio-gpu-rvk-pci", result.stdout)
        launches = [line for line in self.journal_lines() if line.startswith("argv")]
        self.assertEqual(len(launches), 2)
        self.assertIn("virtio-gpu-rvk-pci", launches[0])
        self.assertNotIn("virtio-gpu-rvk-pci", launches[1])
        self.assertIn("rvkgpu-rescue.log", launches[1])
        self.assertTrue((self.work / "qemu-rescue.log").exists())
        self.assertIn("guest: rescue.boot ok", result.stdout)
        self.assertEqual((self.work / "diag" / "dumps" / "MEMORY.DMP").read_bytes(), live_dump(0xD1).encode("latin-1"))
        self.assertEqual(self.journal_lines()[-1], "powerdown")
        self.assert_clean()

    def test_the_kernel_dump_is_left_alone_unless_it_is_asked_for(self):
        """MEMORY.DMP is copied through the agent in base64 chunks, so on a real
        guest it outlasts the step timeout and the failing pull takes the rest of
        the manifest with it. Four bring-up runs of feature 037 lost their
        minidumps that way; the kernel dump is now opt-in."""
        self.script(panic={"during": "rvktest", "code": 0xD1},
                    outputs={"check": ONE_MONITOR, "rvktest": PASSING_REPORT, "collect": COLLECTED})
        result = self.run_suite()
        self.assertEqual(result.returncode, 1)
        self.assertEqual(self.stage_calls()[-1], "collect")
        self.assertFalse((self.work / "diag" / "dumps" / "MEMORY.DMP").exists())
        self.assertEqual((self.work / "diag" / "dumps" / "WD-1.dmp").read_bytes(), BLACK_SCREEN.encode("latin-1"))
        self.assert_clean()

    def test_a_rescue_that_cannot_boot_either_is_reported(self):
        self.script(panic={"during": "check", "loop": True})
        environment = dict(self.environment, FAKE_QEMU_RESCUE_FAILS="1")
        result = self.run_suite(environment=environment)
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: rescue.boot failed: QEMU exited 1 before QMP listened", result.stdout)
        self.assertFalse((self.work / "diag" / "dumps" / "MEMORY.DMP").exists())
        self.assert_clean()

    def test_pausing_on_a_panic_leaves_the_guest_at_the_bugcheck_uncollected(self):
        self.script(panic={"during": "rvktest", "code": 0x116})
        result = self.run_suite("--on-panic", "pause")
        self.assertEqual(result.returncode, 1)
        self.assertIn("-action panic=pause", self.journal_lines()[0])
        self.assertIn("guest: panic: bugcheck 0x116 VIDEO_TDR_FAILURE", result.stdout)
        self.assertIn("guest: panic: the guest is paused at the bugcheck", result.stdout)
        self.assertNotIn("collect", self.stage_calls())
        self.assertNotIn("powerdown", self.journal_lines())
        self.assert_clean()

    def test_an_unnamed_bugcheck_or_a_panic_without_parameters_is_still_reported(self):
        self.assertEqual(describe_panic({"action": "pause"}),
                         "the guest panicked (action pause) without bugcheck parameters")
        self.assertEqual(describe_panic({"info": {"type": "hyper-v", "arg1": 0x1234}}),
                         "bugcheck 0x1234 unnamed (0x0, 0x0, 0x0, 0x0)")

    def test_holding_a_failed_guest_waits_for_enter_then_powers_it_off_with_debugging_attached(self):
        self.script(rvktest=1, outputs={"rvktest": "FAIL x\nrvktest: 0 passed, 1 failed\n"})
        result = self.run_suite("--hold", "--gdb", "1234", "--vnc", "7", "--trace", "virtio_gpu_*", stdin="\n")
        self.assertEqual(result.returncode, 1)
        argv = self.journal_lines()[0]
        self.assertIn("-gdb tcp:127.0.0.1:1234", argv)
        self.assertIn("-vnc 127.0.0.1:7", argv)
        self.assertIn("-trace enable=virtio_gpu_*", argv)
        self.assertIn("guest: hold: the guest is left running", result.stdout)
        self.assertIn("target remote 127.0.0.1:1234", result.stdout)
        self.assertIn("127.0.0.1:5907", result.stdout)
        self.assertIn("guest: hold.agent ok", result.stdout)
        self.assertEqual(self.journal_lines()[-1], "powerdown")
        self.assert_clean()

    def test_preparing_an_image_boots_it_directly_runs_prepare_through_its_reboot_and_powers_off(self):
        self.script(stages={"prepare": [3010, 0]})
        result = self.run_suite("--prepare-image", "--driver", self.scratch / "absent.sys")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        lines = self.journal_lines()
        self.assertIn(f"file={self.image} ", lines[0])
        self.assertNotIn("overlay.qcow2", lines[0])
        self.assertEqual([line.split()[1] for line in lines if line.startswith("file")], ["C:\\rvk\\provision.ps1"])
        self.assertEqual(self.stage_calls(), ["prepare", "prepare"])
        self.assertEqual(lines[-1], "powerdown")
        self.assertIn("guest: prepare.reboot ok", result.stdout)
        self.assertFalse((self.work / "symbols").exists())
        self.script(stages={"prepare": [3010]})
        result = self.run_suite("--prepare-image")
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: prepare failed: still asks for a reboot after rebooting", result.stdout)

    def test_qemu_that_dies_at_boot_or_a_guest_agent_that_never_answers_fails_the_boot(self):
        self.script(exit_at_boot=True)
        result = self.run_suite()
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: boot failed: QEMU exited 1 before QMP listened", result.stdout)
        self.assertIn("guest: qemu exited 1: nothing in qemu.log", result.stdout)
        self.assert_clean()
        self.script(agent_silent=True)
        result = self.run_suite("--boot-timeout", "3")
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: boot failed: the guest agent never answered", result.stdout)
        self.assertIn("guest: collect.agent failed: the guest agent never answered", result.stdout)
        self.assert_clean()

    def test_qemu_that_dies_mid_run_is_reported_with_its_last_words_and_nothing_is_collected(self):
        self.script(die_during="install")
        result = self.run_suite()
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: install failed: connection closed", result.stdout)
        self.assertIn("guest: qemu exited on SIGKILL: qemu-system-x86_64: ../source/hw/pci/msix.c:122: "
                      "msix_fire_vector_notifier: Assertion `ret >= 0' failed.", result.stdout)
        self.assertNotIn("collect", self.stage_calls())
        self.assert_clean()

    def test_a_hung_process_or_a_reboot_that_never_resets_is_timed_out_by_name(self):
        self.script(hang="check")
        result = self.run_suite("--step-timeout", "2")
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: check failed: ", result.stdout)
        self.assertIn("still running after 2.0 s", result.stdout)
        self.assert_clean()
        self.script(no_reset=True)
        result = self.run_suite("--boot-timeout", "2")
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: sign.reset failed: timed out waiting for an answer", result.stdout)
        self.assert_clean()

    def start_suite(self, *extra, stdin=subprocess.DEVNULL):
        """run_suite's command, left running for a test to act on the guest meanwhile."""
        arguments = ["--image", self.image, "--work", self.work, "--power-poll", "0.1",
                     "--qemu", FAKES / "fake_qemu.py",
                     "--qemu-img", FAKES / "fake_qemu_img.py", "--executor", "/nowhere/librvk_echo_executor.so",
                     "--driver", self.scratch / "rvkgpu.sys", "--test", self.scratch / "rvktest.exe",
                     "--inspect", self.scratch / "rvkinspect.exe",
                     "--icd", self.scratch / "rvkvk.dll", "--icd-manifest", self.scratch / "rvkvk.json",
                     "--gl-icd", self.scratch / "rvkgl.dll",
                     "--firmware", self.scratch / "OVMF_CODE.fd", "--firmware-vars", self.scratch / "OVMF_VARS.fd",
                     "--boot-timeout", "10", "--step-timeout", "5", "--agent-timeout", "2", *extra]
        runner = subprocess.Popen([sys.executable, "-B", str(RUNNER), *map(str, arguments)], stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True, env=self.environment,
                                  stdin=stdin)
        self.addCleanup(lambda: runner.poll() is None and runner.kill())
        return runner

    def wait_for_stage(self, name, runner):
        deadline = time.monotonic() + 30
        while f"-Stage {name}" not in "\n".join(self.journal_lines()):
            self.assertIsNone(runner.poll(), "the run ended before the stage")
            self.assertLess(time.monotonic(), deadline, f"the run never reached {name}")
            time.sleep(0.05)

    def test_the_agent_relay_serves_the_guest_while_the_run_holds_its_agent(self):
        """docs/trace.md: tools/mcp/rvk_inspect.py reaches a guest a run holds
        through <work>/agent-relay.sock, one answer per request line, without
        disturbing the run's own exchanges."""
        self.script(hang="check", reg=7, answer_delay=0.02)
        runner = self.start_suite("--step-timeout", "8")
        self.wait_for_stage("check", runner)
        relay = self.work / "agent-relay.sock"
        self.assertTrue(relay.exists())
        client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        client.settimeout(10)
        client.connect(str(relay))
        replies = client.makefile("rb")

        def ask(request):
            client.sendall((request if isinstance(request, bytes) else json.dumps(request).encode()) + b"\n")
            return json.loads(replies.readline())

        self.assertEqual(ask({"execute": "guest-ping"}), {"return": {}})
        started = ask({"execute": "guest-exec", "arguments": {"path": "C:\\Windows\\System32\\reg.exe",
                                                              "arg": ["query", "HKLM"], "capture-output": True}})
        pid = started["return"]["pid"]
        # The run polls its hung stage all along; every relayed answer is still this request's.
        for _ in range(40):
            status = ask({"execute": "guest-exec-status", "arguments": {"pid": pid}})
            self.assertEqual((status["return"]["exited"], status["return"]["exitcode"]), (True, 7))
        self.assertEqual(ask(b"{not json")["error"]["class"], "JSONParsing")
        self.assertEqual(ask({"execute": 5})["error"]["class"], "InvalidParameter")
        self.assertEqual(ask([1, 2])["error"]["class"], "InvalidParameter")
        for refused in ("guest-shutdown", "guest-sync-delimited", "guest-suspend-disk"):
            self.assertEqual(ask({"execute": refused, "arguments": {"mode": "powerdown"}})["error"]["class"],
                             "Refused")
        self.assertEqual(ask({"execute": "guest-no-such"})["error"]["class"], "CommandNotFound")
        self.assertEqual(ask({"execute": "guest-ping"}), {"return": {}})
        # A client gone mid-request leaves the relay serving the next one.
        dropped = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        dropped.connect(str(relay))
        dropped.sendall(json.dumps({"execute": "guest-ping"}).encode() + b"\n")
        dropped.close()
        guest = rvk_inspect.Guest(self.work)
        self.assertEqual(guest.via, "relay")
        self.assertEqual(guest.agent.execute("guest-ping"), {})
        guest.close()
        replies.close()
        client.close()
        output, _ = runner.communicate(timeout=120)
        self.assertEqual(runner.returncode, 1, output)
        self.assertIn("guest: check failed: ", output)
        self.assertIn("still running after 8.0 s", output)
        self.assertFalse(relay.exists())
        self.assertEqual(self.stage_calls()[-1], "collect")
        self.assert_clean()

    def test_a_held_guest_gives_its_agent_socket_back_and_the_relay_goes(self):
        self.script(rvktest=1, outputs={"rvktest": "FAIL x\nrvktest: 0 passed, 1 failed\n"})
        runner = subprocess.Popen([sys.executable, "-B", str(RUNNER), "--image", str(self.image),
                                   "--work", str(self.work), "--power-poll", "0.1",
                                   "--qemu", str(FAKES / "fake_qemu.py"), "--qemu-img", str(FAKES / "fake_qemu_img.py"),
                                   "--executor", "/nowhere/librvk_echo_executor.so",
                                   "--driver", str(self.scratch / "rvkgpu.sys"),
                                   "--test", str(self.scratch / "rvktest.exe"),
                                   "--inspect", str(self.scratch / "rvkinspect.exe"),
                                   "--icd", str(self.scratch / "rvkvk.dll"),
                                   "--gl-icd", str(self.scratch / "rvkgl.dll"),
                                   "--icd-manifest", str(self.scratch / "rvkvk.json"),
                                   "--firmware", str(self.scratch / "OVMF_CODE.fd"),
                                   "--firmware-vars", str(self.scratch / "OVMF_VARS.fd"),
                                   "--boot-timeout", "10", "--step-timeout", "5", "--agent-timeout", "2", "--hold"],
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                  env=self.environment)
        self.addCleanup(lambda: runner.poll() is None and runner.kill())
        held = []
        while True:
            line = runner.stdout.readline()
            self.assertTrue(line, "the run ended without holding: " + "".join(held))
            held.append(line)
            if "press Enter" in line:
                break
        self.assertFalse((self.work / "agent-relay.sock").exists())
        guest = rvk_inspect.Guest(self.work)
        self.assertEqual(guest.via, "agent")
        self.assertEqual(guest.agent.execute("guest-ping"), {})
        guest.close()
        output, _ = runner.communicate("\n", timeout=120)
        self.assertEqual(runner.returncode, 1, output)
        self.assertIn("guest: hold.agent ok", output)
        self.assert_clean()

    def test_a_kept_guest_is_held_after_a_passing_run(self):
        """--keep holds whatever the verdict, so a passing guest can be inspected
        (docs/trace.md); the run still answers its verdict."""
        self.script()
        runner = self.start_suite("--keep", stdin=subprocess.PIPE)
        self.addCleanup(lambda: runner.poll() is None and runner.kill())
        held = []
        while True:
            line = runner.stdout.readline()
            self.assertTrue(line, "the run ended without holding: " + "".join(held))
            held.append(line)
            if "press Enter" in line:
                break
        self.assertIn("guest: hold: the guest is left running\n", held)
        output, _ = runner.communicate("\n", timeout=120)
        self.assertEqual(runner.returncode, 0, "".join(held) + output)
        self.assertIn("guest: hold.agent ok", output)
        self.assert_clean()

    def test_an_overlay_qemu_img_refuses_or_a_missing_input_stops_before_qemu(self):
        self.script()
        refused = self.run_suite(environment=dict(self.environment, FAKE_QEMU_IMG_FAIL="1"))
        self.assertEqual(refused.returncode, 1)
        self.assertIn("guest: overlay failed: qemu-img: Could not open backing file", refused.stdout)
        self.assertEqual(self.journal_lines(), [])
        missing = self.run_suite("--test", self.scratch / "absent.exe")
        self.assertEqual(missing.returncode, 1)
        self.assertIn("absent.exe does not exist", missing.stdout)
        no_gl = self.run_suite("--gl-icd", self.scratch / "nogl.dll")
        self.assertEqual(no_gl.returncode, 1)
        self.assertIn("guest: prepare failed: " + str(self.scratch / "nogl.dll") + " does not exist", no_gl.stdout)
        uninspected = self.run_suite("--inspect", self.scratch / "noinspect.exe")
        self.assertEqual(uninspected.returncode, 1)
        self.assertIn("guest: prepare failed: " + str(self.scratch / "noinspect.exe") + " does not exist",
                      uninspected.stdout)
        unbuilt = self.run_suite("--icd", self.scratch / "absent.dll")
        self.assertEqual(unbuilt.returncode, 1)
        self.assertIn("guest: prepare failed: " + str(self.scratch / "absent.dll") + " does not exist", unbuilt.stdout)
        self.assertEqual(self.journal_lines(), [])
        self.assert_clean()

    def test_the_display_step_asserts_the_host_scans_out_the_guest_s_primary(self):
        """What a screendump can prove is that the host reads our resource.

        The picture itself is not a desktop yet - nothing executes GPU work
        until features 042-044 - so the step asserts the committed size, that
        the dump is not QEMU's placeholder, and that the driver recorded the
        primary's aperture pages and refused no frame.
        """
        self.script(screendump=[1280, 800])
        result = self.run_suite()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("display ok: 1 head(s) of 1280x800 scanned out of the guest's primaries", result.stdout)
        self.assertEqual(Screendump(self.work / "screen0.ppm").size(), (1280, 800))
        self.assertIn("screendump 1280x800", self.journal_lines())

    def test_a_mode_the_driver_offers_must_reach_the_guest(self):
        """US2: the modes the run works with are ones the guest can choose.
        What Windows adds of its own is not the driver's, so only the absence
        of one of ours fails the run."""
        self.script(screendump=[1280, 800], modes=["1280x800", "1920x1080"])
        result = self.run_suite()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("the guest is not offered ['1024x768']", (self.work / "verdict.txt").read_text())

    def test_the_drivers_own_vertical_blank_rate_is_checked(self):
        """US3: a waiter in the guest cannot see the driver's rate - the blank
        is a pulse it can miss - so the line the driver logs is read here."""
        self.script(screendump=[1280, 800],
                    driver_log=["rvkgpu: aperture map segment 1 offset 0 pages 1000 status 0",
                                "rvkgpu: vsync 600 periods in 200000000 ticks, 600 reported, enabled 0 times"])
        result = self.run_suite("--vsync-wait", "0")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("the driver reports 30.00 vertical blanks a second, not 60 within 5%",
                      (self.work / "verdict.txt").read_text())

    def test_a_driver_that_logged_no_vertical_blank_window_fails_the_run(self):
        self.script(screendump=[1280, 800],
                    driver_log=["rvkgpu: aperture map segment 1 offset 0 pages 1000 status 0"])
        result = self.run_suite("--vsync-wait", "0")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("the driver logged no vertical blank window", (self.work / "verdict.txt").read_text())

    def test_the_fastest_window_is_the_drivers_rate(self):
        """A window dxgkrnl turned the blanks off in is slower than the driver's
        period; the fastest window says what the period is."""
        self.script(screendump=[1280, 800],
                    driver_log=["rvkgpu: aperture map segment 1 offset 0 pages 1000 status 0",
                                "rvkgpu: vsync 120 periods in 700000000 ticks, 120 reported, enabled 120 times",
                                "rvkgpu: vsync 120 periods in 20000000 ticks, 120 reported, enabled 0 times"])
        result = self.run_suite()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("guest: vsync ok: 60.00 blanks a second", result.stdout)

    def test_a_window_at_the_drivers_period_is_waited_for(self):
        """The driver says its rate once every 120 blanks, and after a resume
        the windows already in the log are the ones dxgkrnl kept the blanks off
        through. The run waits for one at the period rather than judging the
        log the instant the suite ends (feature 040, US1)."""
        self.script(screendump=[1280, 800],
                    driver_log=["rvkgpu: aperture map segment 1 offset 0 pages 1000 status 0",
                                "rvkgpu: vsync 120 periods in 700000000 ticks, 120 reported, enabled 120 times"],
                    driver_log_late=["rvkgpu: vsync 120 periods in 20000000 ticks, 120 reported, enabled 120 times"],
                    driver_log_late_seconds=1.5)
        result = self.run_suite("--vsync-wait", "20")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("guest: vsync ok: 60.00 blanks a second", result.stdout)

    def test_a_switch_the_guest_refuses_must_leave_the_scanout_alone(self):
        """The console session's display is not the session 0 service's to
        change; a refusal must not disturb what the host scans out."""
        self.script(screendump=[1280, 800], setmode_refused=True)
        result = self.run_suite()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("setmode ok: 1024x768 refused, and the console session kept", result.stdout)

    def test_a_switch_the_host_does_not_follow_fails_the_run(self):
        """The guest took the mode; if the host still scans out the old size,
        the driver never told it."""
        self.script(screendump=[1280, 800], screendump_fixed=True)
        result = self.run_suite()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("the host scans out 1280x800 after a switch to 1024x768",
                      (self.work / "verdict.txt").read_text())

    def test_a_mode_the_driver_never_offered_must_be_refused(self):
        """A guest that takes 2560x1600 committed a mode the driver does not
        have, and the host would scan out a rectangle it does not have."""
        self.script(screendump=[1280, 800],
                    modes=["640x480", "800x600", "1024x768", "1280x720", "1280x800", "2560x1600"])
        result = self.run_suite()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("the guest took 2560x1600, which the driver never offered",
                      (self.work / "verdict.txt").read_text())

    def test_a_guest_with_the_wrong_monitor_count_fails_the_run(self):
        """US4: Windows attaches a monitor to every output the device reports."""
        self.script(screendump=[1280, 800],
                    outputs={"check": "display: dwm=1 services=rvkgpu monitors=2\n",
                             "rvktest": PASSING_REPORT, "collect": COLLECTED})
        result = self.run_suite()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("the guest has 2 monitors, not 1", (self.work / "verdict.txt").read_text())

    def test_a_check_stage_that_counted_no_monitors_fails_the_run(self):
        self.script(screendump=[1280, 800],
                    outputs={"check": "device: PCI\\VEN_1AF4 status=OK\n",
                             "rvktest": PASSING_REPORT, "collect": COLLECTED})
        result = self.run_suite()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("did not count the guest's monitors", (self.work / "verdict.txt").read_text())

    def test_two_outputs_are_each_scanned_out(self):
        """US4: with max_outputs=2 the desktop is extended and each head shows
        the committed size."""
        self.script(screendump=[1280, 800],
                    outputs={"check": "display: dwm=1 services=rvkgpu monitors=2\n",
                             "rvktest": PASSING_REPORT, "collect": COLLECTED})
        result = self.run_suite("--outputs", "2")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("extend ok: the desktop is across 2 displays", result.stdout)
        self.assertIn("display ok: 2 head(s) of 1280x800", result.stdout)
        self.assertEqual(Screendump(self.work / "screen1.ppm").size(), (1280, 800))
        self.assertIn("max_outputs=2", self.journal_lines()[0])
        self.assertIn("screendump 1280x800 head 1", self.journal_lines())

    def test_a_desktop_that_will_not_extend_fails_the_run(self):
        """A guest that refuses the topology for a reason of its own is a
        failure; only the console session's access is not."""
        self.script(screendump=[1280, 800],
                    outputs={"check": "display: dwm=1 services=rvkgpu monitors=2\n", "extend": "extend: -1\n",
                             "rvktest": PASSING_REPORT, "collect": COLLECTED})
        result = self.run_suite("--outputs", "2")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("the guest answered '-1' extending the desktop", (self.work / "verdict.txt").read_text())

    def test_a_topology_the_session_0_service_may_not_change_asserts_the_first_head(self):
        """No user is logged in, so SetDisplayConfig answers ERROR_ACCESS_DENIED
        and the displays past the first stay dark - the run says so."""
        self.script(screendump=[1280, 800],
                    outputs={"check": "display: dwm=1 services=rvkgpu monitors=2\n", "extend": "extend: 5\n",
                             "rvktest": PASSING_REPORT, "collect": COLLECTED})
        result = self.run_suite("--outputs", "2")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("not a session 0 service's to change", result.stdout)
        self.assertIn("display ok: 1 head(s) of 1280x800", result.stdout)

    def test_the_display_goes_dark_by_policy_and_comes_back_with_a_key(self):
        """US4: the power policy turns the display off without a session, and the
        host scans out the same mode again once the guest sees input."""
        self.script(screendump=[1280, 800])
        result = self.run_suite()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("power ok: the display went dark and came back at 1280x800", result.stdout)
        self.assertIn("input-send-event", self.journal_lines())
        self.assertIn("exec C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe -NoProfile "
                      "-ExecutionPolicy Bypass -File C:\\rvk\\provision.ps1 -Stage monitortimeout -Minutes 0",
                      self.journal_lines())

    def test_a_display_that_never_goes_dark_fails_the_run(self):
        self.script(screendump=[1280, 800], power_stays_lit=True)
        result = self.run_suite()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("the display never went dark", (self.work / "verdict.txt").read_text())

    def test_a_scanout_of_the_wrong_size_fails_the_run(self):
        self.script(screendump=[640, 480])
        result = self.run_suite()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("the host scans out 640x480 on head 0, not 1280x800", (self.work / "verdict.txt").read_text())

    def test_qemus_placeholder_is_not_a_scanout(self):
        """QEMU draws "display output is not active" in grey on black when no
        scanout is programmed; that is the absence of a frame, not a frame."""
        self.script(screendump=[1280, 800], screendump_placeholder=True)
        result = self.run_suite()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("head 0 scans out QEMU's placeholder", (self.work / "verdict.txt").read_text())

    def test_a_primary_with_no_recorded_pages_fails_the_run(self):
        """Without DXGK_OPERATION_MAP_APERTURE_SEGMENT the driver has no pages
        to attach, and the host reads whatever sits at the aperture address."""
        self.script(screendump=[1280, 800], driver_log=["rvkgpu: DriverEntry"])
        result = self.run_suite()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("aperture pages were never recorded", (self.work / "verdict.txt").read_text())

    def test_a_refused_frame_fails_the_run(self):
        self.script(screendump=[1280, 800],
                    driver_log=["rvkgpu: aperture map segment 1 offset 0 pages 1000 status 0",
                                "rvkgpu: scanout 0 address 0x00000000c0000000 pages 0 runs 0 needs 1000 status 1"])
        result = self.run_suite()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("a frame was not carried: rvkgpu: scanout 0", (self.work / "verdict.txt").read_text())

    def test_the_device_is_given_a_host_visible_region_and_the_driver_reports_two_segments(self):
        """Feature 041: the device only has a region where the run asks for one,
        and the driver's own log is where a run sees the second segment."""
        self.script()
        result = self.run_suite()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("max_outputs=1,vectors=11,hostmem=256M", self.journal_lines()[0])
        self.assertIn("guest: segments ok: 2 reported, the second 256 MiB of host memory at 0xc0000000",
                      result.stdout)
        # A suite that wrote no pattern is said and skipped, not failed: the
        # scenario is the guest's to run.
        self.assertIn("guest: hostvisible skipped: the suite reported no host-visible pattern", result.stdout)
        self.assert_clean()

    def test_hostmem_zero_leaves_the_device_without_a_region(self):
        """FR-007: the fallback is what a run with no region is for - one
        segment, and a pattern in guest memory that proves nothing."""
        self.script(outputs={"check": ONE_MONITOR, "sleepstates": SLEEP_STATES,
                             "rvktest": HOST_VISIBLE_REPORT, "collect": COLLECTED})
        result = self.run_suite("--hostmem", "0")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("hostmem=", self.journal_lines()[0])
        self.assertIn("guest: segments ok: 1 reported, this device has no region", result.stdout)
        self.assertIn("guest: hostvisible ok: 4096 bytes written through the guest's view", result.stdout)
        self.assertIn("guest: hostvisible skipped: with no region on this device", result.stdout)
        self.assertNotIn("pmemsave", "\n".join(self.journal_lines()))
        self.assert_clean()

    def test_record_writes_a_wire_recording_beside_the_qemu_log(self):
        """Feature 065, FR-001: the device is told where to write down what
        crosses the wire, and the run says where it went."""
        self.script()
        result = self.run_suite("--record", "--record-limit", "1048576")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        device = self.journal_lines()[0]
        self.assertIn(f"record={self.work / 'recording.rvk'}", device)
        self.assertIn("record-limit=1048576", device)
        self.assertIn(f"guest: recording what crosses the wire into {self.work / 'recording.rvk'}",
                      result.stdout)
        self.assert_clean()

    def test_a_run_without_record_asks_the_device_for_no_recording(self):
        self.script()
        result = self.run_suite()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("record=", self.journal_lines()[0])
        self.assertNotIn("recording what crosses the wire", result.stdout)
        self.assert_clean()

    def test_a_driver_that_opened_no_region_fails_the_segments_step(self):
        """The device has a region and the driver reported none: the capset's
        host-visible bit is clear, or opening it failed - either way the feature
        did not happen and the run says which."""
        self.script(region_status=None)
        result = self.run_suite()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("the driver opened no host-visible region", (self.work / "verdict.txt").read_text())
        self.script(region_status="0xc0000017")
        result = self.run_suite()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("could not open the host-visible region: status 0xc0000017",
                      (self.work / "verdict.txt").read_text())
        self.assert_clean()

    def test_the_host_reads_back_what_the_guest_wrote_through_its_view(self):
        """SC-003: the guest wrote its pattern through the view it locked, and
        the host's own read of the BAR holds the same bytes - at whatever offset
        VidMm placed the allocation at, which the driver is never told."""
        self.script(outputs={"check": ONE_MONITOR, "sleepstates": SLEEP_STATES,
                             "rvktest": HOST_VISIBLE_REPORT, "collect": COLLECTED},
                    hostmem_written={"offset": 0x2000, "bytes": 4096})
        result = self.run_suite("--hostmem", "4")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("guest: hostvisible ok: 4096 bytes written through the guest's view", result.stdout)
        self.assertIn("guest: hostvisible ok: the host reads the same bytes out of the BAR", result.stdout)
        self.assertIn("pmemsave 0xc0000000 4194304", self.journal_lines())
        self.assertFalse((self.work / "hostmem.bin").exists())
        self.assert_clean()

    def test_a_pattern_the_host_cannot_find_is_a_skip_until_something_asks_for_host_memory(self):
        """The driver reports the segment and asks VidMm for nothing in it - a
        live guest refused every allocation that preferred it - so the pattern
        is in guest memory and the host cannot find it. That is where feature
        041 landed: a skip with the reason, not a failure (T026)."""
        self.script(outputs={"check": ONE_MONITOR, "sleepstates": SLEEP_STATES,
                             "rvktest": HOST_VISIBLE_REPORT, "collect": COLLECTED})
        result = self.run_suite("--hostmem", "4")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("nothing asks for host memory yet (feature 043)", result.stdout)
        self.assert_clean()

    def test_a_pattern_the_host_cannot_find_in_the_bar_fails_the_run(self):
        """With --require-host-visible, which feature 043 turns on once it has a
        client: the guest's own read-back passed and the host's did not, so the
        view was guest RAM with a flag on it, not the device's memory."""
        self.script(outputs={"check": ONE_MONITOR, "sleepstates": SLEEP_STATES,
                             "rvktest": HOST_VISIBLE_REPORT, "collect": COLLECTED})
        result = self.run_suite("--hostmem", "4", "--require-host-visible")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("does not hold the 4096 bytes the guest wrote", (self.work / "verdict.txt").read_text())
        self.script(outputs={"check": ONE_MONITOR, "sleepstates": SLEEP_STATES,
                             "rvktest": HOST_VISIBLE_REPORT, "collect": COLLECTED},
                    hostmem_written={"offset": 0, "bytes": 4096}, pmemsave_nothing=True)
        result = self.run_suite("--hostmem", "4", "--require-host-visible")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("QEMU wrote no 4194304-byte read of 0xc0000000", (self.work / "verdict.txt").read_text())
        self.assert_clean()

    def test_without_an_image_it_says_how_to_name_one(self):
        environment = {key: value for key, value in self.environment.items() if key != "AV_GUEST_IMAGE"}
        result = subprocess.run([sys.executable, "-B", str(RUNNER), "--work", str(self.work)], capture_output=True,
                                text=True, env=environment)
        self.assertEqual(result.returncode, 2)
        self.assertIn("no image: pass --image or set AV_GUEST_IMAGE", result.stderr)


class VulkaninfoListing(unittest.TestCase):
    """What the runner reads out of vulkaninfo: tolerant of how the text is
    laid out, never of how many devices it names."""

    def test_each_physical_device_section_carries_its_own_luid(self):
        listing = VULKANINFO + vulkaninfo_gpu(1, "0b5e0100-00000000", "llvmpipe")
        self.assertEqual(vulkaninfo_devices(listing), {0: ["a1d30000-00000000"], 1: ["0b5e0100-00000000"]})

    def test_layout_may_change_without_changing_what_is_read(self):
        listing = ("Device Properties and Extensions:\n  GPU0:  \n    VkPhysicalDeviceIDProperties:\n"
                   "        deviceLUID: \"a1-d3-00-00-00-00-00-00\"\n        deviceLUIDValid: true\n")
        devices = vulkaninfo_devices(listing)
        self.assertEqual(list(devices), [0])
        self.assertEqual(luid_low_part(devices[0][0]), ADAPTER_LUID)

    def test_lines_that_only_mention_a_gpu_are_not_devices(self):
        """Device groups and presentable surfaces name devices by id; a device
        section header is the name alone on its line."""
        listing = ("Presentable Surfaces:\r\n\tGPU id : 0 (rvk) :\r\n\tGPU0 (rvk) (ID: 0)\r\n"
                   "\tdeviceLUIDValid = true\r\n" + vulkaninfo_gpu(0))
        self.assertEqual(vulkaninfo_devices(listing), {0: ["a1d30000-00000000"]})

    def test_a_device_named_twice_is_one_device(self):
        listing = VULKANINFO + "Devices:\r\n========\r\nGPU0:\r\n\tdeviceName = rvk\r\n"
        self.assertEqual(vulkaninfo_devices(listing), {0: ["a1d30000-00000000"]})

    def test_a_luid_is_eight_bytes_lowpart_first(self):
        self.assertEqual(luid_low_part("a1d30000-00000000"), 0xD3A1)
        self.assertEqual(luid_low_part("78563412-ffffffff"), 0x12345678)
        self.assertIsNone(luid_low_part("a1d30000"))
        self.assertIsNone(luid_low_part("a1d30000-0000000000"))
        self.assertIsNone(luid_low_part("g1d30000-00000000"))


if __name__ == "__main__":
    unittest.main()


class ScreendumpTest(unittest.TestCase):
    """The PPM QEMU writes, read back: its size, and whether two differ."""

    def setUp(self):
        self.scratch = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch, ignore_errors=True)

    def ppm(self, name, width, height, pixel=b"\x10\x20\x30"):
        path = self.scratch / name
        path.write_bytes(f"P6\n{width} {height}\n255\n".encode() + pixel * (width * height))
        return path

    def test_size_reads_the_ppm_header(self):
        self.assertEqual(Screendump(self.ppm("a.ppm", 1280, 800)).size(), (1280, 800))

    def test_a_dump_that_is_not_a_ppm_has_no_size(self):
        path = self.scratch / "bad.ppm"
        path.write_bytes(b"not a ppm at all")
        self.assertEqual(Screendump(path).size(), (0, 0))
        path.write_bytes(b"P6\nwide high\n255\n")
        self.assertEqual(Screendump(path).size(), (0, 0))
        path.write_bytes(b"P6\n12")
        self.assertEqual(Screendump(path).size(), (0, 0))

    def test_dumps_differ_when_a_pixel_does(self):
        first = Screendump(self.ppm("first.ppm", 4, 4))
        same = Screendump(self.ppm("same.ppm", 4, 4))
        other = Screendump(self.ppm("other.ppm", 4, 4, b"\x99\x20\x30"))
        bigger = Screendump(self.ppm("bigger.ppm", 8, 4))
        self.assertFalse(first.differs_from(same))
        self.assertTrue(first.differs_from(other))
        self.assertTrue(first.differs_from(bigger))

    def test_capture_asks_qemu_and_refuses_an_empty_dump(self):
        asked = []

        class Writing:
            def __init__(self, contents):
                self.contents = contents

            def execute(self, command, arguments=None):
                asked.append((command, arguments))
                if self.contents is not None:
                    pathlib.Path(arguments["filename"]).write_bytes(self.contents)

        path = self.scratch / "taken.ppm"
        dump = Screendump(path).capture(Writing(b"P6\n2 2\n255\n" + b"\x00\x00\x00" * 4))
        self.assertEqual(dump.size(), (2, 2))
        self.assertEqual(asked, [("screendump", {"filename": str(path)})])
        with self.assertRaises(StepFailed):
            Screendump(self.scratch / "missing.ppm").capture(Writing(None))
        with self.assertRaises(StepFailed):
            Screendump(self.scratch / "empty.ppm").capture(Writing(b""))


CTS_CASELIST = ("DEQP-VK\r\nTopLevel\r\n"
                "GROUP: api\r\nTEST: dEQP-VK.api.smoke.triangle\r\n"
                "TEST: dEQP-VK.api.smoke.create_sampler\r\n"
                "TEST: dEQP-VK.pipeline.enormous.case\r\n"
                "TEST: dEQP-VK.memory.budget.query\r\n")
"""What deqp-vk --deqp-runmode=stdout-caselist prints: groups, cases, and a case
this repository's subset does not name."""


def qpa_case(name, status):
    return (f"#beginTestCaseResult {name}\r\n"
            f'<Result StatusCode="{status}">{status}</Result>\r\n'
            "#endTestCaseResult\r\n")


def qpa(*pairs, ended=True):
    text = "#beginSession\r\n" + "".join(qpa_case(name, status) for name, status in pairs)
    return text + ("#endSession\r\n" if ended else "")
