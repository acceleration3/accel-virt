import pathlib
import sys
import tempfile
import unittest

from tool_run import ROOT  # noqa: F401  (keeps tests/tools' import path)

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))

from recording_cost import HEADER, RecordingCost  # noqa: E402


class RecordingCostTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = pathlib.Path(self.directory.name) / "recording-cost.txt"

    def record(self, text=None):
        if text is not None:
            self.path.write_text(text)
        return RecordingCost(self.path)

    def test_a_pair_of_round_trips_is_the_cost_of_a_recording(self):
        record = self.record("# a comment\n\noff 640\non 712\n")
        self.assertEqual(record.measured, {"off": 640, "on": 712})
        self.assertEqual(record.cost(), 72)

    def test_one_side_alone_is_no_cost_at_all(self):
        for text in ("off 640\n", "on 712\n"):
            record = self.record(text)
            self.assertIsNone(record.cost())
            self.assertEqual(len(record.measured), 1)

    def test_an_absent_record_holds_neither_side(self):
        record = RecordingCost(pathlib.Path(self.directory.name) / "no_boot_wrote_this.txt")
        self.assertEqual(record.measured, {})
        self.assertIsNone(record.cost())

    def test_an_empty_record_holds_neither_side(self):
        for text in ("", "\n\n", "# EMPTY ON PURPOSE\n"):
            self.assertEqual(self.record(text).measured, {})

    def test_the_second_boot_replaces_its_own_side_and_keeps_the_others(self):
        record = self.record("off 640\n")
        record.keep("on", 712)
        self.assertEqual(record.cost(), 72)
        self.assertEqual(self.record().measured, {"off": 640, "on": 712})
        record.keep("off", 700)
        self.assertEqual(record.cost(), 12)
        self.assertEqual(self.record().measured, {"off": 700, "on": 712})

    def test_what_is_written_is_what_is_read_back(self):
        record = self.record()
        record.keep("on", 712)
        self.assertEqual(self.path.read_text(), HEADER + "on 712\n")
        record.keep("off", 640)
        self.assertEqual(self.path.read_text(), HEADER + "off 640\non 712\n")

    def test_a_recording_that_cost_nothing_measurable_is_a_negative_difference(self):
        self.assertEqual(self.record("off 700\non 640\n").cost(), -60)
        self.assertEqual(self.record("off 640\non 640\n").cost(), 0)

    def test_a_side_that_is_neither_of_the_two_is_refused(self):
        record = self.record()
        with self.assertRaises(ValueError):
            record.keep("maybe", 640)
        self.assertFalse(self.path.exists())

    def test_anything_that_is_not_two_round_trips_is_refused(self):
        refused = ["six hundred\n", "off\n", "off -5\n", "off 6.4\n", "off 640 700\n", "off 1000001\n",
                   "off 640\noff 700\n", "on 712\non 712\n", "  off640\n", "left 640\n", "OFF 640\n"]
        for text in refused:
            record = self.record(text)
            with self.assertRaises(ValueError, msg=text):
                record.measured

    def test_a_refused_record_is_not_overwritten_by_the_run_that_read_it(self):
        record = self.record("off 640\noff 700\n")
        with self.assertRaises(ValueError):
            record.keep("on", 712)
        self.assertEqual(self.path.read_text(), "off 640\noff 700\n")

    def test_the_largest_round_trip_the_record_believes_is_kept(self):
        self.assertEqual(self.record("off 1000000\non 1000000\n").cost(), 0)


if __name__ == "__main__":
    unittest.main()
