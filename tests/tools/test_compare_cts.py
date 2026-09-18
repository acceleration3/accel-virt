"""The two verdicts a conformance run answers with (feature 048, US1 and US3):
the guest against the host, and the guest against what it used to pass.

Both are driven over `.qpa` logs the tests write. A comparison that was never
made says so rather than passing quietly, which is the case worth testing most:
this machine has no CTS, so "could not compare" is what the tools answer today.
"""
import pathlib
import tempfile
import unittest

from tool_run import ROOT, run

COMPARE = ROOT / "tools" / "compare_cts.py"
RATCHET = ROOT / "tools" / "check_cts_ratchet.py"


def case(name, status):
    return (f"#beginTestCaseResult {name}\n"
            f'<Result StatusCode="{status}">{status}</Result>\n'
            "#endTestCaseResult\n")


def log(*pairs):
    return "#beginSession\n" + "".join(case(name, status) for name, status in pairs) + "#endSession\n"


class Verdicts(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name)

    def write(self, name, text):
        path = self.root / name
        path.write_text(text)
        return path

    def test_a_run_with_no_guest_only_failure_passes_and_says_how_many_it_compared(self):
        guest = self.write("guest.qpa", log(("dEQP-VK.a", "Pass"), ("dEQP-VK.b", "Fail")))
        host = self.write("host.qpa", log(("dEQP-VK.a", "Pass"), ("dEQP-VK.b", "Fail")))
        result = run(COMPARE, guest, host)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("cts: 2 compared, 0 guest-only failures", result.stdout)
        self.assertIn("1 matching failure", result.stdout)

    def test_a_guest_only_failure_is_named_one_per_line_and_fails(self):
        guest = self.write("guest.qpa", log(("dEQP-VK.a", "Fail"), ("dEQP-VK.b", "Crash"),
                                            ("dEQP-VK.c", "Pass")))
        host = self.write("host.qpa", log(("dEQP-VK.a", "Pass"), ("dEQP-VK.b", "Pass"),
                                          ("dEQP-VK.c", "Pass")))
        result = run(COMPARE, guest, host)
        self.assertEqual(result.returncode, 1)
        self.assertIn("dEQP-VK.a", result.stderr)
        self.assertIn("dEQP-VK.b", result.stderr)
        self.assertNotIn("dEQP-VK.c", result.stderr)
        self.assertIn("cts: 2 guest-only failure(s)", result.stderr)

    def test_a_comparison_that_was_never_made_says_so(self):
        """Neither an empty log nor a subset that ran on one side only is a
        pass: this machine has no CTS, and what it must never do is report
        conformance it did not measure."""
        empty = self.write("empty.qpa", "")
        whole = self.write("host.qpa", log(("dEQP-VK.a", "Pass")))
        nothing = run(COMPARE, empty, whole)
        self.assertEqual(nothing.returncode, 2)
        self.assertIn("nothing to compare", nothing.stderr)

        one_side = self.write("guest.qpa", log(("dEQP-VK.a", "Pass"), ("dEQP-VK.extra", "Pass")))
        different = run(COMPARE, one_side, whole)
        self.assertEqual(different.returncode, 2)
        self.assertIn("dEQP-VK.extra", different.stderr)
        self.assertIn("not the same cases", different.stderr)

        missing = run(COMPARE, self.root / "nowhere.qpa", whole)
        self.assertEqual(missing.returncode, 2)
        self.assertIn("nothing to compare", missing.stderr)

    def test_a_guest_only_pass_is_reported_without_failing(self):
        guest = self.write("guest.qpa", log(("dEQP-VK.a", "Pass")))
        host = self.write("host.qpa", log(("dEQP-VK.a", "Fail")))
        result = run(COMPARE, guest, host)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("1 the guest alone passed", result.stdout)

    def test_the_ratchet_holds_a_run_to_what_the_guest_used_to_pass(self):
        record = self.write("cts_ratchet.txt", "dEQP-VK.a\ndEQP-VK.b\n")
        kept = self.write("kept.qpa", log(("dEQP-VK.a", "Pass"), ("dEQP-VK.b", "Pass")))
        held = run(RATCHET, kept, "--record", record)
        self.assertEqual(held.returncode, 0, held.stdout + held.stderr)
        self.assertIn("cts_ratchet: 2 kept, 0 lost", held.stdout)

        lost = self.write("lost.qpa", log(("dEQP-VK.a", "Pass"), ("dEQP-VK.b", "Fail")))
        slipped = run(RATCHET, lost, "--record", record)
        self.assertEqual(slipped.returncode, 1)
        self.assertIn("dEQP-VK.b", slipped.stderr)
        self.assertIn("passed before", slipped.stderr)

    def test_a_gain_is_printed_with_the_line_to_commit(self):
        record = self.write("cts_ratchet.txt", "dEQP-VK.a\n")
        gained = self.write("gained.qpa", log(("dEQP-VK.a", "Pass"), ("dEQP-VK.new", "Pass")))
        result = run(RATCHET, gained, "--record", record)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("1 gained", result.stdout)
        self.assertIn("dEQP-VK.new", result.stdout)
        # And --write puts it there, so the gain is committed with the change.
        written = run(RATCHET, gained, "--record", record, "--write")
        self.assertEqual(written.returncode, 0, written.stdout + written.stderr)
        self.assertEqual(record.read_text(), "dEQP-VK.a\ndEQP-VK.new\n")

    def test_the_repositorys_own_record_is_empty_and_passes(self):
        """No guest has run the CTS here, so the record claims nothing - and a
        run against it cannot fail for having lost what was never recorded."""
        anything = self.write("anything.qpa", log(("dEQP-VK.a", "Fail")))
        result = run(RATCHET, anything)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("cts_ratchet: 0 kept, 0 lost", result.stdout)

    def test_a_record_that_is_not_case_names_is_an_error(self):
        record = self.write("cts_ratchet.txt", "dEQP-VK.a\nnot a case name\n")
        anything = self.write("anything.qpa", log(("dEQP-VK.a", "Pass")))
        result = run(RATCHET, anything, "--record", record)
        self.assertEqual(result.returncode, 2)
        self.assertIn("is not a case name", result.stderr)


if __name__ == "__main__":
    unittest.main()
