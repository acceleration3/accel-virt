#!/usr/bin/env python3
"""qemu-img create for the runner's tests: an overlay file naming its backing
image, which must exist; $FAKE_QEMU_IMG_FAIL makes it refuse."""
import os
import sys


def main():
    argv = sys.argv[1:]
    if os.environ.get("FAKE_QEMU_IMG_FAIL"):
        sys.exit("qemu-img: Could not open backing file")
    backing = argv[argv.index("-b") + 1]
    if argv[0] != "create" or not os.path.isfile(backing):
        sys.exit(f"qemu-img: bad invocation {argv}")
    with open(argv[-1], "w") as overlay:
        overlay.write(f"overlay of {backing}\n")


if __name__ == "__main__":
    main()
