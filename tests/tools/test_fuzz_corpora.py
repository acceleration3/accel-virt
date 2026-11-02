import unittest

from source_tree import SourceTree
from tool_run import ROOT, run

SCRIPT = ROOT / "cmake" / "check_fuzz_corpora.cmake"


class FuzzCorpora(unittest.TestCase):
    def setUp(self):
        self.tree = SourceTree()
        self.addCleanup(self.tree.remove)
        for name in ("fuzz_a", "fuzz_b", "fuzz_c"):
            self.tree.write(f"tests/fuzz/{name}.cpp", "int x;\n")
        self.tree.write("tests/corpus/fuzz_a/seed", "x")
        (self.tree.root / "tests" / "corpus" / "fuzz_b").mkdir(parents=True)

    def check(self, root):
        return run("cmake", f"-DAV_SOURCE_DIR={root}", "-P", SCRIPT)

    def test_names_every_target_with_a_missing_or_empty_corpus(self):
        result = self.check(self.tree.root)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("fuzz_b", result.stderr)
        self.assertIn("fuzz_c", result.stderr)
        self.assertNotIn("fuzz_a", result.stderr)

    def test_passes_when_every_target_has_seeds(self):
        self.tree.write("tests/corpus/fuzz_b/seed", "x")
        self.tree.write("tests/corpus/fuzz_c/seed", "x")
        result = self.check(self.tree.root)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_the_repository_passes(self):
        result = self.check(ROOT)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
