"""tools/host/probe_host_memory.py against a fake QEMU: one device per host GPU,
the probe verdicts read back out of what realize said, and a refused probe that
is an answer rather than a failure."""
import json
import pathlib
import shutil
import stat
import tempfile
import unittest

from tool_run import ROOT, run

TOOL = ROOT / "tools" / "host" / "probe_host_memory.py"

INTEL = "8680a646-0c00-0000-0002-000000000000"
NVIDIA = "7b7ad2c5-4219-8c80-c4fd-a64b8775aedb"
CAPTURE = f"""Device Properties and Extensions:
GPU0:
VkPhysicalDeviceProperties:
\tvendorID          = 0x8086
\tdeviceID          = 0x46a6
\tdeviceType        = PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
\tdeviceName        = Intel(R) Iris(R) Xe Graphics (ADL GT2)
VkPhysicalDeviceIDProperties:
\tdeviceUUID                        = {INTEL}
VkPhysicalDeviceMemoryProperties:
memoryHeaps: count = 1
\tmemoryHeaps[0]:
\t\tsize   = 12306771968
\t\tflags: count = 1
\t\t\tMEMORY_HEAP_DEVICE_LOCAL_BIT
GPU1:
VkPhysicalDeviceProperties:
\tvendorID          = 0x10de
\tdeviceID          = 0x24e0
\tdeviceType        = PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
\tdeviceName        = NVIDIA GeForce RTX 3070 Ti Laptop GPU
VkPhysicalDeviceIDProperties:
\tdeviceUUID                        = {NVIDIA}
VkPhysicalDeviceMemoryProperties:
memoryHeaps: count = 1
\tmemoryHeaps[0]:
\t\tsize   = 8589934592
\t\tflags: count = 1
\t\t\tMEMORY_HEAP_DEVICE_LOCAL_BIT
"""

FAKE_QEMU = """#!/usr/bin/env python3
import sys
uuid = [a for a in sys.argv if "host-gpu=" in a][0].split("host-gpu=")[1].split(",")[0]
verdicts = {verdicts}
for line in verdicts[uuid]:
    print(f"qemu-system-x86_64: -device virtio-gpu-av-pci: warning: virtio-gpu-av: {{line}}", file=sys.stderr)
sys.exit(0)
"""

class ProbeHostMemory(unittest.TestCase):
    def setUp(self):
        self.scratch = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)
        self.build = self.scratch / "qemu"
        self.build.mkdir()

    def qemu(self, verdicts):
        binary = self.build / "qemu-system-x86_64"
        binary.write_text(FAKE_QEMU.format(verdicts=repr(verdicts)))
        binary.chmod(binary.stat().st_mode | stat.S_IEXEC)
        return binary

    def probe(self, verdicts, *extra):
        self.qemu(verdicts)
        capture = self.scratch / "vulkaninfo.txt"
        capture.write_text(CAPTURE)
        return run(TOOL, "--qemu", self.build, "--vulkaninfo", capture, *extra)

    def test_reports_one_answer_per_host_gpu(self):
        result = self.probe({INTEL: ["probe exported memory: passed, 1048576",
                                     "probe guest import: passed, 1048576"],
                             NVIDIA: ["probe exported memory: passed, 1048576",
                                      "probe guest import: failed, status 6"]})
        self.assertEqual(result.returncode, 0, result.stderr)
        answers = json.loads(result.stdout)
        self.assertEqual([answer["uuid"] for answer in answers], [INTEL, NVIDIA])
        self.assertEqual(answers[0]["exported_memory"], {"outcome": "passed", "measured": 1048576})
        self.assertEqual(answers[1]["guest_import"], {"outcome": "failed", "status": 6})
        self.assertEqual(answers[0]["name"], "Intel(R) Iris(R) Xe Graphics (ADL GT2)")

    def test_a_refused_probe_is_an_answer_not_a_failure(self):
        result = self.probe({INTEL: ["probe exported memory: failed, status 6, host -22",
                                     "probe guest import: failed, status 6"],
                             NVIDIA: ["probe exported memory: failed, status 6, host -22",
                                      "probe guest import: failed, status 6"]})
        self.assertEqual(result.returncode, 0, result.stderr)
        answers = json.loads(result.stdout)
        self.assertEqual(answers[0]["exported_memory"], {"outcome": "failed", "status": 6, "host_error": -22})

    def test_a_device_that_says_nothing_about_a_probe_fails_the_run(self):
        result = self.probe({INTEL: ["probe exported memory: passed, 1048576"],
                             NVIDIA: []})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("said nothing about", result.stderr)

    def test_the_guest_check_is_skipped_with_its_reason(self):
        result = self.probe({INTEL: ["probe exported memory: passed, 1048576",
                                     "probe guest import: passed, 1048576"],
                             NVIDIA: ["probe exported memory: passed, 1048576",
                                      "probe guest import: passed, 1048576"]},
                            "--guest-check", "--kernel", str(self.scratch / "no-such-kernel"))
        self.assertEqual(result.returncode, 0, result.stderr)
        answers = json.loads(result.stdout)
        self.assertEqual(answers[0]["guest_check"]["outcome"], "skipped")
        self.assertIn("no-such-kernel", answers[0]["guest_check"]["reason"])


if __name__ == "__main__":
    unittest.main()
