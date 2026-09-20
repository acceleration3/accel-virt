"""rvkgpu.inf against what the build and the device actually are: the image it
installs, the PCI identity QEMU gives virtio-gpu-rvk-pci, and the interrupt
properties the driver's message-signalled path needs."""
import configparser
import json
import re
import unittest

from tool_run import ROOT

INF = ROOT / "src" / "km" / "rvkgpu.inf"
DEVICE = ROOT / "src" / "device" / "qemu" / "virtio-gpu-pci-rvk.c"
DRIVER_BUILD = ROOT / "src" / "km" / "CMakeLists.txt"
MANIFEST = ROOT / "src" / "um" / "vulkan" / "rvkvk.json"
PACKAGE_FILES = ("rvkgpu.sys", "rvkvk.dll", "rvkvk.json", "rvkgl.dll", "rvkcl.dll", "rvkumd.dll")
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
                                      if not line.startswith("HKR") and line.strip() not in PACKAGE_FILES))

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

    def test_declares_a_user_mode_driver_name_in_the_software_key(self):
        """dxgkrnl refuses the adapter without it. DXGADAPTER::Initialize
        (dxgkrnl.sys @1401a2818) returns STATUS_OBJECT_NAME_NOT_FOUND when
        UmdNameMissing is set and MiscCaps & 0xC is clear; DpiGetAdapterInfo
        (@14024eb50) sets UmdNameMissing when DxgkRetrieveStringFromRegistry
        (@140250b00) cannot read UserModeDriverName from the software key -
        DpiOpenPnpRegistryKey(dev, 2, ...), which is HKR under the install
        section, not under .HW. Feature 036's live run hit exactly this."""
        software = self.inf["Rvkgpu_Install"]["AddReg"]
        self.assertRegex(
            self.text,
            rf"\[{re.escape(software)}\][\s\S]*?HKR,+\s*UserModeDriverName,\s*0x00010000,\s*\"?rvkumd\.dll",
            "rvkgpu.inf must add UserModeDriverName (REG_MULTI_SZ) under the install section's AddReg")

    def test_registers_the_vulkan_icd_through_the_adapter_and_nowhere_else(self):
        """Feature 045, contracts/loader_interface.md: the Windows Vulkan loader
        reads VulkanDriverName from each adapter's software key through
        D3DKMTQueryAdapterInfo(KMTQAITYPE_QUERYREGISTRY, ADAPTERKEY), as
        REG_MULTI_SZ or REG_SZ - never REG_EXPAND_SZ - and takes it as an
        absolute manifest path; a global Khronos key would list the device
        twice."""
        software = self.inf["Rvkgpu_Install"]["AddReg"]
        section = re.search(rf"\[{re.escape(software)}\]([\s\S]*?)(?:\n\[|\Z)", self.text).group(1)
        self.assertRegex(section, r'HKR,+\s*VulkanDriverName,\s*0x00010000,\s*"%13%\\rvkvk\.json"')
        self.assertNotIn("Khronos", self.text)
        self.assertNotIn("HKLM", self.text)
        for image in ("rvkvk.dll", "rvkvk.json"):
            self.assertIn(f"{image} = 1", self.text)
            files = re.search(r"\[Rvkgpu_Files\]([\s\S]*?)(?:\n\[|\Z)", self.text).group(1)
            self.assertIn(image, files.split())

    def test_registers_the_opengl_icd_through_the_adapter_and_nowhere_else(self):
        """Feature 050, contracts/icd_interface.md: dxgkrnl's DpiGetAdapterInfo
        reads OpenGLDriverName as a string and OpenGLVersion and OpenGLFlags as
        type-checked REG_DWORDs from the software key; opengl32 asks for them
        with KMTQAITYPE_UMOPENGLINFO. Flags must be 1, or opengl32 describes
        pixel formats itself and never asks the ICD. The WOW values wait for
        a 32-bit ICD (061), and the global OpenGLDrivers key is the pre-WDDM
        path FR-001 forbids."""
        software = self.inf["Rvkgpu_Install"]["AddReg"]
        section = re.search(rf"\[{re.escape(software)}\]([\s\S]*?)(?:\n\[|\Z)", self.text).group(1)
        values = {match.group(1): (match.group(2), match.group(3))
                  for match in re.finditer(r"^HKR,,\s*(OpenGL\w*),\s*(0x[0-9A-Fa-f]{8}),\s*(.+?)\s*$",
                                           section, re.M)}
        self.assertEqual(values, {"OpenGLDriverName": ("0x00010000", '"%13%\\rvkgl.dll"'),
                                  "OpenGLVersion": ("0x00010001", "2"),
                                  "OpenGLFlags": ("0x00010001", "1")})
        self.assertNotIn("OpenGLDrivers", self.text)
        # No ...Wow value is written: the 32-bit arm is feature 061's. Comments
        # may say so, so this reads the directives rather than the whole file.
        directives = "\n".join(line for line in self.text.splitlines() if not line.lstrip().startswith(";"))
        self.assertNotIn("Wow", directives)
        self.assertIn("rvkgl.dll = 1", self.text)
        files = re.search(r"\[Rvkgpu_Files\]([\s\S]*?)(?:\n\[|\Z)", self.text).group(1)
        self.assertIn("rvkgl.dll", files.split())

    def test_registers_the_opencl_icd_as_a_single_string_through_the_adapter(self):
        """Feature 052, contracts/icd_interface.md: the loader's first discovery
        path (khrIcdOsVendorsEnumerateDXGK) asks D3DKMTQueryAdapterInfo for
        OpenCLDriverName with value_type = REG_SZ and returns nothing for a
        value of another type; only the slower SetupAPI path tolerates a
        multi-string. So this value is 0x00000000 where VulkanDriverName and
        OpenGLDriverName are 0x00010000 - written multi-string the ICD would be
        found on one path and silently missed on the other. OpenCLDriverNameWow
        waits for a 32-bit ICD (061), and the global Khronos vendors key is the
        path FR-001 forbids."""
        software = self.inf["Rvkgpu_Install"]["AddReg"]
        section = re.search(rf"\[{re.escape(software)}\]([\s\S]*?)(?:\n\[|\Z)", self.text).group(1)
        values = {match.group(1): (match.group(2), match.group(3))
                  for match in re.finditer(r"^HKR,,\s*(OpenCL\w*),\s*(0x[0-9A-Fa-f]{8}),\s*(.+?)\s*$",
                                           section, re.M)}
        self.assertEqual(values, {"OpenCLDriverName": ("0x00000000", '"%13%\\rvkcl.dll"')})
        self.assertNotEqual(values["OpenCLDriverName"][0], "0x00010000")
        self.assertNotIn("OpenCLDriverNameWow", self.text)
        self.assertNotIn("Khronos", self.text)
        self.assertIn("rvkcl.dll = 1", self.text)
        files = re.search(r"\[Rvkgpu_Files\]([\s\S]*?)(?:\n\[|\Z)", self.text).group(1)
        self.assertIn("rvkcl.dll", files.split())

    def test_the_manifest_names_the_icd_beside_it(self):
        """A relative library_path is joined to the manifest's directory only
        when it holds a backslash (Vulkan-Loader loader.c:2460)."""
        manifest = json.loads(MANIFEST.read_text())
        self.assertEqual(manifest["file_format_version"], "1.0.1")
        self.assertEqual(manifest["ICD"]["library_path"], ".\\rvkvk.dll")
        self.assertEqual(manifest["ICD"]["library_arch"], "64")
        self.assertEqual(manifest["ICD"]["api_version"], "1.3.0")

    def test_asks_for_message_signalled_interrupts(self):
        self.assertIn('"Interrupt Management\\MessageSignaledInterruptProperties", MSISupported, 0x00010001, 1',
                      self.text)
        self.assertIn('MessageNumberLimit, 0x00010001, 11', self.text)
        self.assertEqual(self.inf["Rvkgpu_Install.HW"]["AddReg"], "Rvkgpu_Interrupts")


if __name__ == "__main__":
    unittest.main()


class Provisioning(unittest.TestCase):
    """provision.ps1 is run only in a guest; what it shares with the package and
    the runner is held still here."""

    def test_checks_the_hardware_id_the_package_installs_on(self):
        script = (ROOT / "tools" / "guest" / "provision.ps1").read_text()
        hardware = re.search(r"\$HardwareId = '([^']+)'", script).group(1)
        self.assertIn(f"= Rvkgpu_Install, {hardware}", INF.read_text())
        self.assertIn("service -ne 'rvkgpu'", script)

    def test_offers_exactly_the_stages_the_runner_calls(self):
        script = (ROOT / "tools" / "guest" / "provision.ps1").read_text()
        offered = re.search(r"ValidateSet\(([^)]*)\)", script, re.S).group(1)
        runner = (ROOT / "tools" / "guest" / "run_guest_suite.py").read_text()
        called = set(re.findall(r'self\.(?:stage|stage_output|provision|provision_ok)\("(\w+)"', runner))
        self.assertEqual(sorted(called), sorted(name.strip(" '\n") for name in offered.split(",")))
        self.assertIn("-CatalogFilePath \"$Package\\rvkgpu.cat\"", script)
        self.assertIn('PACKAGE_DIRECTORY = GUEST_DIRECTORY + "\\\\package"', runner)

    def test_the_runner_stages_every_file_the_package_names_where_the_catalog_covers_it(self):
        """pnputil /add-driver refuses a package whose INF names a file the
        directory does not hold ("The system cannot find the file specified"),
        and the sign stage catalogs the whole directory, so a file staged
        anywhere else would install uncovered. Every run installs the package,
        so this holds for every mode, not only --vulkan (feature 045)."""
        text = INF.read_text()
        section = re.search(r"\[SourceDisksFiles\]([\s\S]*?)(?:\n\[|\Z)", text).group(1)
        named = {line.split("=")[0].strip() for line in section.splitlines()
                 if "=" in line and not line.lstrip().startswith(";")}
        runner = (ROOT / "tools" / "guest" / "run_guest_suite.py").read_text()
        staged = set(re.findall(r'PACKAGE_DIRECTORY \+ "\\\\([\w.]+)"', runner))
        self.assertEqual(named, {"rvkgpu.sys", "rvkvk.dll", "rvkvk.json", "rvkgl.dll", "rvkcl.dll",
                                 "rvkumd.dll"})
        self.assertEqual(staged, named | {"rvkgpu.inf"})
        script = (ROOT / "tools" / "guest" / "provision.ps1").read_text()
        self.assertIn("New-FileCatalog -Path $Package ", script)

    def test_collects_the_icd_log_with_the_guest_s_diagnostics(self):
        """FR-007: whatever the run did, the ICD's own log (src/um/vulkan's
        FileLog, one file per process) is among the files the runner pulls."""
        script = (ROOT / "tools" / "guest" / "provision.ps1").read_text()
        collect = script[script.index("'collect' {"):]
        copied = collect.index("Copy-Item 'C:\\ProgramData\\rvk\\*.log' $Diagnostics")
        self.assertLess(copied, collect.index("Get-ChildItem $Diagnostics -File"))


class FeatureStaging(unittest.TestCase):
    """Feature 049, research.md §19: dxgkrnl leaves a staged feature off unless
    something turns it on, and what it reads is Features\\<id>\\Enabled under the
    adapter's own key. The driver implements user-mode submission (4) and native
    fences (37), so its INF enables exactly those two and no others."""

    def setUp(self):
        self.text = INF.read_text(encoding="utf-8-sig")

    def test_the_two_features_this_driver_implements_are_enabled(self):
        for feature in (4, 37):
            self.assertRegex(self.text, rf"HKR,\s*Features\\{feature},\s*Enabled,\s*0x00010001,\s*1",
                             f"rvkgpu.inf must enable DXGK feature {feature}")
            self.assertRegex(self.text, rf"HKR,\s*Features\\{feature},\s*AllowExperimental,\s*0x00010001,\s*1",
                             f"rvkgpu.inf must allow feature {feature} while the OS stages it")

    def test_no_other_feature_is_turned_on(self):
        enabled = set(re.findall(r"HKR,\s*Features\\(\d+),\s*Enabled", self.text))
        self.assertEqual(enabled, {"4", "37"},
                         "only the features this driver implements may be enabled (constitution VI)")
