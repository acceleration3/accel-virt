#!/usr/bin/env python3
"""The verdict of a conformance run (feature 048, FR-002): the same subset in
the guest and on the host, and every case that passes there and not here.

An absolute pass count says nothing - the same subset on the same GPU fails
differently between driver versions, and a host GPU missing an extension fails
cases that were never this driver's to pass. So this compares, and refuses to
compare two runs that were not the same subset rather than answering about
them.

  compare_cts.py <guest.qpa> <host.qpa>
"""
import argparse
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from cts_results import QpaResults, SuiteComparison  # noqa: E402


class Verdict:
    def __init__(self, guest, host):
        self.guest = guest
        self.host = host

    def compared(self):
        return SuiteComparison(QpaResults(self.guest), QpaResults(self.host))

    def report(self):
        compared = self.compared()
        if not compared.guest.cases or not compared.host.cases:
            print(f"cts: nothing to compare in {self.guest} and {self.host}", file=sys.stderr)
            return 2
        if compared.compared == 0 and not compared.unjudged:
            print(f"cts: nothing to compare in {self.guest} and {self.host}", file=sys.stderr)
            return 2
        if compared.unjudged:
            for name in compared.unjudged:
                print(f"cts: {name} ran on one side only", file=sys.stderr)
            print(f"cts: the two runs are not the same cases ({len(compared.unjudged)} on one side only)",
                  file=sys.stderr)
            return 2
        if compared.guest_only_failures:
            for name in compared.guest_only_failures:
                print(f"cts: {name} passes on the host and does not in the guest", file=sys.stderr)
            print(f"cts: {len(compared.guest_only_failures)} guest-only failure(s) of "
                  f"{compared.compared} compared", file=sys.stderr)
            return 1
        print(f"cts: {compared.compared} compared, 0 guest-only failures, "
              f"{len(compared.matching_failures)} matching failure(s), "
              f"{len(compared.guest_only_passes)} the guest alone passed")
        return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("guest", type=pathlib.Path, help="the .qpa the guest run left")
    parser.add_argument("host", type=pathlib.Path, help="the .qpa the same subset left on the host")
    arguments = parser.parse_args(argv)
    return Verdict(arguments.guest, arguments.host).report()


if __name__ == "__main__":
    sys.exit(main())
