import pathlib
import sys
import tempfile
import unittest

from tool_run import ROOT  # noqa: F401

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))

from cts_shards import CtsShards  # noqa: E402


class Sharding(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name)

    def subset(self, text, per_shard=2):
        path = self.root / "subset.txt"
        path.write_text(text)
        return CtsShards(path, per_shard)

    def test_every_case_runs_once_in_the_order_the_subset_gave(self):
        shards = self.subset("# a comment\n\ndEQP-VK.a\ndEQP-VK.b\n  dEQP-VK.c  \ndEQP-VK.d\ndEQP-VK.e\n")
        self.assertEqual(shards.cases, ["dEQP-VK.a", "dEQP-VK.b", "dEQP-VK.c", "dEQP-VK.d", "dEQP-VK.e"])
        self.assertEqual(shards.shards(), [["dEQP-VK.a", "dEQP-VK.b"], ["dEQP-VK.c", "dEQP-VK.d"],
                                           ["dEQP-VK.e"]])
        flattened = [name for shard in shards.shards() for name in shard]
        self.assertEqual(flattened, shards.cases)

    def test_a_subset_smaller_than_a_shard_is_one_shard(self):
        shards = self.subset("dEQP-VK.only\n", per_shard=64)
        self.assertEqual(shards.shards(), [["dEQP-VK.only"]])
        self.assertEqual(self.subset("# nothing yet\n").shards(), [])

    def test_a_shard_of_fewer_than_one_case_is_refused(self):
        for size in (0, -1):
            with self.assertRaises(ValueError):
                self.subset("dEQP-VK.a\n", per_shard=size)

    def test_a_name_no_case_could_have_is_refused_rather_than_run(self):
        with self.assertRaises(ValueError):
            self.subset("dEQP-VK.a\nnot a case name\n").cases
        with self.assertRaises(ValueError):
            self.subset("dEQP-VK.a\n" + "dEQP-VK." + "x" * 600 + "\n").cases

    def test_a_shard_is_picked_up_after_the_case_that_died(self):
        shards = self.subset("dEQP-VK.a\ndEQP-VK.b\ndEQP-VK.c\ndEQP-VK.d\n", per_shard=4)
        shard = shards.shards()[0]
        self.assertEqual(shards.after(shard, "dEQP-VK.b"), ["dEQP-VK.c", "dEQP-VK.d"])
        self.assertEqual(shards.after(shard, "dEQP-VK.d"), [])
        self.assertEqual(shards.after(shard, "dEQP-VK.elsewhere"), [])
        self.assertEqual(shards.after(shard, None), [])

    def test_a_case_list_the_guest_printed_is_what_is_sharded(self):
        expanded = self.root / "cases.txt"
        expanded.write_text("dEQP-VK.api.smoke.create_sampler\ndEQP-VK.api.smoke.triangle\n")
        shards = CtsShards(expanded, 1)
        self.assertEqual(shards.shards(), [["dEQP-VK.api.smoke.create_sampler"],
                                           ["dEQP-VK.api.smoke.triangle"]])


if __name__ == "__main__":
    unittest.main()
