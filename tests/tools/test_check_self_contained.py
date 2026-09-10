import unittest

from source_tree import SourceTree
from tool_run import ROOT, run

CHECK = ROOT / "tools" / "check_self_contained.py"

# Assembled so this file does not itself trip the gate it tests.
SCRATCH = "claude" + "-scratch"
KB = "wddm" + "-kb"
SIBLING = "../" + "render-server"
HOME = "/home" + "/someone/x"


class SelfContained(unittest.TestCase):
    def setUp(self):
        self.tree = SourceTree()
        self.addCleanup(self.tree.remove)
        self.tree.write("README.md", f"# title\n\nsee ../{SCRATCH}/{KB}/kb/x.md\n")
        self.tree.write("NOTES.md", "serialization from " + SIBLING + "\n")
        self.tree.write("docs/local.md", "see ../src/core/x.hpp\n")
        self.tree.write("tools/a.py", 'PATH = "' + HOME + '"\n')
        self.tree.write("specs/001-x/spec.md", "binding: [rules](../../CLAUDE.md)\n")
        self.tree.write("CLAUDE.md", "rules\n")
        self.tree.write("image.bin", "\0\0" + KB + "\0")
        for command in (("git", "init", "-q"), ("git", "add", "."),):
            self.assertEqual(run(*command, cwd=self.tree.root).returncode, 0)
        self.tree.write("junk.md", KB + "\n")

    def check(self):
        return run(CHECK, self.tree.root)

    def test_reports_the_knowledge_base_checkout_with_file_and_line(self):
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn(f"README.md:3: references ../{SCRATCH}/{KB}/kb/x.md", result.stderr)

    def test_reports_a_sibling_project(self):
        self.assertIn("NOTES.md:1: references " + SIBLING, self.check().stderr)

    def test_reports_a_home_directory(self):
        self.assertIn("tools/a.py:1: references " + HOME, self.check().stderr)

    def reported(self):
        stderr = self.check().stderr
        self.assertIn("README.md:3:", stderr)
        return stderr

    def test_a_relative_path_inside_the_repository_is_fine(self):
        stderr = self.reported()
        self.assertNotIn("specs/001-x/spec.md", stderr)
        self.assertNotIn("docs/local.md", stderr)

    def test_each_token_is_reported_once_per_line(self):
        self.tree.write("README.md", f"[../{SCRATCH}/a](../{SCRATCH}/a)\n")
        run("git", "add", ".", cwd=self.tree.root)
        self.assertEqual(self.check().stderr.count("README.md:1:"), 1)

    def test_binary_and_untracked_files_are_ignored(self):
        stderr = self.reported()
        self.assertNotIn("image.bin", stderr)
        self.assertNotIn("junk.md", stderr)

    def test_the_repository_passes(self):
        result = run(CHECK, ROOT)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("self_contained: ok", result.stdout)


if __name__ == "__main__":
    unittest.main()
