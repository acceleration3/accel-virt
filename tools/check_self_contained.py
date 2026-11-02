#!/usr/bin/env python3
import os
import re
import subprocess
import sys

FORBIDDEN = re.compile(r"[\w./~-]*(?:" + "claude" + "-scratch" + "|" + "wddm" + "-kb"
                       + r")[\w./-]*|/home/[\w./-]+")
RELATIVE = re.compile(r"(?<![\w./])(?:\.\./)+[\w.-][\w./-]*")


def tracked_files(root):
    listing = subprocess.run(["git", "-C", root, "ls-files", "-z"],
                             capture_output=True, text=True, check=True).stdout
    return [name for name in listing.split("\0") if name]


def text_of(path):
    with open(path, "rb") as source:
        data = source.read()
    if b"\0" in data:
        return None
    return data.decode("utf-8", errors="replace")


def escapes(root, relative, token):
    target = os.path.normpath(os.path.join(root, os.path.dirname(relative), token))
    return os.path.commonpath([root, target]) != root


def main():
    root = os.path.realpath(sys.argv[1] if len(sys.argv) > 1 else ".")
    violations = []
    for relative in tracked_files(root):
        path = os.path.join(root, relative)
        if not os.path.isfile(path):
            continue
        text = text_of(path)
        if text is None:
            continue
        for number, line in enumerate(text.split("\n"), 1):
            found = FORBIDDEN.findall(line)
            found += [t for t in RELATIVE.findall(line) if escapes(root, relative, t)]
            for token in dict.fromkeys(t.rstrip(".") for t in found):
                violations.append(f"{relative}:{number}: references {token}")
    for violation in violations:
        print(violation, file=sys.stderr)
    if violations:
        sys.exit(1)
    print("self_contained: ok")


if __name__ == "__main__":
    main()
