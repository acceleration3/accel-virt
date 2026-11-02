import pathlib
import sys
import tempfile
import textwrap
import unittest

from tool_run import ROOT, run

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))

from cts_subset import CtsSubset  # noqa: E402

CHECK = ROOT / "tools" / "check_cts_subset.py"


class Subset(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name)

    def subset(self, text):
        path = self.root / "cts_subset.txt"
        path.write_text(text)
        return CtsSubset(path)

    def test_a_group_says_which_extension_it_is_for(self):
        subset = self.subset(textwrap.dedent("""
            # a comment
            dEQP-VK.api.smoke.*        for: core
            dEQP-VK.wsi.win32.*        for: VK_KHR_surface, VK_KHR_swapchain
        """))
        self.assertEqual(subset.groups, [("dEQP-VK.api.smoke.*", ["core"]),
                                         ("dEQP-VK.wsi.win32.*", ["VK_KHR_surface", "VK_KHR_swapchain"])])
        self.assertEqual(subset.extensions(), ["VK_KHR_surface", "VK_KHR_swapchain"])

    def test_what_a_subset_keeps_of_a_case_list_the_cts_printed(self):
        subset = self.subset("dEQP-VK.api.smoke.*   for: core\ndEQP-VK.memory.mapping.*  for: core\n")
        printed = ["dEQP-VK.api.smoke.triangle", "dEQP-VK.pipeline.huge.case",
                   "dEQP-VK.memory.mapping.suballocation.full", "dEQP-VK.api.smoke.triangle"]
        self.assertEqual(subset.matching(printed),
                         ["dEQP-VK.api.smoke.triangle", "dEQP-VK.memory.mapping.suballocation.full"])
        self.assertEqual(subset.matching([]), [])

    def test_a_line_that_is_not_a_group_is_refused(self):
        for text in ("dEQP-VK.api.smoke.*\n", "dEQP-VK.api.smoke.* for:\n",
                     "dEQP-VK.api.smoke.* for: everything\n"):
            with self.assertRaises(ValueError):
                self.subset(text).groups
        with self.assertRaises(ValueError):
            CtsSubset(self.root / "nowhere.txt").groups

    def repository(self, rows, subset):
        table = self.root / "src" / "icd" / "vulkan"
        table.mkdir(parents=True, exist_ok=True)
        (table / "icd_root.cpp").write_text(rows)
        tools = self.root / "tools"
        tools.mkdir(exist_ok=True)
        (tools / "cts_subset.txt").write_text(subset)
        return self.root

    def test_an_extension_the_icd_reports_with_no_group_fails_the_gate(self):
        rows = textwrap.dedent('''
            constexpr ExtensionPolicy::Row reported_extensions[] = {
                {"VK_KHR_swapchain", 70u, "some.test"},
                {"VK_EXT_memory_budget", 1u, "some.other"},
            };
            constexpr InstanceExtension reported_instance_extensions[] = {
                {"VK_KHR_get_physical_device_properties2", 2u},
            };
        ''')
        covered = ("dEQP-VK.wsi.win32.swapchain.*  for: VK_KHR_swapchain\n"
                   "dEQP-VK.api.info.instance.*    for: VK_KHR_get_physical_device_properties2\n")
        result = run(CHECK, self.repository(rows, covered))
        self.assertEqual(result.returncode, 1)
        self.assertIn("VK_EXT_memory_budget is reported by the ICD and no group", result.stderr)

        whole = covered + "dEQP-VK.memory.budget.*   for: VK_EXT_memory_budget\n"
        passing = run(CHECK, self.repository(rows, whole))
        self.assertEqual(passing.returncode, 0, passing.stdout + passing.stderr)
        self.assertIn("3 reported extension(s) covered", passing.stdout)

    def test_a_group_for_an_extension_nobody_reports_fails_too(self):
        rows = 'constexpr ExtensionPolicy::Row reported_extensions[] = {\n    {"VK_KHR_swapchain", 70u, "t.t"},\n};\n'
        result = run(CHECK, self.repository(rows, "dEQP-VK.wsi.*  for: VK_KHR_swapchain\n"
                                                  "dEQP-VK.ray.*  for: VK_KHR_ray_tracing_pipeline\n"))
        self.assertEqual(result.returncode, 1)
        self.assertIn("VK_KHR_ray_tracing_pipeline has a group", result.stderr)

    def test_a_repository_with_no_policy_table_is_an_error(self):
        result = run(CHECK, self.repository("/* nothing */\n", "dEQP-VK.api.*  for: core\n"))
        self.assertEqual(result.returncode, 1)
        self.assertIn("no policy table", result.stderr)

    def test_this_repositorys_own_subset_covers_what_its_icd_reports(self):
        result = run(CHECK, ROOT)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("cts_subset: ok", result.stdout)


if __name__ == "__main__":
    unittest.main()
