#!/usr/bin/env python3
"""The virtio-gpu-rvk device in the QEMU tools/build_qemu.sh built, with no guest:
qtest's port I/O reads the PCI function a guest driver would see, and the
realize failures a bad host-gpu property produces.

  qemu_smoke.py <qemu build directory>
"""
import os
import subprocess
import sys

SLOT = 5
VIRTIO_GPU = 0x10501AF4          # device 1050, vendor 1af4
RVK_SUBSYSTEM = 0x52561AF4       # subsystem 5256 ("RV"), vendor 1af4


class Qemu:
    def __init__(self, build):
        self.binary = os.path.join(build, "qemu-system-x86_64")
        self.environment = dict(os.environ, QEMU_MODULE_DIR=build)

    def start(self, host_gpu):
        command = [self.binary, "-M", "pc", "-display", "none", "-nodefaults", "-machine", "accel=qtest",
                   "-qtest", "stdio", "-device", f"virtio-gpu-rvk-pci,addr={SLOT:02x}.0,host-gpu={host_gpu}"]
        return subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                text=True, env=self.environment)

    def config_registers(self, host_gpu, registers):
        """The PCI configuration registers a guest reads, and what QEMU printed.
        QEMU keeps running on qtest's EOF, so it is stopped once they are read."""
        process = self.start(host_gpu)
        values = []
        try:
            for register in registers:
                self.command(process, f"outl 0xcf8 0x{0x80000000 | (SLOT << 11) | register:x}")
                values.append(int(self.command(process, "inl 0xcfc")[1], 16))
        finally:
            process.kill()
        return values, process.communicate(timeout=60)[1]

    def command(self, process, line):
        process.stdin.write(line + "\n")
        process.stdin.flush()
        while True:
            reply = process.stdout.readline()
            if not reply:
                raise AssertionError(f"qemu exited: {process.stderr.read()}")
            if reply.startswith(("OK", "FAIL")):
                return reply.split()

    def refused(self, host_gpu):
        """The exit status and stderr of a device that must fail to realize."""
        process = self.start(host_gpu)
        try:
            _, stderr = process.communicate(timeout=60)
        except subprocess.TimeoutExpired:
            process.kill()
            return 0, process.communicate()[1]
        return process.returncode, stderr


def check(condition, message):
    if not condition:
        print(f"qemu_smoke: {message}", file=sys.stderr)
        sys.exit(1)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    qemu = Qemu(sys.argv[1])

    (identity, subsystem), stderr = qemu.config_registers("auto", (0x00, 0x2C))
    check(identity == VIRTIO_GPU, f"vendor/device 0x{identity:08x}, want 0x{VIRTIO_GPU:08x}")
    check(subsystem == RVK_SUBSYSTEM, f"subsystem 0x{subsystem:08x}, want 0x{RVK_SUBSYSTEM:08x}")
    check("virtio-gpu-rvk" not in stderr, f"a device realized on auto logged: {stderr}")

    for host_gpu, reason in (("not-a-uuid", "is neither auto nor a device UUID"),
                             ("00000000-0000-0000-0000-000000000001", "no host Vulkan device has UUID")):
        code, stderr = qemu.refused(host_gpu)
        check(code != 0, f"host-gpu={host_gpu} realized")
        check(reason in stderr, f"host-gpu={host_gpu}: {stderr.strip()}")
    print("qemu_smoke: ok")


if __name__ == "__main__":
    main()
