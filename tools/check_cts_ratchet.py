#!/usr/bin/env python3
"""What the guest passed before, and still must (feature 048, US3).

The comparison needs a host run; this does not. It is the record of what the
guest passes, committed, so a change that loses a case fails on the day it is
made - and a change that gains one writes it down, which is what keeps the
record from being a wish.

  check_cts_ratchet.py <guest.qpa> [--record tools/cts_ratchet.txt] [--write]
"""
import argparse
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from cts_results import CtsRatchet, QpaResults  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent


class RatchetCheck:
    """One run against one record."""

    def __init__(self, results, record, write):
        self.results = results
        self.record = record
        self.write = write

    def report(self):
        run = QpaResults(self.results)
        record = CtsRatchet(self.record)
        try:
            recorded = record.names
        except ValueError as refused:
            print(f"cts_ratchet: {refused}", file=sys.stderr)
            return 2
        lost = record.lost(run)
        gained = record.gained(run)
        for name in lost:
            print(f"cts_ratchet: {name} passed before and does not now", file=sys.stderr)
        if lost:
            print(f"cts_ratchet: {len(lost)} case(s) lost of {len(recorded)} recorded", file=sys.stderr)
            return 1
        print(f"cts_ratchet: {len(recorded) - len(lost)} kept, {len(lost)} lost, {len(gained)} gained")
        for name in gained:
            print(f"cts_ratchet: gained {name}")
        if gained and not self.write:
            print(f"cts_ratchet: commit these {len(gained)} line(s) into {self.record} "
                  "in the change that earned them")
        if self.write:
            self.record.write_text("".join(f"{name}\n" for name in record.rewritten(run)))
            print(f"cts_ratchet: {self.record} rewritten")
        return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=pathlib.Path, help="the .qpa a guest run left")
    parser.add_argument("--record", type=pathlib.Path, default=ROOT / "tools" / "cts_ratchet.txt")
    parser.add_argument("--write", action="store_true",
                        help="rewrite the record with what this run passed")
    arguments = parser.parse_args(argv)
    return RatchetCheck(arguments.results, arguments.record, arguments.write).report()


if __name__ == "__main__":
    sys.exit(main())
