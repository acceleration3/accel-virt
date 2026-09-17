"""tools/run_test_shard.py: shards cover a module exactly once between them."""
import pathlib
import subprocess
import sys
import tempfile
import textwrap
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools" / "run_test_shard.py"

MODULE = textwrap.dedent('''
    import os
    import unittest

    class Many(unittest.TestCase):
        pass

    for number in range(7):
        def test(self, number=number):
            with open(os.environ["SHARD_LOG"], "a") as log:
                log.write(f"{number}\\n")
            if number == 5 and os.environ.get("SHARD_FAIL"):
                self.fail("five")
        setattr(Many, f"test_{number}", test)
''')


class RunTestShard(unittest.TestCase):
    def setUp(self):
        self.directory = pathlib.Path(tempfile.mkdtemp())
        (self.directory / "sharded_module.py").write_text(MODULE)
        self.log = self.directory / "log.txt"

    def run_shard(self, index, count, fail=False):
        environment = {"SHARD_LOG": str(self.log), "PATH": "/usr/bin:/bin"}
        if fail:
            environment["SHARD_FAIL"] = "1"
        return subprocess.run([sys.executable, "-B", str(TOOL), str(self.directory), "sharded_module",
                               str(index), str(count)], capture_output=True, text=True, env=environment)

    def test_three_shards_run_every_test_once(self):
        for index in range(3):
            self.assertEqual(self.run_shard(index, 3).returncode, 0)
        ran = sorted(int(line) for line in self.log.read_text().split())
        self.assertEqual(ran, list(range(7)))

    def test_a_failing_test_fails_only_its_shard(self):
        codes = [self.run_shard(index, 3, fail=True).returncode for index in range(3)]
        # Sorted ids test_0..test_6: test_5 is position 5, shard 2.
        self.assertEqual(codes, [0, 0, 1])

    def test_a_shard_with_no_test_and_bad_arguments_are_refused(self):
        empty = self.run_shard(7, 8)
        self.assertEqual(empty.returncode, 1)
        self.assertIn("selects no test", empty.stderr)
        for arguments in (["3", "3"], ["x", "2"], ["0"]):
            bad = subprocess.run([sys.executable, "-B", str(TOOL), str(self.directory), "sharded_module",
                                  *arguments], capture_output=True, text=True)
            self.assertEqual(bad.returncode, 2, arguments)


if __name__ == "__main__":
    unittest.main()
