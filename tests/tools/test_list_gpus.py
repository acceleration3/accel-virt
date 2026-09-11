import unittest

from source_tree import SourceTree
from tool_run import ROOT, run

TOOL = ROOT / "tools" / "host" / "list_gpus.py"

# The shape of full `vulkaninfo` output, trimmed to what the tool reads: two GPUs,
# the second with a device-local heap and a host heap, and llvmpipe.
CAPTURE = """==========
VULKANINFO
==========

Device Properties and Extensions:
=================================
GPU0:
VkPhysicalDeviceProperties:
---------------------------
\tapiVersion        = 1.4.354 (4211042)
\tvendorID          = 0x8086
\tdeviceID          = 0x46a6
\tdeviceType        = PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
\tdeviceName        = Intel(R) Iris(R) Xe Graphics (ADL GT2)
VkPhysicalDeviceIDProperties:
\tdeviceUUID                        = 8680a646-0c00-0000-0002-000000000000
\tdriverUUID                        = 1d68214a-188b-15c9-9c73-95592fb07850
VkPhysicalDeviceMemoryProperties:
=================================
memoryHeaps: count = 1
\tmemoryHeaps[0]:
\t\tsize   = 12306785280 (0x2dd8aa400) (11.46 GiB)
\t\tflags: count = 1
\t\t\tMEMORY_HEAP_DEVICE_LOCAL_BIT
memoryTypes: count = 1
\tmemoryTypes[0]:
\t\tpropertyFlags = 0x0001: count = 1
\t\t\tMEMORY_PROPERTY_DEVICE_LOCAL_BIT
GPU1:
VkPhysicalDeviceProperties:
\tvendorID          = 0x10de
\tdeviceID          = 0x24e0
\tdeviceType        = PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
\tdeviceName        = NVIDIA GeForce RTX 3070 Ti Laptop GPU
\tdeviceUUID                        = 7b7ad2c5-4219-8c80-c4fd-a64b8775aedb
memoryHeaps: count = 2
\tmemoryHeaps[0]:
\t\tsize   = 8589934592 (0x200000000) (8.00 GiB)
\t\tflags: count = 1
\t\t\tMEMORY_HEAP_DEVICE_LOCAL_BIT
\tmemoryHeaps[1]:
\t\tsize   = 12306785280 (0x2dd8aa400) (11.46 GiB)
\t\tflags: count = 0
\t\t\tNone
GPU2:
\tvendorID          = 0x10005
\tdeviceID          = 0x0000
\tdeviceType        = PHYSICAL_DEVICE_TYPE_CPU
\tdeviceName        = llvmpipe (LLVM 21.1.0, 256 bits)
\tdeviceUUID        = 6d657361-3231-2e31-2e30-000000000000
memoryHeaps: count = 1
\tmemoryHeaps[0]:
\t\tsize   = 2147483648
\t\tflags: count = 1
\t\t\tMEMORY_HEAP_DEVICE_LOCAL_BIT
"""


class ListGpus(unittest.TestCase):
    def setUp(self):
        self.tree = SourceTree()
        self.addCleanup(self.tree.remove)

    def list(self, text, *flags):
        self.tree.write("vulkaninfo.txt", text)
        return run(TOOL, "--vulkaninfo", self.tree.root / "vulkaninfo.txt", *flags)

    def test_the_table_names_each_device_by_uuid_with_its_local_memory(self):
        result = self.list(CAPTURE)
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = result.stdout.splitlines()
        self.assertEqual(len(lines), 4)
        self.assertIn("8680a646-0c00-0000-0002-000000000000  8086:46a6  integrated", lines[1])
        self.assertIn("11.5 GiB", lines[1])
        self.assertIn("7b7ad2c5-4219-8c80-c4fd-a64b8775aedb  10de:24e0  discrete", lines[2])
        self.assertIn("8.0 GiB  NVIDIA GeForce RTX 3070 Ti Laptop GPU", lines[2])
        self.assertIn("cpu", lines[3])
        self.assertIn("llvmpipe", lines[3])

    def test_qemu_gets_one_device_per_gpu_bound_by_uuid(self):
        result = self.list(CAPTURE, "--qemu")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.splitlines(), [
            "-device virtio-gpu-rvk-pci,host-gpu=8680a646-0c00-0000-0002-000000000000,max_outputs=1",
            "-device virtio-gpu-rvk-pci,host-gpu=7b7ad2c5-4219-8c80-c4fd-a64b8775aedb,max_outputs=1",
            "-device virtio-gpu-rvk-pci,host-gpu=6d657361-3231-2e31-2e30-000000000000,max_outputs=1",
        ])

    def test_libvirt_passes_the_same_arguments_with_shared_memfd_memory(self):
        result = self.list(CAPTURE, "--libvirt")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("<source type='memfd'/><access mode='shared'/>", result.stdout)
        self.assertIn("<model type='none'/>", result.stdout)
        self.assertEqual(result.stdout.count("<qemu:arg value='-device'/>"), 3)
        self.assertIn('<qemu:arg value="virtio-gpu-rvk-pci,host-gpu=7b7ad2c5-4219-8c80-c4fd-a64b8775aedb,'
                      'max_outputs=1"/>', result.stdout)

    def test_output_without_a_device_is_an_error(self):
        result = self.list("==========\nVULKANINFO\n==========\n")
        self.assertEqual(result.returncode, 1)
        self.assertIn("no Vulkan physical device found", result.stderr)

    def test_a_device_missing_a_field_or_with_a_malformed_uuid_is_named(self):
        missing = self.list("GPU3:\n\tdeviceName = x\n\tvendorID = 0x1\n\tdeviceID = 0x2\n")
        self.assertEqual(missing.returncode, 1)
        self.assertIn("GPU3: no deviceType, deviceUUID", missing.stderr)
        malformed = self.list("GPU4:\n\tdeviceName = x\n\tvendorID = 0x1\n\tdeviceID = 0x2\n"
                              "\tdeviceType = PHYSICAL_DEVICE_TYPE_OTHER\n\tdeviceUUID = not-a-uuid\n")
        self.assertIn("GPU4: malformed deviceUUID not-a-uuid", malformed.stderr)

    def test_an_unreadable_capture_or_conflicting_outputs_are_refused(self):
        result = run(TOOL, "--vulkaninfo", self.tree.root / "absent.txt")
        self.assertEqual(result.returncode, 1)
        self.assertIn("cannot read", result.stderr)
        self.assertNotEqual(self.list(CAPTURE, "--qemu", "--libvirt").returncode, 0)


if __name__ == "__main__":
    unittest.main()
