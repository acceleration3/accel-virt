#!/usr/bin/env python3
"""Gate: the driver image is a native x64 image that binds only to the kernel.

Linking proves every symbol resolved; it does not prove they resolved to the
kernel. A user-mode import library on the link line links cleanly and then
fails to load in the guest; so does a header flag a user-mode linker default
sets.

  check_driver_image.py build/driver/avgpu.sys [--require-import NAME ...]

--require-import proves a static library's code was linked in, e.g. that
displib's DxgkInitialize reached the image through the routines it imports.
"""
import argparse
import os
import re
import subprocess
import sys

MACHINE = "IMAGE_FILE_MACHINE_AMD64"
SUBSYSTEM = "IMAGE_SUBSYSTEM_NATIVE"
TERMINAL_SERVER_AWARE = "IMAGE_DLL_CHARACTERISTICS_TERMINAL_SERVER_AWARE"
KERNEL_MODULES = {"ntoskrnl.exe", "hal.dll"}


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
    # ntoskrnl's MiObtainSectionForDriver refuses such a native image with
    # STATUS_CONFLICTING_ADDRESSES before DriverEntry runs.
    if TERMINAL_SERVER_AWARE in dump.stdout:
        violations.append(f"{TERMINAL_SERVER_AWARE} set, which the kernel will not load")
    for module in re.findall(r"^\s*Name: (\S+)", dump.stdout, re.M):
        if module.lower() not in KERNEL_MODULES:
            violations.append(f"imports {module}, not a kernel module")
    imported = set(re.findall(r"^\s*Symbol: (\S+)", dump.stdout, re.M))
    for routine in args.require_import:
        if routine not in imported:
            violations.append(f"does not import {routine}")

    for violation in violations:
        print(f"{image}: {violation}", file=sys.stderr)
    if violations:
        sys.exit(1)
    print("driver_image: ok")


if __name__ == "__main__":
    main()
