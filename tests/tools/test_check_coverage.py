import importlib.util
import unittest

from source_tree import SourceTree
from tool_run import ROOT

spec = importlib.util.spec_from_file_location("check_coverage",
                                              ROOT / "tools" / "check_coverage.py")
check_coverage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check_coverage)


def branch(line, column, true_count, false_count):
    return [line, column, line, column + 5, true_count, false_count, 0, 0, 4]


def source_file(root, relative, branches):
    covered = sum((t > 0) + (f > 0) for _l, _c, _el, _ec, t, f, *_ in branches)
    return {"filename": str(root / relative), "branches": branches, "expansions": [],
            "summary": {"branches": {"count": 2 * len(branches), "covered": covered}}}


class CoverageGate(unittest.TestCase):
    def setUp(self):
        self.tree = SourceTree()
        self.addCleanup(self.tree.remove)
        self.tree.write("src/core/platform/platform.hpp", "#define AV_UNTESTABLE(reason) /* reason */\n")
        self.tree.write("tests/untestable_baseline.txt", "0\n")
        self.profiles = self.tree.root / "build" / "coverage"
        self.profiles.mkdir(parents=True)
        (self.profiles / "test_a.profraw").write_bytes(b"")

    def evaluate(self, files, binaries=("test_a",)):
        export = {"data": [{"files": files}]}
        return check_coverage.CoverageVerdict(self.tree.root, export, binaries,
                                              self.profiles).violations()

    def test_full_coverage_of_src_passes_and_test_code_is_excluded(self):
        files = [source_file(self.tree.root, "src/core/a.cpp", [branch(3, 1, 1, 1)]),
                 source_file(self.tree.root, "src/core/a.hpp", [branch(4, 1, 2, 2)]),
                 source_file(self.tree.root, "tests/unit/test_a.cpp", [branch(9, 1, 1, 0)])]
        self.assertEqual(self.evaluate(files), [])

    def test_names_each_untaken_direction_and_the_percentage(self):
        files = [source_file(self.tree.root, "src/core/a.cpp",
                             [branch(41, 9, 3, 0), branch(50, 2, 1, 1)])]
        violations = self.evaluate(files)
        self.assertIn("src/core/a.cpp:41:9: false branch never taken", violations)
        self.assertIn("branch coverage 75.00% < 100%", violations)

    def test_a_source_no_test_binary_links_is_reported(self):
        self.tree.write("src/device/orphan.cpp", "int orphan() { return 1; }\n")
        files = [source_file(self.tree.root, "src/core/a.cpp", [branch(3, 1, 1, 1)])]
        self.assertEqual(self.evaluate(files),
                         ["src/device/orphan.cpp: not linked into any test binary"])

    def test_template_instantiations_of_one_branch_are_merged(self):
        files = [source_file(self.tree.root, "src/core/t.hpp",
                             [branch(13, 24, 3, 0), branch(13, 24, 0, 2)])]
        self.assertEqual(self.evaluate(files), [])

    def test_a_direction_no_instantiation_takes_is_reported_once(self):
        files = [source_file(self.tree.root, "src/core/t.hpp",
                             [branch(13, 24, 3, 0), branch(13, 24, 1, 0)])]
        violations = self.evaluate(files)
        self.assertEqual(violations.count("src/core/t.hpp:13:24: false branch never taken"), 1)
        self.assertIn("branch coverage 50.00% < 100%", violations)

    def test_a_header_branch_counts(self):
        files = [source_file(self.tree.root, "src/device/b.hpp", [branch(7, 3, 0, 4)])]
        self.assertIn("src/device/b.hpp:7:3: true branch never taken", self.evaluate(files))

    def test_no_source_directories_means_zero_branches_not_an_error(self):
        self.assertEqual(self.evaluate([]), [])

    def test_a_test_binary_without_a_profile_fails(self):
        violations = self.evaluate([], binaries=("test_a", "test_b"))
        self.assertEqual(violations, ["test_b: no profile written"])

    def test_marks_above_the_baseline_fail_listing_each(self):
        self.tree.write("src/core/a.cpp",
                        'int a() {\n  AV_UNTESTABLE("hardware only");\n'
                        '  AV_UNTESTABLE("isr race");\n}\n')
        violations = self.evaluate([source_file(self.tree.root, "src/core/a.cpp", [])])
        self.assertEqual(violations, ["AV_UNTESTABLE marks 2 > baseline 0",
                                      "src/core/a.cpp:2: hardware only",
                                      "src/core/a.cpp:3: isr race"])

    def test_marks_below_the_baseline_pass_and_ask_to_lower_it(self):
        self.tree.write("tests/untestable_baseline.txt", "1\n")
        verdict = check_coverage.CoverageVerdict(self.tree.root, {"data": [{"files": []}]},
                                                 ("test_a",), self.profiles)
        self.assertEqual(verdict.violations(), [])
        self.assertIn("lower tests/untestable_baseline.txt to 0", verdict.notes())


if __name__ == "__main__":
    unittest.main()
