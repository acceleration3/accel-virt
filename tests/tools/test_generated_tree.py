"""Every generator's --check (GeneratedTree): a committed tree that matches
what the generator writes passes; one that lost a file, gained one, or drifted
by a byte fails and says which; and a missing --out is refused rather than
made. The Vulkan generator's own copy crashed on a missing file rather than
reporting it.
"""
import contextlib
import io
import pathlib
import shutil
import sys
import tempfile
import unittest

from tool_run import ROOT

sys.path.insert(0, str(ROOT / "tools" / "codegen"))

from av_codegen.emit.common import GeneratedTree, missing_input  # noqa: E402


def emit_two(out: pathlib.Path) -> None:
    (out / "a.h").write_text("int a;\n")
    (out / "b.cpp").write_text("int b;\n")


class GeneratedTreeCheck(unittest.TestCase):
    def setUp(self):
        self.out = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.out)
        emit_two(self.out)
        (self.out / ".clang-format").write_text("DisableFormat: true\n")

    def check(self):
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors):
            code = GeneratedTree(self.out).check(emit_two)
        return code, errors.getvalue()

    def test_a_tree_that_matches_passes_and_its_dotfiles_are_not_output(self):
        self.assertEqual(self.check(), (0, ""))

    def test_a_missing_file_is_reported_not_a_crash(self):
        (self.out / "b.cpp").unlink()
        code, errors = self.check()
        self.assertEqual(code, 1)
        self.assertIn("file set differs", errors)

    def test_an_extra_file_is_reported(self):
        (self.out / "stale.h").write_text("int stale;\n")
        code, errors = self.check()
        self.assertEqual(code, 1)
        self.assertIn("stale.h", errors)

    def test_a_drifted_file_is_named(self):
        (self.out / "a.h").write_text("int a = 1;\n")
        code, errors = self.check()
        self.assertEqual(code, 1)
        self.assertIn("a.h differs from committed copy", errors)

    def test_a_missing_out_is_refused_and_not_made(self):
        gone = self.out / "nowhere"
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors):
            self.assertEqual(GeneratedTree(gone).check(emit_two), 1)
        self.assertFalse(gone.exists())
        self.assertIn("--out does not exist", errors.getvalue())


class MissingInput(unittest.TestCase):
    def test_names_every_input_that_is_not_a_file(self):
        class Args:
            xml = ROOT / "CMakeLists.txt"
            proc_table = ROOT / "no-such-file"
            ids = ROOT / "neither"
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors):
            self.assertTrue(missing_input(Args, ("xml", "proc_table", "ids")))
            self.assertFalse(missing_input(Args, ("xml",)))
        self.assertIn("--proc-table does not exist", errors.getvalue())
        self.assertIn("--ids does not exist", errors.getvalue())
        self.assertNotIn("--xml", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
