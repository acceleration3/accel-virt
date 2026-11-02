import pathlib
import tempfile
import unittest

from tool_run import ROOT  # noqa: F401  (keeps tests/tools' import path)

import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))

from cts_results import CtsRatchet, QpaResults, SuiteComparison  # noqa: E402


def case(name, status):
    return (f"#beginTestCaseResult {name}\n"
            '<?xml version="1.0"?>\n'
            f'<TestCaseResult Version="0.3.4" CasePath="{name}" CaseType="SelfValidate">\n'
            f'<Result StatusCode="{status}">{status}</Result>\n'
            "</TestCaseResult>\n"
            "#endTestCaseResult\n")


def session(*bodies, ended=True):
    text = "#beginSession\n#sessionInfo releaseName git-0000\n" + "".join(bodies)
    return text + ("#endSession\n" if ended else "")


class Logs(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name)

    def log(self, text, name="results.qpa"):
        path = self.root / name
        path.write_text(text)
        return QpaResults(path)


class QpaReading(Logs):
    def test_every_status_is_kept_as_the_run_wrote_it(self):
        statuses = ("Pass", "Fail", "QualityWarning", "CompatibilityWarning", "NotSupported",
                    "ResourceError", "InternalError", "Crash", "Timeout", "Waiver")
        results = self.log(session(*(case(f"dEQP-VK.group.{status.lower()}", status) for status in statuses)))
        self.assertEqual(len(results.cases), len(statuses))
        self.assertTrue(results.session_ended)
        self.assertIsNone(results.unfinished)
        for status in statuses:
            self.assertEqual(results.cases[f"dEQP-VK.group.{status.lower()}"], status)
        self.assertTrue(results.passed("dEQP-VK.group.pass"))
        self.assertTrue(results.passed("dEQP-VK.group.qualitywarning"))
        self.assertTrue(results.passed("dEQP-VK.group.compatibilitywarning"))
        self.assertTrue(results.passed("dEQP-VK.group.waiver"))
        for status in ("fail", "notsupported", "resourceerror", "internalerror", "crash", "timeout"):
            self.assertFalse(results.passed(f"dEQP-VK.group.{status}"))
        self.assertFalse(results.passed("dEQP-VK.group.absent"))

    def test_a_case_that_never_ended_is_the_one_a_run_died_in(self):
        results = self.log(session(case("dEQP-VK.api.first", "Pass"),
                                   "#beginTestCaseResult dEQP-VK.api.second\n", ended=False))
        self.assertEqual(results.unfinished, "dEQP-VK.api.second")
        self.assertEqual(results.cases["dEQP-VK.api.second"], "Crash")
        self.assertFalse(results.passed("dEQP-VK.api.second"))
        self.assertFalse(results.session_ended)
        self.assertTrue(results.passed("dEQP-VK.api.first"))

    def test_a_terminated_case_keeps_the_reason_the_run_gave(self):
        results = self.log(session("#beginTestCaseResult dEQP-VK.api.slow\n",
                                   "#terminateTestCaseResult Timeout\n",
                                   case("dEQP-VK.api.after", "Pass")))
        self.assertEqual(results.cases["dEQP-VK.api.slow"], "Timeout")
        self.assertIsNone(results.unfinished)
        self.assertTrue(results.passed("dEQP-VK.api.after"))
        bare = self.log(session("#beginTestCaseResult dEQP-VK.api.mystery\n",
                                "#terminateTestCaseResult\n"), name="bare.qpa")
        self.assertEqual(bare.cases["dEQP-VK.api.mystery"], "Crash")

    def test_a_log_truncated_mid_record_answers_what_it_held(self):
        whole = session(case("dEQP-VK.api.one", "Pass"), case("dEQP-VK.api.two", "Fail"))
        for cut in (len(whole) // 3, len(whole) // 2, len(whole) - 5):
            results = self.log(whole[:cut], name=f"cut{cut}.qpa")
            self.assertLessEqual(len(results.cases), 2)
            self.assertFalse(results.session_ended)
            for name, status in results.cases.items():
                self.assertTrue(name.startswith("dEQP-VK.api."))
                self.assertTrue(status)

    def test_a_status_nobody_defines_is_never_silently_a_pass(self):
        results = self.log(session(case("dEQP-VK.api.strange", "PassWithFlyingColours"),
                                   case("dEQP-VK.api.empty", "")))
        self.assertEqual(results.cases["dEQP-VK.api.strange"], "PassWithFlyingColours")
        self.assertFalse(results.passed("dEQP-VK.api.strange"))
        self.assertFalse(results.passed("dEQP-VK.api.empty"))

    def test_a_name_no_case_could_have_is_not_a_case(self):
        long_name = "dEQP-VK." + "x" * 600
        results = self.log(session("#beginTestCaseResult dEQP-VK.api with a space\n",
                                   '<Result StatusCode="Pass">Pass</Result>\n#endTestCaseResult\n',
                                   f"#beginTestCaseResult {long_name}\n",
                                   '<Result StatusCode="Pass">Pass</Result>\n#endTestCaseResult\n',
                                   "#beginTestCaseResult <script>\n",
                                   '<Result StatusCode="Pass">Pass</Result>\n#endTestCaseResult\n',
                                   case("dEQP-VK.api.real", "Pass")))
        self.assertEqual(list(results.cases), ["dEQP-VK.api.real"])
        self.assertIsNone(results.unfinished)

    def test_two_logs_read_as_one_keep_the_case_the_first_died_in(self):
        died = session(case("dEQP-VK.api.first", "Pass"),
                       "#beginTestCaseResult dEQP-VK.api.killer\n", ended=False)
        resumed = session(case("dEQP-VK.api.after", "Pass"))
        results = self.log(died + resumed)
        self.assertEqual(results.cases["dEQP-VK.api.killer"], "Crash")
        self.assertTrue(results.passed("dEQP-VK.api.first"))
        self.assertTrue(results.passed("dEQP-VK.api.after"))
        self.assertTrue(results.session_ended)
        self.assertIsNone(results.unfinished)

    def test_a_duplicate_case_keeps_what_it_answered_last(self):
        results = self.log(session("#beginTestCaseResult dEQP-VK.api.twice\n",
                                   "#terminateTestCaseResult Crash\n",
                                   case("dEQP-VK.api.twice", "Pass")))
        self.assertEqual(results.cases["dEQP-VK.api.twice"], "Pass")
        self.assertEqual(len(results.cases), 1)

    def test_a_file_that_is_not_a_log_answers_nothing(self):
        for text in ("", "not a qpa at all\n", "#beginSession\n#endSession\n"):
            results = self.log(text, name="odd.qpa")
            self.assertEqual(results.cases, {})
            self.assertIsNone(results.unfinished)
        missing = QpaResults(self.root / "nowhere.qpa")
        self.assertEqual(missing.cases, {})
        self.assertFalse(missing.session_ended)


class Comparison(Logs):
    def results(self, pairs, name):
        return self.log(session(*(case(case_name, status) for case_name, status in pairs)), name=name)

    def test_a_case_the_guest_alone_fails_is_the_verdict(self):
        guest = self.results([("dEQP-VK.a", "Pass"), ("dEQP-VK.b", "Fail"), ("dEQP-VK.c", "Crash")], "guest.qpa")
        host = self.results([("dEQP-VK.a", "Pass"), ("dEQP-VK.b", "Pass"), ("dEQP-VK.c", "Fail")], "host.qpa")
        compared = SuiteComparison(guest, host)
        self.assertEqual(compared.guest_only_failures, ["dEQP-VK.b"])
        self.assertEqual(compared.matching_failures, ["dEQP-VK.c"])
        self.assertEqual(compared.guest_only_passes, [])
        self.assertEqual(compared.unjudged, [])
        self.assertEqual(compared.compared, 3)

    def test_a_case_on_one_side_only_is_not_a_comparison(self):
        guest = self.results([("dEQP-VK.a", "Pass"), ("dEQP-VK.only.guest", "Pass")], "guest.qpa")
        host = self.results([("dEQP-VK.a", "Pass"), ("dEQP-VK.only.host", "Pass")], "host.qpa")
        compared = SuiteComparison(guest, host)
        self.assertEqual(compared.unjudged, ["dEQP-VK.only.guest", "dEQP-VK.only.host"])
        self.assertEqual(compared.guest_only_failures, [])

    def test_not_supported_matches_only_itself(self):
        guest = self.results([("dEQP-VK.ext", "NotSupported"), ("dEQP-VK.other", "NotSupported")], "guest.qpa")
        host = self.results([("dEQP-VK.ext", "NotSupported"), ("dEQP-VK.other", "Pass")], "host.qpa")
        compared = SuiteComparison(guest, host)
        self.assertEqual(compared.matching_failures, [])
        self.assertEqual(compared.unjudged, [])
        self.assertEqual(compared.compared, 2)
        self.assertEqual(compared.guest_only_failures, ["dEQP-VK.other"])

        skipped_on_the_host = self.results([("dEQP-VK.ext", "Pass"), ("dEQP-VK.two", "Fail")], "g2.qpa")
        host_without = self.results([("dEQP-VK.ext", "NotSupported"), ("dEQP-VK.two", "NotSupported")], "h2.qpa")
        other_way = SuiteComparison(skipped_on_the_host, host_without)
        self.assertEqual(other_way.unjudged, ["dEQP-VK.ext", "dEQP-VK.two"])
        self.assertEqual(other_way.compared, 0)

    def test_a_case_the_guest_alone_passes_is_reported(self):
        guest = self.results([("dEQP-VK.a", "Pass")], "guest.qpa")
        host = self.results([("dEQP-VK.a", "Fail")], "host.qpa")
        compared = SuiteComparison(guest, host)
        self.assertEqual(compared.guest_only_passes, ["dEQP-VK.a"])
        self.assertEqual(compared.guest_only_failures, [])

    def test_two_empty_runs_compare_nothing(self):
        empty = self.log("", name="empty.qpa")
        compared = SuiteComparison(empty, empty)
        self.assertEqual(compared.compared, 0)
        self.assertEqual(compared.guest_only_failures, [])
        self.assertEqual(compared.unjudged, [])


class Ratchet(Logs):
    def record(self, names, text=None):
        path = self.root / "cts_ratchet.txt"
        path.write_text(text if text is not None else "".join(f"{name}\n" for name in names))
        return CtsRatchet(path)

    def results(self, pairs):
        return self.log(session(*(case(name, status) for name, status in pairs)))

    def test_a_lost_case_is_what_the_record_is_for(self):
        record = self.record(["dEQP-VK.a", "dEQP-VK.b"])
        results = self.results([("dEQP-VK.a", "Pass"), ("dEQP-VK.b", "Fail")])
        self.assertEqual(record.lost(results), ["dEQP-VK.b"])
        self.assertEqual(record.gained(results), [])
        thinner = self.results([("dEQP-VK.a", "Pass")])
        self.assertEqual(record.lost(thinner), ["dEQP-VK.b"])

    def test_a_gained_case_is_written_down_in_the_change_that_earned_it(self):
        record = self.record(["dEQP-VK.a"])
        results = self.results([("dEQP-VK.a", "Pass"), ("dEQP-VK.new", "Pass")])
        self.assertEqual(record.gained(results), ["dEQP-VK.new"])
        self.assertEqual(record.rewritten(results), ["dEQP-VK.a", "dEQP-VK.new"])

    def test_an_empty_record_passes_and_a_malformed_one_does_not(self):
        empty = self.record([], text="# no guest has run the CTS yet\n\n")
        results = self.results([("dEQP-VK.a", "Fail")])
        self.assertEqual(empty.lost(results), [])
        self.assertEqual(empty.names, [])
        with self.assertRaises(ValueError):
            self.record([], text="dEQP-VK.a\nnot a case name\n").names
        missing = CtsRatchet(self.root / "nowhere.txt")
        self.assertEqual(missing.names, [])

    def test_the_rewritten_record_is_sorted_and_each_name_once(self):
        record = self.record(["dEQP-VK.b", "dEQP-VK.a", "dEQP-VK.b"])
        self.assertEqual(record.names, ["dEQP-VK.a", "dEQP-VK.b"])
        results = self.results([("dEQP-VK.b", "Pass"), ("dEQP-VK.a", "Pass"), ("dEQP-VK.c", "Pass")])
        self.assertEqual(record.rewritten(results), ["dEQP-VK.a", "dEQP-VK.b", "dEQP-VK.c"])


if __name__ == "__main__":
    unittest.main()
