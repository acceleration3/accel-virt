import unittest

from source_tree import SourceTree
from tool_run import ROOT, run

CHECK = ROOT / "tools" / "check_symbol_tests.py"

HEADER = """class Widget {
public:
    int named();
    int unnamed();
private:
    int hidden();
    int v_;
};
"""


class SymbolTests(unittest.TestCase):
    def setUp(self):
        self.tree = SourceTree()
        self.addCleanup(self.tree.remove)
        self.tree.write("src/core/widget.hpp", HEADER)

    def test_reports_a_public_method_no_test_names(self):
        self.tree.write("tests/unit/test_widget.cpp", "Widget w; w.named();\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stderr.strip().splitlines(),
                         ["src/core/widget.hpp: Widget::unnamed has no test naming it"])

    def test_reports_a_class_no_test_names(self):
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 1)
        self.assertIn("src/core/widget.hpp: class Widget has no test naming it", result.stderr)

    def test_the_harness_does_not_count_as_a_test(self):
        self.tree.write("tests/harness/fixture.cpp", "Widget w; w.named(); w.unnamed();\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 1)

    def test_passes_when_every_public_method_is_named(self):
        self.tree.write("tests/unit/test_widget.cpp", "Widget w; w.named(); w.unnamed();\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("symbol_tests: ok", result.stdout)

    def test_a_private_nested_type_is_covered_through_its_class(self):
        self.tree.write("src/core/table.hpp",
                        "class Table {\npublic:\n    int size();\n    struct Row {\n        int v_;\n    };\n"
                        "private:\n    struct Slot {\n        int id_;\n    };\n    Slot slots_[4];\n"
                        "public:\n    int after();\n};\n")
        self.tree.write("tests/unit/test_widget.cpp", "Widget w; w.named(); w.unnamed();\n")
        self.tree.write("tests/unit/test_table.cpp", "Table t; t.size();\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(sorted(result.stderr.strip().splitlines()),
                         ["src/core/table.hpp: Table::after has no test naming it",
                          "src/core/table.hpp: class Row has no test naming it"])

    def test_calls_inside_an_inline_template_body_are_not_declarations(self):
        self.tree.write("src/core/holder.hpp", """template <typename T>
class Holder {
public:
    explicit Holder(T value) : value_(value)
    {
        check();
        if (value_) { touch(); }
    }

private:
    void check() const { helper(); }
    T value_;
};
""")
        self.tree.write("tests/unit/test_widget.cpp", "Widget w; w.named(); w.unnamed(); Holder h;\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_the_platform_contract_is_excluded(self):
        self.tree.write("src/core/platform/platform.hpp",
                        "class Platform {\npublic:\n    virtual int allocate() = 0;\n};\n")
        self.tree.write("tests/unit/test_widget.cpp", "Widget w; w.named(); w.unnamed();\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_the_register_window_contract_is_excluded(self):
        self.tree.write("src/core/platform/register_window.hpp",
                        "class RegisterWindow {\npublic:\n    virtual int read_u8() = 0;\n};\n")
        self.tree.write("tests/unit/test_widget.cpp", "Widget w; w.named(); w.unnamed();\n")
        result = run(CHECK, self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_the_repository_passes(self):
        result = run(CHECK, ROOT)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("symbol_tests: ok", result.stdout)


if __name__ == "__main__":
    unittest.main()


class ShimDirectories(unittest.TestCase):
    """Constitution II again: a class that only calls the host's own loaders
    cannot be named by a unit test on a machine that may have no GPU, and the
    L3 and L5 layers are where it is proven (feature 042)."""

    def setUp(self):
        self.tree = SourceTree()
        self.addCleanup(self.tree.remove)
        self.tree.write("tests/unit/test_nothing.cpp", "// names nothing\n")

    def check(self):
        return run(ROOT / "tools" / "check_symbol_tests.py", self.tree.root)

    def test_a_shim_class_needs_no_test_naming_it(self):
        self.tree.write("src/device/shim/loader.hpp",
                        "class Loader {\npublic:\n  void open();\nprivate:\n  int fd_;\n};\n")
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_the_same_class_outside_one_still_does(self):
        self.tree.write("src/device/portable/loader.hpp",
                        "class Loader {\npublic:\n  void open();\nprivate:\n  int fd_;\n};\n")
        result = self.check()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Loader", result.stdout + result.stderr)
