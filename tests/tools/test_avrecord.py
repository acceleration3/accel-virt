"""avrecord, as a person runs it: a recording printed, a damaged one reported,
a usage error refused, and a recording replayed into an executor library.

The recordings are the committed seed corpus of `fuzz_recording_reader`, which
is what the device writes; the tool is skipped when the tree is not built, so
this test says what it could not check rather than failing a source checkout.
"""
import pathlib
import shutil
import struct
import tempfile
import unittest

from tool_run import ROOT, run

TOOL = ROOT / "build" / "avrecord"
ECHO = ROOT / "build" / "libav_echo_executor.so"
CORPUS = ROOT / "tests" / "corpus" / "fuzz_recording_reader"


def built(path):
    return path.is_file()


class AvRecord(unittest.TestCase):
    def setUp(self):
        if not built(TOOL):
            self.skipTest(f"{TOOL} is not built")
        self.scratch = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)

    def recording(self, name):
        target = self.scratch / name
        shutil.copyfile(CORPUS / "one-of-each", target)
        return target

    def test_prints_every_record_of_a_recording(self):
        answered = run(TOOL, "print", self.recording("run.av"))
        self.assertEqual(answered.returncode, 0, answered.stderr)
        lines = answered.stdout.splitlines()
        self.assertEqual(len(lines), 8, answered.stdout)
        self.assertIn("setup api=vulkan", lines[0])
        self.assertIn("batch ", lines[1])
        self.assertIn("vkCreateInstance", lines[2])
        self.assertIn("reply ", lines[3])
        self.assertIn("fence completed", lines[4])
        self.assertIn("fault ", lines[5])
        self.assertIn("blob imported", lines[6])
        self.assertIn("blob bytes", lines[7])
        self.assertNotIn("damaged", answered.stdout)

    def test_shows_the_payload_bytes_it_is_asked_for(self):
        answered = run(TOOL, "print", self.recording("run.av"), "--bytes", "4")
        self.assertEqual(answered.returncode, 0, answered.stderr)
        self.assertRegex(answered.stdout, r"vkCreateInstance 8 bytes reply [0-9A-F]{8}")

    def test_a_filter_keeps_only_what_it_names(self):
        answered = run(TOOL, "print", self.recording("run.av"), "--command", "vkCreate")
        self.assertEqual(answered.returncode, 0, answered.stderr)
        self.assertIn("vkCreateInstance", answered.stdout)
        self.assertNotIn("fence completed", answered.stdout)

    def test_a_recording_cut_short_prints_what_it_can_and_says_where_the_damage_is(self):
        cut = self.recording("cut.av")
        whole = cut.read_bytes()
        cut.write_bytes(whole[: len(whole) - 8])
        answered = run(TOOL, "print", cut)
        self.assertEqual(answered.returncode, 1, answered.stdout)
        self.assertIn("setup api=vulkan", answered.stdout)
        self.assertRegex(answered.stdout, r"damaged at [0-9A-F]{8}: ")

    def test_a_file_that_is_no_recording_is_one_line_of_damage(self):
        nothing = self.scratch / "nothing.av"
        nothing.write_bytes(b"not a recording")
        answered = run(TOOL, "print", nothing)
        self.assertEqual(answered.returncode, 1, answered.stdout)
        self.assertEqual(len(answered.stdout.splitlines()), 1)
        self.assertIn("damaged at 00000000", answered.stdout)

    def test_a_usage_error_is_refused_with_a_reason(self):
        for arguments in (["print"],
                          ["show", "run.av"],
                          ["print", "run.av", "--nonsense", "1"],
                          ["replay", "run.av"]):
            answered = run(TOOL, *arguments)
            self.assertEqual(answered.returncode, 2, answered.stdout)
            self.assertTrue(answered.stderr.startswith("avrecord: "), answered.stderr)

    def test_a_recording_that_is_not_there_is_refused(self):
        answered = run(TOOL, "print", self.scratch / "missing.av")
        self.assertEqual(answered.returncode, 2)
        self.assertIn("could not be read", answered.stderr)

    def test_a_library_that_is_no_executor_is_refused(self):
        answered = run(TOOL, "replay", self.recording("run.av"), "--executor", self.scratch / "no.so")
        self.assertEqual(answered.returncode, 2)
        self.assertIn("no executor library", answered.stderr)

    def test_replays_a_recording_into_an_executor_library(self):
        if not built(ECHO):
            self.skipTest(f"{ECHO} is not built")
        answered = run(TOOL, "replay", self.recording("run.av"), "--executor", ECHO)
        self.assertIn("replay: 1 contexts,", answered.stdout)
        # The echo library answers every command with its payload, and the
        # recording's reply is not that, so the difference is reported and the
        # run is a failure - which is the point of comparing at all.
        self.assertEqual(answered.returncode, 1, answered.stdout)
        self.assertRegex(answered.stdout, r"ctx=\d+ fence=\d+ replied \d+ bytes, recorded \d+")

    def test_a_replay_of_a_recording_with_no_batches_reports_no_difference(self):
        if not built(ECHO):
            self.skipTest(f"{ECHO} is not built")
        setup_only = self.scratch / "setup.av"
        whole = (CORPUS / "one-of-each").read_bytes()
        # The file header and the setup record alone: 64 + 32 + the setup's 32.
        setup_only.write_bytes(whole[:128])
        answered = run(TOOL, "replay", setup_only, "--executor", ECHO)
        self.assertEqual(answered.returncode, 0, answered.stdout + answered.stderr)
        self.assertIn("replay: 1 contexts, 0 commands, 0 differences", answered.stdout)


class Corpus(unittest.TestCase):
    """The committed seeds are recordings, so the tool's tests mean something."""

    def test_the_seed_the_tool_reads_is_a_recording_of_every_kind(self):
        whole = (CORPUS / "one-of-each").read_bytes()
        self.assertEqual(struct.unpack_from("<I", whole)[0], 0x00525641)
        kinds, offset = [], 64
        while offset < len(whole):
            magic, kind = struct.unpack_from("<IH", whole, offset)
            self.assertEqual(magic, 0x00445641)
            length = struct.unpack_from("<I", whole, offset + 12)[0]
            kinds.append(kind)
            offset += 32 + length
        self.assertEqual(kinds, [1, 2, 3, 4, 5, 6, 7])


if __name__ == "__main__":
    unittest.main()
