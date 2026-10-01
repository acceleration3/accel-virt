#!/usr/bin/env python3
"""Run the device's memory feasibility probes on every host GPU and print what
each answered (feature 039).

The device decides two capset bits at realize, on the host GPU its instance is
bound to: whether memory it exports reaches a guest through the hostmem BAR, and
whether the host GPU can address guest pages directly. It says what it found in
one line per probe. This tool starts one device per GPU, reads those lines back,
and prints a JSON object per GPU for the knowledge base.

A refused probe is an answer, not a failure: the tool exits non-zero only when a
device would not start or said nothing about a probe.

  probe_host_memory.py --qemu build-qemu/qemu
  probe_host_memory.py --qemu build-qemu/qemu --guest-check --kernel /boot/vmlinuz-linux
"""
import argparse
import json
import os
import pathlib
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import list_gpus  # noqa: E402

VERDICT = re.compile(r"virtio-gpu-av: probe (exported memory|guest import): "
                     r"(passed, (?P<measured>\d+)|failed, status (?P<status>\d+)(, host (?P<host>-?\d+))?)")
PROBES = {"exported memory": "exported_memory", "guest import": "guest_import"}
GUEST_MEMORY = "512M"
HOSTMEM = "256M"
REALIZE_SECONDS = 60


class ProbeFailed(Exception):
    """A device that would not start, or said nothing about a probe."""


class Device:
    """One QEMU started with the device bound to one host GPU, realized and
    stopped: guest RAM is memfd-backed and shared, which is what a host GPU can
    import, and the hostmem region is what an export is placed in."""

    def __init__(self, qemu, gpu):
        self.qemu = pathlib.Path(qemu)
        self.gpu = gpu

    def command(self):
        return [str(self.qemu / "qemu-system-x86_64"), "-M", "pc", "-m", GUEST_MEMORY,
                "-display", "none", "-nodefaults", "-monitor", "none", "-serial", "none", "-S",
                "-object", f"memory-backend-memfd,id=mem,size={GUEST_MEMORY},share=on",
                "-machine", "memory-backend=mem",
                "-device", f"virtio-gpu-av-pci,addr=05.0,host-gpu={self.gpu.uuid},hostmem={HOSTMEM}"]

    def realize(self):
        """What the device said while realizing; it is stopped once it has."""
        environment = dict(os.environ, QEMU_MODULE_DIR=str(self.qemu))
        started = subprocess.Popen(self.command(), stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   text=True, env=environment)
        try:
            _, said = started.communicate(timeout=REALIZE_SECONDS)
        except subprocess.TimeoutExpired:
            started.kill()
            _, said = started.communicate()
        return said


def verdicts(said, gpu):
    """The probe answers in what a device said, by probe."""
    found = {}
    for match in VERDICT.finditer(said):
        answer = {"outcome": "passed", "measured": int(match.group("measured"))} if match.group("measured") \
            else {"outcome": "failed", "status": int(match.group("status"))}
        if match.group("host"):
            answer["host_error"] = int(match.group("host"))
        found[PROBES[match.group(1)]] = answer
    missing = sorted(set(PROBES.values()) - found.keys())
    if missing:
        raise ProbeFailed(f"the device on {gpu.uuid} said nothing about {', '.join(missing)}:\n{said.strip()}")
    return found


def guest_check(kernel):
    """The KVM half of US1: a guest reads and writes the mapping itself. Without
    a kernel image or /dev/kvm there is nothing to run it in, and a skipped
    check says which."""
    if kernel is None:
        return {"outcome": "skipped", "reason": "no kernel image was named (--kernel)"}
    if not pathlib.Path(kernel).is_file():
        return {"outcome": "skipped", "reason": f"no kernel image at {kernel}"}
    if not os.access("/dev/kvm", os.R_OK | os.W_OK):
        return {"outcome": "skipped", "reason": "/dev/kvm is not readable and writable"}
    # A guest can only read the mapping while a block is placed in the region,
    # and the probe takes its own away inside realize. The guest half is 041's
    # first mapped blob; holding a block just for a guest to find would be a
    # device mode that exists for a test.
    return {"outcome": "skipped", "reason": "the guest reads a mapping only through a blob it asked for (feature 041)"}


def answers(arguments, gpus):
    found = []
    for gpu in gpus:
        answer = {"uuid": gpu.uuid, "name": gpu.name, "vendor": f"{gpu.vendor:04x}:{gpu.device:04x}",
                  "kind": gpu.kind}
        answer.update(verdicts(Device(arguments.qemu, gpu).realize(), gpu))
        if arguments.guest_check:
            answer["guest_check"] = guest_check(arguments.kernel)
        found.append(answer)
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--qemu", required=True, help="the QEMU build directory (build-qemu/qemu)")
    parser.add_argument("--vulkaninfo", metavar="FILE", help="read captured vulkaninfo output instead of running it")
    parser.add_argument("--guest-check", action="store_true", help="also boot a guest that reads the mapping")
    parser.add_argument("--kernel", help="the kernel image the guest check boots")
    arguments = parser.parse_args()
    if arguments.vulkaninfo:
        with open(arguments.vulkaninfo) as captured:
            gpus = list_gpus.parse(captured.read())
    else:
        gpus = list_gpus.parse(subprocess.run(["vulkaninfo"], capture_output=True, text=True, check=True).stdout)
    try:
        print(json.dumps(answers(arguments, gpus), indent=2))
    except ProbeFailed as failure:
        print(f"probe_host_memory: {failure}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
