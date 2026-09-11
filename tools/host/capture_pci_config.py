#!/usr/bin/env python3
"""Capture the 256-byte PCI configuration space of a QEMU device, with no guest.

QEMU's qtest accelerator accepts port I/O on stdin, so the classic 0xCF8/0xCFC
configuration mechanism reads the device exactly as a guest driver would see it.
Used to record test fixtures for the virtio capability walk.

  capture_pci_config.py <output.bin> <qemu -device argument> [extra qemu args...]
  capture_pci_config.py tests/fixtures/virtio_gpu_pci_config.bin virtio-gpu-pci
"""
import subprocess
import sys

SLOT = 5


def command(process, line):
    process.stdin.write(line + "\n")
    process.stdin.flush()
    while True:
        reply = process.stdout.readline()
        if not reply:
            sys.exit(f"qemu exited: {process.stderr.read()}")
        if reply.startswith(("OK", "FAIL")):
            return reply.split()


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    output, device, extra = sys.argv[1], sys.argv[2], sys.argv[3:]
    process = subprocess.Popen(
        ["qemu-system-x86_64", "-M", "pc", "-display", "none", "-nodefaults",
         "-machine", "accel=qtest", "-qtest", "stdio",
         "-device", f"{device},addr={SLOT:02x}.0", *extra],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    data = bytearray()
    try:
        for register in range(0, 256, 4):
            command(process, f"outl 0xcf8 0x{0x80000000 | (SLOT << 11) | register:x}")
            data += int(command(process, "inl 0xcfc")[1], 16).to_bytes(4, "little")
    finally:
        process.kill()
    with open(output, "wb") as out:
        out.write(data)
    print(f"{output}: vendor {data[0] | data[1] << 8:04x} device {data[2] | data[3] << 8:04x}")


if __name__ == "__main__":
    main()
