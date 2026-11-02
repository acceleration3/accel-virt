#!/usr/bin/env python3
"""The virtio-gpu-av device in the QEMU tools/build_qemu.sh built, with no guest:
qtest's port I/O reads the PCI function a guest driver would see, and the
realize failures a bad host-gpu property produces.

  qemu_smoke.py <qemu build directory> <echo executor library> <table-less library>
                <this project's executor library>
"""
import os
import re
import subprocess
import sys

SLOT = 5
VIRTIO_GPU = 0x10501AF4
AV_SUBSYSTEM = 0x52561AF4
HOST_IDENTITY = re.compile(r"virtio-gpu-av: pci identity ([0-9a-f]{4}):([0-9a-f]{4}) rev ([0-9a-f]{2}), "
                           r"the host's ([0-9a-f]{4}:[0-9a-f]{2}:[0-9a-f]{2}\.[0-7])")
SYSFS_PCI = "/sys/bus/pci/devices"


class Qemu:
    def __init__(self, build):
        self.binary = os.path.join(build, "qemu-system-x86_64")
        self.environment = dict(os.environ, QEMU_MODULE_DIR=build)

    def start(self, host_gpu, options=""):
        command = [self.binary, "-M", "pc", "-display", "none", "-nodefaults", "-machine", "accel=qtest",
                   "-qtest", "stdio", "-device", f"virtio-gpu-av-pci,addr={SLOT:02x}.0,host-gpu={host_gpu}{options}"]
        return subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                text=True, env=self.environment)

    def config_registers(self, host_gpu, registers):
        return self.config_registers_with(host_gpu, "", registers)

    def config_registers_with(self, host_gpu, options, registers):
        process = self.start(host_gpu, options)
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

    def refused(self, host_gpu, options=""):
        process = self.start(host_gpu, options)
        try:
            _, stderr = process.communicate(timeout=60)
        except subprocess.TimeoutExpired:
            process.kill()
            return 0, process.communicate()[1]
        return process.returncode, stderr


def complaints(stderr):
    return "\n".join(line for line in stderr.splitlines()
                     if "virtio-gpu-av" in line and ": probe " not in line and ": pci identity " not in line)


def sysfs_value(address, attribute):
    with open(os.path.join(SYSFS_PCI, address, attribute)) as value:
        return int(value.read(), 16)


def presented_identity(stderr):
    taken = HOST_IDENTITY.search(stderr)
    if taken is None:
        return VIRTIO_GPU, 1
    vendor, device, revision = (int(group, 16) for group in taken.groups()[:3])
    address = taken.group(4)
    check((vendor, device, revision) == (sysfs_value(address, "vendor"), sysfs_value(address, "device"),
                                         sysfs_value(address, "revision")),
          f"the device took {vendor:04x}:{device:04x} rev {revision:02x}, which is not what sysfs says {address} is")
    return device << 16 | vendor, revision


def check(condition, message):
    if not condition:
        print(f"qemu_smoke: {message}", file=sys.stderr)
        sys.exit(1)


def main():
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    qemu = Qemu(sys.argv[1])

    (identity, class_revision, subsystem), stderr = qemu.config_registers("auto", (0x00, 0x08, 0x2C))
    expected, revision = presented_identity(stderr)
    check(identity == expected, f"vendor/device 0x{identity:08x}, want 0x{expected:08x}")
    check(class_revision & 0xFF == revision, f"revision 0x{class_revision & 0xFF:02x}, want 0x{revision:02x}")
    check(subsystem == AV_SUBSYSTEM, f"subsystem 0x{subsystem:08x}, want 0x{AV_SUBSYSTEM:08x}")
    check(not complaints(stderr), f"a device realized on auto logged: {stderr}")
    check(": probe exported memory: " in stderr and ": probe guest import: " in stderr,
          f"a realize said nothing about its probes: {stderr}")

    for host_gpu, reason in (("not-a-uuid", "is neither auto nor a device UUID"),
                             ("00000000-0000-0000-0000-000000000001", "no host Vulkan device has UUID")):
        code, stderr = qemu.refused(host_gpu)
        check(code != 0, f"host-gpu={host_gpu} realized")
        check(reason in stderr, f"host-gpu={host_gpu}: {stderr.strip()}")
    for executor, reason in (("/nonexistent/libav_codec.so", "could not be loaded"),
                             ("libc.so.6", "exports no av_executor_ops"),
                             (sys.argv[3], "answered no table")):
        code, stderr = qemu.refused("auto", f",executor={executor}")
        check(code != 0, f"executor={executor} realized")
        check(reason in stderr, f"executor={executor}: {stderr.strip()}")
    (identity, _), stderr = qemu.config_registers_with("auto", f",executor={sys.argv[2]}", (0x00, 0x2C))
    check(identity == presented_identity(stderr)[0] and not complaints(stderr), f"the echo executor: {stderr}")
    (identity, _), stderr = qemu.config_registers_with("auto", f",executor={sys.argv[4]}", (0x00, 0x2C))
    check(identity == presented_identity(stderr)[0] and not complaints(stderr), f"this project's executor: {stderr}")
    print("qemu_smoke: ok")


if __name__ == "__main__":
    main()
