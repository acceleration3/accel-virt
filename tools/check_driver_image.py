#!/usr/bin/env python3
"""Gate: the driver image is a native x64 image that binds only to the kernel.

Linking proves every symbol resolved; it does not prove they resolved to the
kernel. A user-mode import library on the link line links cleanly and then
fails to load in the guest.

  check_driver_image.py build/driver/rvkgpu.sys
"""
import os
import re
import subprocess
import sys

MACHINE = "IMAGE_FILE_MACHINE_AMD64"
SUBSYSTEM = "IMAGE_SUBSYSTEM_NATIVE"
KERNEL_MODULES = {"ntoskrnl.exe", "hal.dll"}


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: check_driver_image.py <image>")
    image = sys.argv[1]
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
        if module.lower() not in KERNEL_MODULES:
            violations.append(f"imports {module}, not a kernel module")

    for violation in violations:
        print(f"{image}: {violation}", file=sys.stderr)
    if violations:
        sys.exit(1)
    print("driver_image: ok")


if __name__ == "__main__":
    main()
