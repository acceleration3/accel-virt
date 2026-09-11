import unittest

from source_tree import SourceTree
from tool_run import ROOT, run

GENERATOR = ROOT / "tools" / "gen_ddi_slots.py"

TWO_ROWS = """| # | field | type | since |
|--:|---|---|---|
| 0 | `Version` | `ULONG` | BASE |
| 1 | `DxgkDdiAddDevice` | `PDXGKDDI_ADD_DEVICE` | BASE |
| 2 | `Reserved` | `VOID*` | WIN7 |
"""


class GenDdiSlots(unittest.TestCase):
    def setUp(self):
        self.tree = SourceTree()
        self.addCleanup(self.tree.remove)
        self.tree.write("docs/ref/driver_init_table.md", TWO_ROWS)

    def test_generates_one_enumerator_and_one_offset_proof_per_routine(self):
        result = run(GENERATOR, "--root", self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)
        header = (self.tree.root / "src/core/miniport/ddi_slot.hpp").read_text()
        layout = (self.tree.root / "src/km/ddi_table_layout.cpp").read_text()
        self.assertIn("Version = 0,", header)
        self.assertIn("DxgkDdiAddDevice = 1,", header)
        self.assertIn("Reserved = 2,", header)
        self.assertIn("offsetof(DRIVER_INITIALIZATION_DATA, Reserved)", layout)
        self.assertIn("offsetof(DRIVER_INITIALIZATION_DATA, DxgkDdiAddDevice)", layout)
        self.assertNotIn("offsetof(DRIVER_INITIALIZATION_DATA, Version)", layout)

    def test_check_fails_naming_a_stale_file(self):
        run(GENERATOR, "--root", self.tree.root)
        header = self.tree.root / "src/core/miniport/ddi_slot.hpp"
        header.write_text(header.read_text() + "// edited\n")
        result = run(GENERATOR, "--root", self.tree.root, "--check")
        self.assertEqual(result.returncode, 1)
        self.assertIn("src/core/miniport/ddi_slot.hpp", result.stderr)

    def test_the_committed_files_are_current(self):
        result = run(GENERATOR, "--root", ROOT, "--check")
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
