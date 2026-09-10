import unittest

from source_tree import SourceTree
from tool_run import ROOT, run

CHECK = ROOT / "tools" / "check_design.py"


class DesignRules(unittest.TestCase):
    def setUp(self):
        self.tree = SourceTree()
        self.addCleanup(self.tree.remove)

    def test_reports_a_static_member_function(self):
        self.tree.write("src/core/bad.hpp",
                        "class Loose {\npublic:\n    static int f();\nprivate:\n    int x_;\n};\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("src/core/bad.hpp: Loose::f is static", result.stderr)

    def test_reports_a_class_with_no_data_members(self):
        self.tree.write("src/device/bad.hpp", "class Empty {\npublic:\n    void g();\n};\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("src/device/bad.hpp: class Empty has no data members", result.stderr)

    def test_a_pure_abstract_contract_is_not_stateless(self):
        self.tree.write("src/proto/contract.hpp",
                        "class Contract {\npublic:\n    virtual ~Contract();\n"
                        "    virtual int f() = 0;\n};\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_does_not_judge_whether_symbols_are_tested(self):
        self.tree.write("src/core/fine.hpp",
                        "class Fine {\npublic:\n    int never_named();\nprivate:\n    int v_;\n};\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("design_rules: ok", result.stdout)

    def test_the_repository_passes(self):
        result = run(CHECK, ROOT)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("design_rules: ok", result.stdout)


if __name__ == "__main__":
    unittest.main()
