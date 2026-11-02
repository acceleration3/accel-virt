import json
import pathlib
import tempfile
import textwrap
import unittest

from tool_run import ROOT, run

TOOL = ROOT / "tools" / "layer_requirements.py"
HEADER = ROOT / "third_party" / "khronos" / "include" / "vulkan" / "vulkan_core.h"

BESIDE_THE_MODE = textwrap.dedent("""
    struct DxvkDeviceExtensions {
      DxvkExt extRobustness2 = { VK_EXT_ROBUSTNESS_2_EXTENSION_NAME, DxvkExtMode::Required };
      DxvkExt extMemoryPriority = { VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME, DxvkExtMode::Optional };
      DxvkExt khrSwapchain = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, DxvkExtMode::Required };
    };
""")

INSIDE_A_TABLE = textwrap.dedent("""
    static const char * const required_device_extensions[] = {
        VK_KHR_MAINTENANCE_1_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
    };
    static const struct vkd3d_optional_extension_info optional_device_extensions[] = {
        VK_EXTENSION(EXT_CONDITIONAL_RENDERING, VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME),
    };
""")


class LayerRequirementsRead(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.layer = pathlib.Path(self.directory.name)
        self.addCleanup(self.directory.cleanup)

    def read(self, *extra):
        result = run(TOOL, self.layer, "--json", "--header", HEADER, *extra)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return json.loads(result.stdout)

    def test_a_name_beside_its_mode_is_required_or_optional(self):
        (self.layer / "dxvk_extensions.h").write_text(BESIDE_THE_MODE)
        answered = self.read()
        self.assertEqual(answered["required"], ["VK_EXT_robustness2", "VK_KHR_swapchain"])
        self.assertEqual(answered["optional"], ["VK_EXT_memory_priority"])
        self.assertEqual(answered["unclassified"], [])

    def test_a_name_inside_a_table_belongs_to_what_the_table_says(self):
        (self.layer / "device.c").write_text(INSIDE_A_TABLE)
        answered = self.read()
        self.assertEqual(answered["required"], ["VK_KHR_maintenance1", "VK_KHR_push_descriptor"])
        self.assertEqual(answered["optional"], ["VK_EXT_conditional_rendering"])
        self.assertEqual(answered["unclassified"], [])

    def test_both_layers_read_together_and_required_wins(self):
        (self.layer / "dxvk_extensions.h").write_text(BESIDE_THE_MODE)
        (self.layer / "device.c").write_text(INSIDE_A_TABLE)
        (self.layer / "both.c").write_text(textwrap.dedent("""
            static const char * const required_device_extensions[] = {
                VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
            };
        """))
        answered = self.read()
        self.assertIn("VK_EXT_memory_priority", answered["required"])
        self.assertNotIn("VK_EXT_memory_priority", answered["optional"])

    def test_a_name_in_neither_shape_is_reported_rather_than_dropped(self):
        (self.layer / "loose.c").write_text(textwrap.dedent("""
            static void enable(void) {
                const char *name = VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME;
            }
        """))
        answered = self.read()
        self.assertEqual(answered["required"], [])
        self.assertEqual(answered["optional"], [])
        self.assertEqual(answered["unclassified"], ["VK_EXT_depth_clip_enable"])

    def test_a_macro_the_registry_does_not_define_is_not_a_name(self):
        (self.layer / "invented.c").write_text(textwrap.dedent("""
            static const char * const required_device_extensions[] = {
                VK_ACME_MADE_UP_EXTENSION_NAME,
            };
        """))
        answered = self.read()
        self.assertEqual(answered["required"], [])
        self.assertEqual(answered["unclassified"], ["VK_ACME_MADE_UP_EXTENSION_NAME"])

    def test_only_source_files_are_read_and_subdirectories_are(self):
        nested = self.layer / "libs" / "vkd3d"
        nested.mkdir(parents=True)
        (nested / "device.c").write_text(INSIDE_A_TABLE)
        (self.layer / "README.md").write_text("VK_EXT_ROBUSTNESS_2_EXTENSION_NAME in prose, Required")
        answered = self.read()
        self.assertEqual(answered["required"], ["VK_KHR_maintenance1", "VK_KHR_push_descriptor"])
        self.assertNotIn("VK_EXT_robustness2", answered["required"])

    def test_a_directory_that_is_not_there_is_an_error_not_an_empty_list(self):
        result = run(TOOL, self.layer / "absent", "--json", "--header", HEADER)
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('"required": []', result.stdout)

    def test_the_plain_output_names_every_line_it_read(self):
        (self.layer / "dxvk_extensions.h").write_text(BESIDE_THE_MODE)
        result = run(TOOL, self.layer, "--header", HEADER)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("required VK_EXT_robustness2", result.stdout)
        self.assertIn("optional VK_EXT_memory_priority", result.stdout)


if __name__ == "__main__":
    unittest.main()
