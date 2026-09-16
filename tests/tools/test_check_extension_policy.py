"""The ICD reports an extension only through a policy row that names a test
which exists and an extension the generator implemented, or the gate says which
row does not (tools/check_extension_policy.py)."""
import pathlib
import shutil
import tempfile
import textwrap
import unittest

from tool_run import ROOT, run

CHECK = ROOT / "tools" / "check_extension_policy.py"

SUPPORTED = textwrap.dedent('''
    inline constexpr const char* kImplementedDeviceExtensions[] = {
        "VK_KHR_spirv_1_4",
        // "VK_KHR_commented_out",
        "VK_KHR_timeline_semaphore",
    };
    inline constexpr const char* kImplementedInstanceExtensions[] = {
        "VK_KHR_surface",
    };
''')


class CheckExtensionPolicy(unittest.TestCase):
    def setUp(self):
        self.root = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.root)
        self.write("src/proto/generated/vulkan/vk_supported_extensions.h", SUPPORTED)
        self.write("tests/unit/icd/vulkan/test_things.cpp",
                   "TEST(memory_entries, reports_the_budget)\n{ }\n"
                   "TEST( probe , enables_spirv )\n{ }\n"
                   "/* TEST(commented, out) */\n")

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def table(self, rows, where="src/icd/vulkan/icd_root.cpp"):
        self.write(where, "namespace {\nconst ExtensionPolicy::Row reported_extensions[] = {\n" + rows + "\n};\n}\n")

    def check(self):
        return run(CHECK, self.root)

    def test_a_null_table_reports_nothing_and_passes(self):
        self.write("src/icd/vulkan/icd_root.cpp",
                   "constexpr const ExtensionPolicy::Row *reported_extensions = nullptr;\n")
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("extension_policy: ok, 0 reported", result.stdout)

    def test_rows_naming_existing_tests_and_implemented_extensions_pass(self):
        self.table('    /* why this is proven */\n'
                   '    {"VK_KHR_spirv_1_4", 1u, "probe.enables_spirv"},\n'
                   '    {"VK_KHR_timeline_semaphore", 2, "memory_entries.reports_the_budget"}')
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("ok, 2 reported", result.stdout)

    def test_a_row_naming_a_test_that_does_not_exist_fails(self):
        self.table('{"VK_KHR_spirv_1_4", 1u, "probe.enables_nothing"}')
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("VK_KHR_spirv_1_4 names probe.enables_nothing, which no TEST under tests/ defines", result.stderr)

    def test_a_test_only_in_a_comment_does_not_exist(self):
        self.table('{"VK_KHR_spirv_1_4", 1u, "commented.out"}')
        self.assertIn("which no TEST", self.check().stderr)

    def test_a_row_naming_no_test_as_suite_case_fails(self):
        self.table('{"VK_KHR_spirv_1_4", 1u, "tests/unit/icd/vulkan/test_things.cpp"}')
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("not a test as suite.case", result.stderr)

    def test_an_extension_the_generator_did_not_implement_fails(self):
        for name in ("VK_EXT_memory_budget", "VK_KHR_commented_out", "VK_KHR_surface"):
            with self.subTest(name=name):
                self.table('{"' + name + '", 1u, "probe.enables_spirv"}')
                result = self.check()
                self.assertEqual(result.returncode, 1)
                self.assertIn(f"{name} is not in src/proto/generated/vulkan/vk_supported_extensions.h",
                              result.stderr)

    def test_a_name_twice_or_a_zero_version_fails(self):
        self.table('{"VK_KHR_spirv_1_4", 1u, "probe.enables_spirv"},\n{"VK_KHR_spirv_1_4", 0u, "probe.enables_spirv"}')
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("VK_KHR_spirv_1_4 is named twice", result.stderr)
        self.assertIn("VK_KHR_spirv_1_4 has spec version 0", result.stderr)

    def test_a_row_it_cannot_read_fails_rather_than_hiding(self):
        for rows in ('{"VK_KHR_spirv_1_4", 1u, "probe.enables_spirv"},\n{kSpirvName, 1u, "probe.enables_spirv"}',
                     '{"VK_KHR_spirv_1_4", 1u}', 'Row{"VK_KHR_spirv_1_4", 1u, "probe.enables_spirv"}'):
            with self.subTest(rows=rows):
                self.table(rows)
                result = self.check()
                self.assertEqual(result.returncode, 1)
                self.assertIn("unreadable row", result.stderr)

    def test_no_table_or_two_tables_fail(self):
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("no ICD directory in src/icd", result.stderr)
        self.write("src/icd/vulkan/icd_root.cpp", "// no table\n")
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("want one reported_extensions table in src/icd/vulkan, found 0: nowhere", result.stderr)
        self.table('{"VK_KHR_spirv_1_4", 1u, "probe.enables_spirv"}')
        self.write("src/icd/vulkan/other.hpp", "const ExtensionPolicy::Row *reported_extensions = nullptr;\n")
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("found 2: src/icd/vulkan/icd_root.cpp, src/icd/vulkan/other.hpp", result.stderr)

    def test_a_table_outside_the_icd_or_in_a_comment_is_not_the_table(self):
        self.write("src/core/elsewhere.cpp", "const ExtensionPolicy::Row *reported_extensions = nullptr;\n")
        self.write("src/icd/vulkan/icd_root.cpp",
                   "/* const ExtensionPolicy::Row *reported_extensions = nullptr; */\n")
        self.assertIn("found 0", self.check().stderr)

    def test_each_icd_directory_has_its_own_table(self):
        self.table('{"VK_KHR_spirv_1_4", 1u, "probe.enables_spirv"}')
        self.write("src/icd/opengl/gl_icd_root.cpp",
                   "constexpr const ExtensionPolicy::Row *reported_extensions = nullptr;\n")
        self.write("src/icd/common/extension_policy.cpp", "// the class, no table\n")
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("ok, 1 reported (opengl 0, vulkan 1)", result.stdout)
        # One directory's table does not stand in for another's.
        self.write("src/icd/opengl/gl_icd_root.cpp", "// no table yet\n")
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("want one reported_extensions table in src/icd/opengl, found 0", result.stderr)
        self.assertNotIn("src/icd/vulkan,", result.stderr)

    def test_a_directory_with_no_source_is_no_icd_yet(self):
        self.table('{"VK_KHR_spirv_1_4", 1u, "probe.enables_spirv"}')
        self.write("src/icd/opengl/gl_helpers.h", "// hooks the generated client calls\n")
        self.write("src/icd/opengl/window_surface.hpp", "class WindowSurface;\n")
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("ok, 1 reported (vulkan 1)", result.stdout)
        # Its first source makes it an ICD, which then needs its table.
        self.write("src/icd/opengl/pixel_format_table.cpp", "// no table\n")
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("want one reported_extensions table in src/icd/opengl, found 0", result.stderr)

    def test_rows_are_judged_against_their_own_apis_list(self):
        self.table('{"VK_KHR_spirv_1_4", 1u, "probe.enables_spirv"}')
        self.table('{"VK_KHR_spirv_1_4", 1u, "probe.enables_spirv"}', where="src/icd/opengl/gl_icd_root.cpp")
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("extension_policy: no generated list is known for opengl", result.stderr)
        self.assertNotIn("vulkan", result.stderr)

    def test_a_table_in_the_shared_directory_belongs_to_no_icd(self):
        self.table('{"VK_KHR_spirv_1_4", 1u, "probe.enables_spirv"}')
        self.write("src/icd/common/extension_policy.hpp",
                   "const ExtensionPolicy::Row *reported_extensions = nullptr;\n")
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("src/icd/common/extension_policy.hpp: a reported_extensions table in src/icd/common "
                      "belongs to no ICD", result.stderr)

    def test_without_the_generated_list_rows_cannot_be_judged(self):
        (self.root / "src/proto/generated/vulkan/vk_supported_extensions.h").unlink()
        self.table('{"VK_KHR_spirv_1_4", 1u, "probe.enables_spirv"}')
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("extension_policy: no src/proto/generated/vulkan/vk_supported_extensions.h", result.stderr)

    def test_a_generated_list_without_the_device_array_implements_nothing(self):
        self.write("src/proto/generated/vulkan/vk_supported_extensions.h", "// nothing\n")
        self.table('{"VK_KHR_spirv_1_4", 1u, "probe.enables_spirv"}')
        self.assertIn("is not in", self.check().stderr)

    def test_the_repository_passes(self):
        result = run(CHECK, ROOT)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("extension_policy: ok", result.stdout)


if __name__ == "__main__":
    unittest.main()
