#!/usr/bin/env python3
import sys
import unittest


def tests_of(suite):
    for item in suite:
        if isinstance(item, unittest.TestSuite):
            yield from tests_of(item)
        else:
            yield item


def shard(directory, module, index, count):
    sys.path.insert(0, directory)
    loaded = unittest.defaultTestLoader.loadTestsFromName(module)
    tests = sorted(tests_of(loaded), key=lambda test: test.id())
    return unittest.TestSuite(test for position, test in enumerate(tests) if position % count == index)


def main(argv):
    if len(argv) != 5 or not argv[3].isdigit() or not argv[4].isdigit() or int(argv[3]) >= int(argv[4]):
        print("usage: run_test_shard.py DIRECTORY MODULE INDEX COUNT (INDEX < COUNT)", file=sys.stderr)
        return 2
    selected = shard(argv[1], argv[2], int(argv[3]), int(argv[4]))
    if selected.countTestCases() == 0:
        print(f"run_test_shard: shard {argv[3]} of {argv[4]} of {argv[2]} selects no test", file=sys.stderr)
        return 1
    result = unittest.TextTestRunner(verbosity=1).run(selected)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
