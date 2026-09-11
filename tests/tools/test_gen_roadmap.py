import unittest

from tool_run import ROOT, run

ROADMAP = ROOT / "docs" / "roadmap.md"


class GenRoadmap(unittest.TestCase):
    def test_the_committed_roadmap_is_what_the_generator_writes(self):
        committed = ROADMAP.read_text()
        self.addCleanup(ROADMAP.write_text, committed)
        result = run(ROOT / "tools" / "gen_roadmap.py")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(ROADMAP.read_text(), committed)


if __name__ == "__main__":
    unittest.main()
