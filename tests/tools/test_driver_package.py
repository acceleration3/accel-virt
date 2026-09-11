"""rvkgpu.inf against what the build and the device actually are: the image it
installs, the PCI identity QEMU gives virtio-gpu-rvk-pci, and the interrupt
properties the driver's message-signalled path needs."""
import configparser
import re
import unittest

from tool_run import ROOT

INF = ROOT / "src" / "km" / "rvkgpu.inf"
DEVICE = ROOT / "src" / "device" / "qemu" / "virtio-gpu-pci-rvk.c"
DRIVER_BUILD = ROOT / "src" / "km" / "CMakeLists.txt"
REDHAT_QUMRANET = 0x1AF4
VIRTIO_GPU = 0x1050


class DriverPackage(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        text = INF.read_text()
        cls.text = text
        cls.inf = configparser.RawConfigParser(strict=False, comment_prefixes=(";",), delimiters=("=",))
        cls.inf.optionxform = str
        cls.inf.read_string("\n".join(line for line in text.splitlines()
                                      if not line.startswith("HKR") and line.strip() != "rvkgpu.sys"))

    def test_is_a_display_class_package_with_a_catalog(self):
        version = self.inf["Version"]
        self.assertEqual(version["Signature"], '"$Windows NT$"')
        self.assertEqual(version["Class"], "Display")
        self.assertEqual(version["ClassGuid"].lower(), "{4d36e968-e325-11ce-bfc1-08002be10318}")
        self.assertEqual(version["CatalogFile"], "rvkgpu.cat")

    def test_matches_the_hardware_id_qemu_gives_the_device(self):
        subsystem = int(re.search(r"#define VIRTIO_GPU_RVK_PCI_SUBSYSTEM_ID (0x[0-9A-Fa-f]+)",
                                  DEVICE.read_text()).group(1), 16)
        self.assertIn("subsystem_vendor_id = PCI_SUBVENDOR_ID_REDHAT_QUMRANET", DEVICE.read_text())
        want = f"PCI\\VEN_{REDHAT_QUMRANET:04X}&DEV_{VIRTIO_GPU:04X}&SUBSYS_{subsystem:04X}{REDHAT_QUMRANET:04X}"
        models = self.inf["Rvkgpu.NTamd64.10.0...26100"]
        self.assertEqual([entry.split(",")[1].strip() for entry in models.values()], [want])
        self.assertEqual(self.inf["Manufacturer"]["%Provider%"], "Rvkgpu, NTamd64.10.0...26100")

    def test_installs_the_image_the_driver_build_produces_as_a_kernel_service(self):
        self.assertRegex(DRIVER_BUILD.read_text(), r"add_executable\(rvkgpu .*\)[\s\S]*SUFFIX \.sys")
        self.assertIn("rvkgpu.sys = 1", self.text)
        service = self.inf["Rvkgpu_Service"]
        self.assertEqual(service["ServiceBinary"], r"%13%\rvkgpu.sys")
        self.assertEqual(service["ServiceType"], "1")
        self.assertEqual(self.inf["Rvkgpu_Install.Services"]["AddService"], "rvkgpu, 0x00000002, Rvkgpu_Service")

    def test_asks_for_message_signalled_interrupts(self):
        self.assertIn('"Interrupt Management\\MessageSignaledInterruptProperties", MSISupported, 0x00010001, 1',
                      self.text)
        self.assertIn('MessageNumberLimit, 0x00010001, 3', self.text)
        self.assertEqual(self.inf["Rvkgpu_Install.HW"]["AddReg"], "Rvkgpu_Interrupts")


if __name__ == "__main__":
    unittest.main()
