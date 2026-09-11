"""tools/guest/run_guest_suite.py against a fake QEMU with a scripted guest:
every step, reboot, timeout and misbehaving agent, and the promise that the
base image is never written and the overlay never outlives the run."""
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

from tool_run import ROOT

RUNNER = ROOT / "tools" / "guest" / "run_guest_suite.py"
FAKES = ROOT / "tests" / "tools" / "fake_guest"
PASSING_REPORT = "ok adapter.find\nrvktest: 23 passed, 0 failed\n"


class GuestSuiteRun(unittest.TestCase):
    def setUp(self):
        self.scratch = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)
        self.image = self.scratch / "windows.qcow2"
        self.image.write_bytes(b"prepared image")
        self.os_stat = os.stat(self.image)
        for name in ("rvkgpu.sys", "rvktest.exe", "OVMF_VARS.fd", "OVMF_CODE.fd"):
            (self.scratch / name).write_bytes(name.encode() * 100)
        self.work = self.scratch / "work"
        self.journal = self.scratch / "journal.txt"
        self.environment = dict(os.environ, FAKE_GUEST_JOURNAL=str(self.journal),
                                FAKE_GUEST_SCRIPT=str(self.scratch / "script.json"))

    def script(self, **behaviour):
        behaviour.setdefault("stages", {"sign": [3010], "install": [3010], "check": [0]})
        behaviour.setdefault("outputs", {"check": "device: PCI\\VEN_1AF4 status=OK problem=0 service=rvkgpu\n",
                                         "rvktest": PASSING_REPORT})
        (self.scratch / "script.json").write_text(json.dumps(behaviour))

    def run_suite(self, *extra, environment=None):
        arguments = ["--image", self.image, "--work", self.work, "--qemu", FAKES / "fake_qemu.py",
                     "--qemu-img", FAKES / "fake_qemu_img.py", "--executor", "/nowhere/librvk_echo_executor.so",
                     "--driver", self.scratch / "rvkgpu.sys", "--test", self.scratch / "rvktest.exe",
                     "--firmware", self.scratch / "OVMF_CODE.fd", "--firmware-vars", self.scratch / "OVMF_VARS.fd",
                     "--boot-timeout", "10", "--step-timeout", "5", "--agent-timeout", "2", *extra]
        return subprocess.run([sys.executable, "-B", str(RUNNER), *map(str, arguments)], capture_output=True,
                              text=True, env=environment or self.environment, timeout=120)

    def journal_lines(self):
        return self.journal.read_text().splitlines() if self.journal.exists() else []

    def assert_clean(self):
        self.assertFalse((self.work / "overlay.qcow2").exists())
        self.assertFalse((self.work / "vars.fd").exists())
        after = os.stat(self.image)
        self.assertEqual((after.st_size, after.st_mtime_ns), (self.os_stat.st_size, self.os_stat.st_mtime_ns))
        self.assertTrue((self.work / "verdict.txt").exists())

    def test_a_passing_guest_runs_every_step_through_both_reboots(self):
        self.script()
        result = self.run_suite()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        lines = self.journal_lines()
        argv = lines[0]
        self.assertIn("virtio-gpu-rvk-pci,host-gpu=auto,executor=/nowhere/librvk_echo_executor.so", argv)
        self.assertIn("isa-debugcon,iobase=0x402,chardev=debugcon", argv)
        self.assertIn("name=org.qemu.guest_agent.0", argv)
        events = [line.split(" ")[0] if line.startswith(("exec", "file")) else line for line in lines[1:]]
        stages = [line.rsplit(" ", 1)[1] for line in lines if "-Stage" in line]
        self.assertEqual(stages, ["sign", "install", "check"])
        self.assertEqual(events.count("file"), 4)
        self.assertIn("file C:\\rvk\\package\\rvkgpu.sys 1000", lines)
        self.assertIn("file C:\\rvk\\package\\rvkgpu.inf", "\n".join(lines))
        order = [line for line in lines
                 if line in ("reboot", "powerdown") or (line.startswith("exec") and ("-Stage" in line or "rvktest" in line))]
        self.assertEqual([o if o in ("reboot", "powerdown") else o.split()[-1] for o in order],
                         ["sign", "reboot", "install", "reboot", "check", "C:\\rvk\\rvktest.exe", "powerdown"])
        self.assertEqual((self.work / "report.txt").read_text(), PASSING_REPORT)
        self.assertIn("service=rvkgpu", (self.work / "device.txt").read_text())
        self.assertEqual((self.work / "rvkgpu.log").read_text(), "rvkgpu: DriverEntry\n")
        self.assertIn("guest: report ok: 23 passed", result.stdout)
        self.assert_clean()

    def test_a_stage_that_needs_no_reboot_is_not_rebooted_and_garbage_from_the_agent_is_skipped(self):
        self.script(stages={"sign": [0], "install": [0], "check": [0]}, garbage=True)
        result = self.run_suite()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("reboot", self.journal_lines())
        self.assert_clean()

    def test_a_failing_stage_names_itself_and_nothing_after_it_runs(self):
        self.script(stages={"sign": [0], "install": [1], "check": [0]}, outputs={"install": "pnputil: no match\n"})
        result = self.run_suite()
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: install failed: exited 1: pnputil: no match", result.stdout)
        self.assertFalse(any(line.startswith("exec C:\\rvk\\rvktest.exe") for line in self.journal_lines()))
        self.assertIn("install failed", (self.work / "verdict.txt").read_text())
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

    def test_qemu_that_dies_at_boot_or_a_guest_agent_that_never_answers_fails_the_boot(self):
        self.script(exit_at_boot=True)
        result = self.run_suite()
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: boot failed: QEMU exited 1 before QMP listened", result.stdout)
        self.assert_clean()
        self.script(agent_silent=True)
        result = self.run_suite("--boot-timeout", "3")
        self.assertEqual(result.returncode, 1)
        self.assertIn("guest: boot failed: the guest agent never answered", result.stdout)
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
