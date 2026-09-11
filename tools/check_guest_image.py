#!/usr/bin/env python3
"""Gate: rvktest.exe is an x64 console program that binds only to the user-mode
modules every Windows guest has - gdi32 for D3DKMT, kernel32, ntdll - so it
runs in a guest with nothing installed beside it.

  check_guest_image.py build/guest/rvktest.exe [--require-import NAME ...]
"""
import argparse
import os
import re
import subprocess
import sys

MACHINE = "IMAGE_FILE_MACHINE_AMD64"
SUBSYSTEM = "IMAGE_SUBSYSTEM_WINDOWS_CUI"
GUEST_MODULES = {"gdi32.dll", "kernel32.dll", "ntdll.dll"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image")
    parser.add_argument("--require-import", action="append", default=[])
    args = parser.parse_args()
    image = args.image
    if not os.path.isfile(image):
        print(f"{image}: no such image", file=sys.stderr)
        sys.exit(1)
    dump = subprocess.run(["llvm-readobj", "--file-headers", "--coff-imports", image],
                          capture_output=True, text=True)
    if dump.returncode != 0:
        print(f"{image}: llvm-readobj failed: {dump.stderr.strip()}", file=sys.stderr)
        sys.exit(1)

    violations = []
    machine = re.search(r"^\s*Machine: (\w+)", dump.stdout, re.M)
    if not machine or machine.group(1) != MACHINE:
        violations.append(f"machine {machine.group(1) if machine else 'unknown'}, want {MACHINE}")
    subsystem = re.search(r"^\s*Subsystem: (\w+)", dump.stdout, re.M)
    if not subsystem or subsystem.group(1) != SUBSYSTEM:
        violations.append(f"subsystem {subsystem.group(1) if subsystem else 'unknown'}, want {SUBSYSTEM}")
    for module in re.findall(r"^\s*Name: (\S+)", dump.stdout, re.M):
        if module.lower() not in GUEST_MODULES:
            violations.append(f"imports {module}, not a module every guest has")
    imported = set(re.findall(r"^\s*Symbol: (\S+)", dump.stdout, re.M))
    for routine in args.require_import:
        if routine not in imported:
            violations.append(f"does not import {routine}")

    for violation in violations:
        print(f"{image}: {violation}", file=sys.stderr)
    if violations:
        sys.exit(1)
    print("guest_image: ok")


if __name__ == "__main__":
    main()
