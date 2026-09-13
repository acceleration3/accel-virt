"""tools/guest/run_guest_suite.py against a fake QEMU with a scripted guest:
every step, reboot, timeout and misbehaving agent; bugchecks named, their dumps
pulled after a reboot or from a rescue boot; image preparation; holding a guest
for debugging; and the promise that the base image is never written and the
overlay never outlives the run."""
import json
import os
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

from tool_run import ROOT

sys.path.insert(0, str(ROOT / "tools" / "guest"))
from run_guest_suite import Screendump, StepFailed, describe_panic  # noqa: E402

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
WITH_MEMORY_DUMP = COLLECTED + "dump C:\\Windows\\MEMORY.DMP\r\n"


class GuestSuiteRun(unittest.TestCase):
    def setUp(self):
        self.scratch = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)
        self.image = self.scratch / "windows.qcow2"
        self.image.write_bytes(b"prepared image")
        self.os_stat = os.stat(self.image)
        for name in ("rvkgpu.sys", "rvkgpu.pdb", "rvktest.exe", "OVMF_VARS.fd", "OVMF_CODE.fd"):
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
                      "max_outputs=1", argv)
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
        self.assertEqual(events.count("file"), 4)
        self.assertEqual(events.count("pull"), 2)
        self.assertIn("file C:\\rvk\\package\\rvkgpu.sys 1000", lines)
        self.assertIn("file C:\\rvk\\package\\rvkgpu.inf", "\n".join(lines))
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
        self.script()
        result = self.run_suite("--stall-ms", "5000", "--fault-after", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        environment = json.loads((self.work / "environment.json").read_text())
        self.assertEqual(environment.get("RVK_ECHO_STALL_MS"), "5000")
        self.assertEqual(environment.get("RVK_ECHO_FAULT_AFTER"), "3")
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
        self.script(stages={"sign": [3010], "install": [3010], "check": [0], "hibernate": [0],
                            "sleepstates": [0]})
        result = self.run_suite("--hibernate")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        journal = self.journal_lines()
        self.assertIn("guest-suspend-disk", journal)
        self.assertIn("guest: hibernate ok: the adapter came back from S4", result.stdout)
        self.assertIn("hibernate", " ".join(self.stage_calls()))
        self.assert_clean()

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

    def test_an_overlay_qemu_img_refuses_or_a_missing_input_stops_before_qemu(self):
        self.script()
        refused = self.run_suite(environment=dict(self.environment, FAKE_QEMU_IMG_FAIL="1"))
        self.assertEqual(refused.returncode, 1)
        self.assertIn("guest: overlay failed: qemu-img: Could not open backing file", refused.stdout)
        self.assertEqual(self.journal_lines(), [])
        missing = self.run_suite("--test", self.scratch / "absent.exe")
        self.assertEqual(missing.returncode, 1)
        self.assertIn("absent.exe does not exist", missing.stdout)
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
                                "rvkgpu: vsync 600 blanks in 200000000 ticks, enabled 0 times"])
        result = self.run_suite()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("the driver reports 30.00 vertical blanks a second, not 60 within 5%",
                      (self.work / "verdict.txt").read_text())

    def test_a_driver_that_logged_no_vertical_blank_window_fails_the_run(self):
        self.script(screendump=[1280, 800],
                    driver_log=["rvkgpu: aperture map segment 1 offset 0 pages 1000 status 0"])
        result = self.run_suite()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("the driver logged no vertical blank window", (self.work / "verdict.txt").read_text())

    def test_the_fastest_window_is_the_drivers_rate(self):
        """A window dxgkrnl turned the blanks off in is slower than the driver's
        period; the fastest window says what the period is."""
        self.script(screendump=[1280, 800],
                    driver_log=["rvkgpu: aperture map segment 1 offset 0 pages 1000 status 0",
                                "rvkgpu: vsync 120 blanks in 700000000 ticks, enabled 120 times",
                                "rvkgpu: vsync 120 blanks in 20000000 ticks, enabled 0 times"])
        result = self.run_suite()
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

    def test_without_an_image_it_says_how_to_name_one(self):
        environment = {key: value for key, value in self.environment.items() if key != "AV_GUEST_IMAGE"}
        result = subprocess.run([sys.executable, "-B", str(RUNNER), "--work", str(self.work)], capture_output=True,
                                text=True, env=environment)
        self.assertEqual(result.returncode, 2)
        self.assertIn("no image: pass --image or set AV_GUEST_IMAGE", result.stderr)


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
