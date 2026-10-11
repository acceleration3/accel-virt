import sys
import unittest

from source_tree import SourceTree
from tool_run import ROOT, run

TOOL = ROOT / "tools" / "host" / "list_gpus.py"
sys.path.insert(0, str(ROOT / "tools" / "host"))
import list_gpus  # noqa: E402

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
VkPhysicalDevicePCIBusInfoPropertiesEXT:
----------------------------------------
\tpciDomain   = 0
\tpciBus      = 123
\tpciDevice   = 0
\tpciFunction = 0
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
            "-device virtio-gpu-av-pci,host-gpu=8680a646-0c00-0000-0002-000000000000,max_outputs=1",
            "-device virtio-gpu-av-pci,host-gpu=7b7ad2c5-4219-8c80-c4fd-a64b8775aedb,max_outputs=1",
            "-device virtio-gpu-av-pci,host-gpu=6d657361-3231-2e31-2e30-000000000000,max_outputs=1",
        ])

    def test_libvirt_passes_the_same_arguments_with_shared_memfd_memory(self):
        result = self.list(CAPTURE, "--libvirt")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("<source type='memfd'/><access mode='shared'/>", result.stdout)
        self.assertIn("<model type='none'/>", result.stdout)
        self.assertEqual(result.stdout.count("<qemu:arg value='-device'/>"), 3)
        self.assertIn('<qemu:arg value="virtio-gpu-av-pci,host-gpu=7b7ad2c5-4219-8c80-c4fd-a64b8775aedb,'
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


class WhatTheGuestIsTold(unittest.TestCase):
    """The name and PCI identity a guest sees for each GPU, which the device
    decides in C++ and the guest runner checks Windows against."""

    def setUp(self):
        self.tree = SourceTree()
        self.addCleanup(self.tree.remove)
        self.gpus = list_gpus.parse(CAPTURE)

    def test_the_bus_address_is_read_where_the_driver_reports_one(self):
        self.assertIsNone(self.gpus[0].pci_address)
        self.assertEqual(self.gpus[1].pci_address, "0000:7b:00.0")
        self.assertIsNone(self.gpus[2].pci_address)

    def test_a_gpu_sysfs_describes_presents_its_own_ids(self):
        for name, text in (("vendor", "0x10de\n"), ("device", "0x24e0\n"), ("revision", "0xa1\n")):
            self.tree.write(f"0000:7b:00.0/{name}", text)
        self.assertEqual(self.gpus[1].pci_identity(self.tree.root), (0x10DE, 0x24E0, 0xA1))

    def test_a_gpu_with_no_function_or_none_sysfs_describes_keeps_virtio_gpus(self):
        self.assertEqual(self.gpus[0].pci_identity(self.tree.root), (0x1AF4, 0x1050, 0x01))
        self.assertEqual(self.gpus[1].pci_identity(self.tree.root), (0x1AF4, 0x1050, 0x01))  # no files
        for name, text in (("vendor", "0x10de\n"), ("device", "0x24e0\n"), ("revision", "a1\n")):
            self.tree.write(f"0000:7b:00.0/{name}", text)
        self.assertEqual(self.gpus[1].pci_identity(self.tree.root), (0x1AF4, 0x1050, 0x01))  # no 0x
        self.tree.write("0000:7b:00.0/revision", "0x100\n")
        self.assertEqual(self.gpus[1].pci_identity(self.tree.root), (0x1AF4, 0x1050, 0x01))  # past a byte

    def test_the_adapter_names_list_each_gpu_by_the_ids_it_presents_and_leaves_the_cpu_out(self):
        """What provision.ps1 reads from C:\\av\\adapters.txt: the ids the guest will see, tab-separated."""
        for name, text in (("vendor", "0x10de\n"), ("device", "0x24e0\n"), ("revision", "0xa1\n")):
            self.tree.write(f"0000:7b:00.0/{name}", text)
        self.assertEqual(list_gpus.adapter_names(self.gpus, self.tree.root).splitlines(), [
            f"1af4\t1050\t{self.gpus[0].adapter_name()}",
            f"10de\t24e0\t{self.gpus[1].adapter_name()}",
        ])
        self.assertEqual(list_gpus.adapter_names([self.gpus[2]], self.tree.root), "")

    def test_the_adapter_name_drops_the_driver_suffix_as_the_device_does(self):
        """The cases tests/unit/device/gpu/test_adapter_name.cpp holds the C++ to."""
        cases = {"AMD Radeon RX 9070 XT (RADV GFX1201)": "AMD Radeon RX 9070 XT",
                 "llvmpipe (LLVM 19.1.7, 256 bits)": "llvmpipe",
                 "Intel(R) Arc(tm) A770 Graphics (DG2)": "Intel(R) Arc(tm) A770 Graphics",
                 "Mesa (nested (groups)) ": "Mesa",
                 "Device  (two spaces)": "Device",
                 "NVIDIA GeForce RTX 4090": "NVIDIA GeForce RTX 4090",
                 "Intel(R) Arc(tm)": "Intel(R) Arc(tm)",
                 "(RADV GFX1201)": "(RADV GFX1201)",
                 "   (only spaces before)": "   (only spaces before)",
                 "unbalanced)": "unbalanced)",
                 "unbalanced x)": "unbalanced x)",
                 "": ""}
        for vulkan, expected in cases.items():
            self.assertEqual(list_gpus.adapter_name(vulkan), expected, vulkan)
        self.assertEqual(self.gpus[0].adapter_name(), "Intel(R) Iris(R) Xe Graphics")
        self.assertEqual(list_gpus.adapter_name("x" * 60), "x" * 47)
        self.assertEqual(list_gpus.adapter_name("y" * 46 + "\u00e9" + "y" * 8), "y" * 46)
        self.assertEqual(list_gpus.adapter_name("w" * 47 + " (RADV)"), "w" * 47)

    def test_an_integrated_gpu_amds_table_leaves_out_is_named_as_its_vendor_names_it(self):
        """An AMD one by AMD's generic name, whatever the PCI ID database's bracket
        says; another vendor's by the database's name after the vendor, once."""
        self.tree.write("pci.ids", "1002  Advanced Micro Devices, Inc. [AMD/ATI]\n"
                                   "\t13c0  Granite Ridge [Radeon 610M]\n"
                                   "8086  Intel Corporation\n"
                                   "\t46a6  Alder Lake-P GT2 [Iris Xe Graphics]\n"
                                   "\t7d55  Meteor Lake-P [Intel Arc Graphics]\n"
                                   "\t7d67  Arrow Lake-S\n")
        databases = (self.tree.root / "missing.ids", self.tree.root / "pci.ids")
        cases = {(0x1002, 0x13C0): "AMD Radeon(TM) Graphics",
                 (0x8086, 0x7D55): "Intel(R) Arc Graphics",
                 (0x8086, 0x46A6): "Intel(R) Iris Xe Graphics",
                 (0x8086, 0x7D67): "Intel(R) Arrow Lake-S",
                 (0x8086, 0x9999): None,
                 (0x1AF4, 0x1050): None}
        for (vendor, device), expected in cases.items():
            self.assertEqual(list_gpus.integrated_name(vendor, device, databases), expected, f"{vendor:04x}:{device:04x}")


if __name__ == "__main__":
    unittest.main()
