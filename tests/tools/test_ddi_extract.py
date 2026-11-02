import unittest

from source_tree import SourceTree
from tool_run import ROOT, run

EXTRACT = ROOT / "tools" / "re" / "ddi_extract.py"
KIT = ROOT / "third_party" / "windows-kit"


class DdiExtract(unittest.TestCase):
    @unittest.skipUnless((KIT / "VERSION").exists(), "no Windows kit")
    def test_every_driver_initialization_data_field_is_a_row(self):
        tree = SourceTree()
        self.addCleanup(tree.remove)
        (tree.root / "docs" / "ref").mkdir(parents=True)
        result = run(EXTRACT, KIT, "--ref", tree.root / "docs" / "ref")
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = [line for line in (tree.root / "docs/ref/driver_init_table.md").read_text().splitlines()
                if line.startswith("| ") and not line.startswith("| #")]
        names = [row.split("`")[1] for row in rows]
        self.assertEqual(len(rows), 193)
        self.assertEqual(names[0], "Version")
        for reserved in ("Reserved", "Reserved1", "Reserved2", "Reserved3", "Reserved4"):
            self.assertIn(reserved, names)
        self.assertEqual(names[-1], "DxgkDdiResetDisplayEngine")

    def test_the_committed_table_matches_the_kit(self):
        if not (KIT / "VERSION").exists():
            self.skipTest("no Windows kit")
        tree = SourceTree()
        self.addCleanup(tree.remove)
        (tree.root / "docs" / "ref").mkdir(parents=True)
        run(EXTRACT, KIT, "--ref", tree.root / "docs" / "ref")
        for name in ("driver_init_table.md", "ddi_by_version.json"):
            self.assertEqual((tree.root / "docs/ref" / name).read_text(),
                             (ROOT / "docs/ref" / name).read_text(), name)


if __name__ == "__main__":
    unittest.main()
