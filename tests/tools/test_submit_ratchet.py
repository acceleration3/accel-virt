import pathlib
import tempfile
import unittest

from tool_run import ROOT  # noqa: F401  (keeps tests/tools' import path)

import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))

from submit_ratchet import SubmitRatchet  # noqa: E402


class SubmitRatchetTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = pathlib.Path(self.directory.name) / "submit_ratchet.txt"

    def record(self, text):
        self.path.write_text(text)
        return SubmitRatchet(self.path)

    def test_the_committed_ratio_is_what_a_run_is_judged_against(self):
        record = self.record("# a comment\n\nratio 450\n")
        self.assertEqual(record.recorded, 450)
        self.assertEqual(record.lost(400), 50)
        self.assertEqual(record.lost(450), 0)
        self.assertEqual(record.lost(900), 0)
        self.assertEqual(record.gained(900), 450)
        self.assertEqual(record.gained(450), 0)
        self.assertEqual(record.gained(100), 0)
        self.assertEqual(record.line(900), "ratio 900\n")

    def test_an_empty_record_loses_nothing_and_gains_whatever_was_measured(self):
        for text in ["", "\n\n", "# EMPTY ON PURPOSE\n"]:
            record = self.record(text)
            self.assertEqual(record.recorded, 0)
            self.assertEqual(record.lost(1), 0)
            self.assertEqual(record.gained(450), 450)

    def test_a_record_that_does_not_exist_reads_as_nothing_recorded(self):
        record = SubmitRatchet(pathlib.Path(self.directory.name) / "nobody_wrote_this.txt")
        self.assertEqual(record.recorded, 0)
        self.assertEqual(record.lost(1), 0)

    def test_anything_that_is_not_one_ratio_is_refused(self):
        refused = ["four hundred\n", "ratio\n", "ratio -5\n", "ratio 4.5\n", "ratio 450 500\n",
                   "ratio 0\n", "ratio 1000001\n", "ratio 450\nratio 460\n", "  ratio450\n"]
        for text in refused:
            record = self.record(text)
            with self.assertRaises(ValueError, msg=text):
                record.recorded

    def test_the_record_this_repository_ships_is_empty_on_purpose(self):
        shipped = SubmitRatchet(ROOT / "tools" / "submit_ratchet.txt")
        self.assertEqual(shipped.recorded, 0)
        self.assertIn("EMPTY ON PURPOSE", (ROOT / "tools" / "submit_ratchet.txt").read_text())


if __name__ == "__main__":
    unittest.main()
