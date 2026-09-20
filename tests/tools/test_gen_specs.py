import pathlib
import shutil
import tempfile
import unittest

from tool_run import ROOT, run

GEN_SPECS = ROOT / "tools" / "gen_specs.py"
PLACEHOLDER = "| | object / system | | |"
FILLED = "| Foo | object | `a_` | Owns one foo |"
CLASS_NOTE = "No other class: the adapter answers from state it owns."
# What a plan writes between the heading and the table to say where the classes
# live, as feature 052's spec does. It is inside the generated block too.
CLASS_LEAD = "Guest user mode, `src/icd/opencl/` (namespace `av::clicd`):"


class GenSpecsPreservesClassDecisions(unittest.TestCase):
    def setUp(self):
        self.root = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.root)
        (self.root / "docs" / "ref").mkdir(parents=True)
        shutil.copy(ROOT / "docs" / "ref" / "driver_init_table.md",
                    self.root / "docs" / "ref")
        first = run(GEN_SPECS, "--root", self.root)
        self.assertEqual(first.returncode, 0, first.stderr)
        self.filled = next((self.root / "specs").glob("001-*")) / "spec.md"
        self.untouched = next((self.root / "specs").glob("002-*")) / "spec.md"
        text = self.filled.read_text()
        self.assertEqual(text.count(PLACEHOLDER), 1)
        self.filled.write_text(text.replace(PLACEHOLDER, FILLED + "\n\n" + CLASS_NOTE)
                               + "\nprose written by hand\n")

    def regenerate(self):
        again = run(GEN_SPECS, "--root", self.root)
        self.assertEqual(again.returncode, 0, again.stderr)

    def test_a_filled_row_survives_regeneration(self):
        self.regenerate()
        text = self.filled.read_text()
        self.assertIn(FILLED, text)
        self.assertNotIn(PLACEHOLDER, text)

    def test_a_note_written_after_the_rows_survives_regeneration(self):
        self.regenerate()
        text = self.filled.read_text()
        self.assertIn(FILLED + "\n\n" + CLASS_NOTE + "\n\n### Test requirements", text)
        self.assertEqual(text.count(CLASS_NOTE), 1)

    def test_a_line_written_above_the_table_survives_regeneration(self):
        """Where a plan says its classes live. It sits between the heading and
        the table header, which is inside the generated block, so regeneration
        has to carry it over with the rows - feature 052 lost exactly this
        line, and the rows it named came through."""
        heading = "### Classes this feature introduces"
        text = self.filled.read_text()
        self.filled.write_text(text.replace(heading + "\n\n", heading + "\n\n" + CLASS_LEAD + "\n\n", 1))
        self.regenerate()
        after = self.filled.read_text()
        self.assertIn(heading + "\n\n" + CLASS_LEAD + "\n", after)
        self.assertEqual(after.count(CLASS_LEAD), 1)
        self.assertIn(FILLED, after)

    def test_an_unfilled_table_keeps_only_the_placeholder(self):
        self.regenerate()
        text = self.untouched.read_text()
        self.assertEqual(text.count(PLACEHOLDER), 1)
        self.assertNotIn(FILLED, text)

    def test_prose_below_the_block_is_preserved(self):
        self.regenerate()
        self.assertTrue(self.filled.read_text().endswith("prose written by hand\n"))

    def test_reserved_fields_are_not_listed_as_ddis(self):
        self.regenerate()
        wddm20 = next((self.root / "specs").glob("017-*")) / "spec.md"
        text = wddm20.read_text()
        self.assertIn("DxgkDdiSubmitCommandVirtual", text)
        self.assertNotIn("`Reserved1`", text)

    def test_regenerating_twice_changes_nothing(self):
        self.regenerate()
        once = self.filled.read_text()
        self.regenerate()
        self.assertEqual(self.filled.read_text(), once)


if __name__ == "__main__":
    unittest.main()
