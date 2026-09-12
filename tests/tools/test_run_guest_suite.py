"""tools/guest/run_guest_suite.py against a fake QEMU with a scripted guest:
every step, reboot, timeout and misbehaving agent; bugchecks named, their dumps
pulled after a reboot or from a rescue boot; image preparation; holding a guest
for debugging; and the promise that the base image is never written and the
overlay never outlives the run."""
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

from tool_run import ROOT

sys.path.insert(0, str(ROOT / "tools" / "guest"))
from run_guest_suite import describe_panic  # noqa: E402

RUNNER = ROOT / "tools" / "guest" / "run_guest_suite.py"
FAKES = ROOT / "tests" / "tools" / "fake_guest"
PASSING_REPORT = "ok adapter.find\nrvktest: 23 passed, 0 failed\n"
COLLECTED = "file C:\\rvk\\diag\\system.txt\r\ndump C:\\Windows\\LiveKernelReports\\WATCHDOG\\WD-1.dmp\r\n"
GUEST_FILES = {"C:\\rvk\\diag\\system.txt": "System events\n",
               "C:\\Windows\\LiveKernelReports\\WATCHDOG\\WD-1.dmp": "live dump",
               "C:\\Windows\\MEMORY.DMP": "kernel dump"}
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
        behaviour.setdefault("outputs", {"check": "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                                         "rvktest": PASSING_REPORT, "collect": COLLECTED})
        behaviour.setdefault("files", GUEST_FILES)
        (self.scratch / "script.json").write_text(json.dumps(behaviour))

    def run_suite(self, *extra, environment=None, stdin=""):
        arguments = ["--image", self.image, "--work", self.work, "--qemu", FAKES / "fake_qemu.py",
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
        self.assertIn("virtio-gpu-rvk-pci,host-gpu=auto,executor=/nowhere/librvk_echo_executor.so", argv)
        self.assertIn("isa-debugcon,iobase=0x402,chardev=debugcon", argv)
        self.assertIn("name=org.qemu.guest_agent.0", argv)
        self.assertIn(",hv-crash", argv)
        self.assertIn("-action panic=none", argv)
        self.assertIn("-msg timestamp=on", argv)
        self.assertNotIn("-gdb", argv)
        events = [line.split(" ")[0] if line.startswith(("exec", "file", "pull")) else line for line in lines[1:]]
        self.assertEqual(self.stage_calls(), ["sign", "install", "check", "collect"])
        self.assertEqual(events.count("file"), 4)
        self.assertEqual(events.count("pull"), 2)
        self.assertIn("file C:\\rvk\\package\\rvkgpu.sys 1000", lines)
        self.assertIn("file C:\\rvk\\package\\rvkgpu.inf", "\n".join(lines))
        order = [line for line in lines
                 if line in ("reboot", "powerdown") or (line.startswith("exec") and ("-Stage" in line or "rvktest" in line))]
        self.assertEqual([o if o in ("reboot", "powerdown") else o.split()[-1] for o in order],
                         ["sign", "reboot", "install", "reboot", "check", "C:\\rvk\\rvktest.exe", "collect",
                          "powerdown"])
        self.assertEqual((self.work / "report.txt").read_text(), PASSING_REPORT)
        self.assertIn("service=rvkgpu", (self.work / "device.txt").read_text())
        self.assertEqual((self.work / "rvkgpu.log").read_text(), "rvkgpu: DriverEntry\n")
        self.assertEqual((self.work / "diag" / "system.txt").read_text(), "System events\n")
        self.assertEqual((self.work / "diag" / "dumps" / "WD-1.dmp").read_text(), "live dump")
        self.assertEqual(sorted(p.name for p in (self.work / "symbols").iterdir()), ["rvkgpu.pdb", "rvkgpu.sys"])
        self.assertIn("guest: report ok: 23 passed", result.stdout)
        self.assertIn("guest: collect: 2 files in", result.stdout)
        self.assertFalse((self.work / "panic.txt").exists())
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
                    outputs={"rvktest": PASSING_REPORT, "collect": "file C:\\rvk\\diag\\absent.txt\n"})
        result = self.run_suite()
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("guest: collect.pull failed: guest agent error: no file C:\\rvk\\diag\\absent.txt", result.stdout)
        self.assert_clean()

    def test_a_report_with_failures_or_without_its_summary_fails_the_run(self):
        self.script(rvktest=2, outputs={"check": "", "rvktest": "FAIL submission.wait\nrvktest: 21 passed, 2 failed\n"})
        result = self.run_suite()
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: report failed: 2 step(s) failed, exit 2", result.stdout)
        self.assert_clean()
        self.script(outputs={"check": "", "rvktest": "ok adapter.find\n"})
        result = self.run_suite()
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: report failed: no summary line (exit 0)", result.stdout)
        self.assert_clean()

    def test_a_bugcheck_is_named_and_its_kernel_dump_pulled_after_the_guest_reboots(self):
        self.script(panic={"during": "rvktest", "code": 0xD1},
                    outputs={"check": "", "rvktest": PASSING_REPORT, "collect": WITH_MEMORY_DUMP})
        result = self.run_suite("--memory-dump")
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: rvktest failed: timed out waiting for an answer", result.stdout)
        named = "bugcheck 0xd1 DRIVER_IRQL_NOT_LESS_OR_EQUAL (0x10, 0x2, 0x0, 0xfffff80)"
        self.assertIn(f"guest: panic: {named}", result.stdout)
        self.assertEqual((self.work / "panic.txt").read_text(), named + "\n")
        self.assertIn("guest: panic.reset ok", result.stdout)
        self.assertEqual(self.stage_calls()[-1], "collect -MemoryDump")
        self.assertEqual((self.work / "diag" / "dumps" / "MEMORY.DMP").read_text(), "kernel dump")
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
        self.assertEqual((self.work / "diag" / "dumps" / "MEMORY.DMP").read_text(), "kernel dump")
        self.assertEqual(self.journal_lines()[-1], "powerdown")
        self.assert_clean()

    def test_the_kernel_dump_is_left_alone_unless_it_is_asked_for(self):
        """MEMORY.DMP is copied through the agent in base64 chunks, so on a real
        guest it outlasts the step timeout and the failing pull takes the rest of
        the manifest with it. Four bring-up runs of feature 037 lost their
        minidumps that way; the kernel dump is now opt-in."""
        self.script(panic={"during": "rvktest", "code": 0xD1},
                    outputs={"check": "", "rvktest": PASSING_REPORT, "collect": COLLECTED})
        result = self.run_suite()
        self.assertEqual(result.returncode, 1)
        self.assertEqual(self.stage_calls()[-1], "collect")
        self.assertFalse((self.work / "diag" / "dumps" / "MEMORY.DMP").exists())
        self.assertEqual((self.work / "diag" / "dumps" / "WD-1.dmp").read_text(), "live dump")
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

    def test_without_an_image_it_says_how_to_name_one(self):
        environment = {key: value for key, value in self.environment.items() if key != "AV_GUEST_IMAGE"}
        result = subprocess.run([sys.executable, "-B", str(RUNNER), "--work", str(self.work)], capture_output=True,
                                text=True, env=environment)
        self.assertEqual(result.returncode, 2)
        self.assertIn("no image: pass --image or set AV_GUEST_IMAGE", result.stderr)


if __name__ == "__main__":
    unittest.main()
