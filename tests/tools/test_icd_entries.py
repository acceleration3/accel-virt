"""The generated Vulkan ICD wrappers and their table, from a registry small
enough to read (tools/codegen/rvk_codegen/emit/icd_entries.py).

What the loader relies on: every row says which object its command dispatches
on, so vk_icdGetPhysicalDeviceProcAddr can answer only physical-device
commands; and every physical device an enumeration answers is the ICD's
wrapped handle, whether it came in an array or inside a device group."""
import pathlib
import re
import shutil
import sys
import tempfile
import unittest

from tool_run import ROOT

sys.path.insert(0, str(ROOT / "tools" / "codegen"))

from rvk_codegen.emit import icd_entries  # noqa: E402
from rvk_codegen.ir import Command, CType, Param, Registry, Struct, StructMember  # noqa: E402


def param(name, base, pointer_depth=0, is_const=False, direction="in", len_expr=None):
    return Param(name=name, type=CType(base=base, pointer_depth=pointer_depth, is_const=is_const),
                 direction=direction, len_expr=len_expr)


def command(name, cmd_id, params, returns="VkResult"):
    return Command(name=name, return_type=CType(base=returns), params=params, cmd_id=cmd_id)


ALLOCATOR = param("pAllocator", "VkAllocationCallbacks", 1, True)
GROUP_COUNT = param("pPhysicalDeviceGroupCount", "uint32_t", 1, direction="out")
GROUPS = param("pPhysicalDeviceGroupProperties", "VkPhysicalDeviceGroupProperties", 1, direction="out",
               len_expr="pPhysicalDeviceGroupCount")

COMMANDS = [
    command("vkCreateInstance", 1, [param("pCreateInfo", "VkInstanceCreateInfo", 1, True), ALLOCATOR,
                                    param("pInstance", "VkInstance", 1, direction="out")]),
    command("vkDestroyInstance", 2, [param("instance", "VkInstance"), ALLOCATOR], "void"),
    command("vkEnumeratePhysicalDevices", 3, [
        param("instance", "VkInstance"), param("pPhysicalDeviceCount", "uint32_t", 1, direction="out"),
        param("pPhysicalDevices", "VkPhysicalDevice", 1, direction="out", len_expr="pPhysicalDeviceCount")]),
    command("vkGetPhysicalDeviceFeatures", 4, [
        param("physicalDevice", "VkPhysicalDevice"),
        param("pFeatures", "VkPhysicalDeviceFeatures", 1, direction="out")], "void"),
    command("vkDeviceWaitIdle", 5, [param("device", "VkDevice")]),
    command("vkQueueWaitIdle", 6, [param("queue", "VkQueue")]),
    command("vkCmdDraw", 7, [param("commandBuffer", "VkCommandBuffer"), param("vertexCount", "uint32_t")],
            "void"),
    command("vkEnumeratePhysicalDeviceGroups", 8, [param("instance", "VkInstance"), GROUP_COUNT, GROUPS]),
    command("vkEnumeratePhysicalDeviceGroupsKHR", 9, [param("instance", "VkInstance"), GROUP_COUNT, GROUPS]),
]


class GeneratedIcdEntries(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.out = pathlib.Path(tempfile.mkdtemp())
        registry = Registry()
        for cmd in COMMANDS:
            registry.commands[cmd.name] = cmd
        registry.structs["VkPhysicalDeviceGroupProperties"] = Struct(
            name="VkPhysicalDeviceGroupProperties", returned_only=True, members=[
                StructMember(name="sType", type=CType(base="VkStructureType")),
                StructMember(name="pNext", type=CType(base="void", pointer_depth=1)),
                StructMember(name="physicalDeviceCount", type=CType(base="uint32_t")),
                StructMember(name="physicalDevices", len_expr="physicalDeviceCount",
                             type=CType(base="VkPhysicalDevice", fixed_dim=32,
                                        api_constant="VK_MAX_DEVICE_GROUP_SIZE")),
                StructMember(name="subsetAllocation", type=CType(base="VkBool32")),
            ])
        icd_entries.emit(registry, cls.out)
        cls.header = (cls.out / "vk_icd_entries.h").read_text()
        cls.source = (cls.out / "vk_icd_entries.cpp").read_text()

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.out)

    def row_level(self, name):
        rows = re.findall(r'Entry\{ "(\w+)", reinterpret_cast<PFN_vkVoidFunction>\(&Wrap_\w+\), '
                          r'DispatchLevel::(\w+) \},', self.source)
        levels = dict(rows)
        self.assertEqual(len(levels), len(COMMANDS), "every command has exactly one row")
        return levels[name]

    def wrapper(self, name):
        match = re.search(r"VKAPI_CALL Wrap_" + name + r"\(.*?\n\}\n", self.source, re.S)
        self.assertIsNotNone(match, name)
        return match.group(0)

    def test_the_header_declares_the_level_each_row_carries(self):
        self.assertIn("enum class DispatchLevel : uint8_t { global, instance, physical_device, device };",
                      self.header)
        self.assertRegex(self.header, r"struct Entry \{\s*const char\* name;\s*PFN_vkVoidFunction fn;\s*"
                                      r"DispatchLevel level;\s*\};")

    def test_a_command_with_no_dispatchable_first_parameter_is_global(self):
        self.assertEqual(self.row_level("vkCreateInstance"), "global")

    def test_an_instance_command_is_instance_level(self):
        self.assertEqual(self.row_level("vkDestroyInstance"), "instance")
        self.assertEqual(self.row_level("vkEnumeratePhysicalDevices"), "instance")
        self.assertEqual(self.row_level("vkEnumeratePhysicalDeviceGroups"), "instance")

    def test_only_a_physical_device_first_parameter_is_physical_device_level(self):
        self.assertEqual(self.row_level("vkGetPhysicalDeviceFeatures"), "physical_device")
        levels = [self.row_level(cmd.name) for cmd in COMMANDS]
        self.assertEqual(levels.count("physical_device"), 1)

    def test_a_device_queue_or_command_buffer_command_is_device_level(self):
        self.assertEqual(self.row_level("vkDeviceWaitIdle"), "device")
        self.assertEqual(self.row_level("vkQueueWaitIdle"), "device")
        self.assertEqual(self.row_level("vkCmdDraw"), "device")

    def test_level_of_a_pointer_or_missing_first_parameter_is_global(self):
        self.assertEqual(icd_entries.dispatch_level(command("vkNoParameters", 90, [])), "global")
        by_pointer = command("vkByPointer", 91, [param("pDevice", "VkDevice", 1)])
        self.assertEqual(icd_entries.dispatch_level(by_pointer), "global")
        by_array = Command(name="vkByArray", return_type=CType(base="void"), cmd_id=92,
                           params=[Param(name="devices", type=CType(base="VkDevice", fixed_dim=4))])
        self.assertEqual(icd_entries.dispatch_level(by_array), "global")

    def test_a_physical_device_array_is_wrapped_element_by_element(self):
        body = self.wrapper("vkEnumeratePhysicalDevices")
        self.assertIn("pPhysicalDevices[k] = ::av::icd::wrap_server_handle<VkPhysicalDevice>(", body)

    def test_every_physical_device_inside_a_group_is_wrapped(self):
        for name in ("vkEnumeratePhysicalDeviceGroups", "vkEnumeratePhysicalDeviceGroupsKHR"):
            body = self.wrapper(name)
            self.assertIn("rvk_rc != VK_SUCCESS && rvk_rc != VK_INCOMPLETE", body)
            self.assertIn("if (pPhysicalDeviceGroupProperties == nullptr) {", body)
            self.assertIn("::av::icd::wrap_server_handle<VkPhysicalDevice>(reinterpret_cast<uint64_t>(", body)
            self.assertIn("return VK_ERROR_OUT_OF_HOST_MEMORY;", body)
            # The wrap follows the call: it reads the host's handles the call wrote.
            self.assertLess(body.index("rvk_" + name + "("), body.index("wrap_server_handle"))

    def test_each_groups_own_count_bounds_the_devices_wrapped(self):
        body = self.wrapper("vkEnumeratePhysicalDeviceGroups")
        self.assertIn("for (uint32_t k = 0; k < *pPhysicalDeviceGroupCount; ++k) {", body)
        self.assertIn("for (uint32_t j = 0; j < pPhysicalDeviceGroupProperties[k].physicalDeviceCount; ++j) {",
                      body)


if __name__ == "__main__":
    unittest.main()
