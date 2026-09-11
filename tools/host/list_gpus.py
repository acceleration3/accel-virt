#!/usr/bin/env python3
"""List the host's Vulkan physical devices, and generate the configuration that
gives each one its own guest adapter (docs/kb/11-implementation-plan.md, "One
adapter per host GPU").

Each host GPU becomes one virtio-gpu-rvk-pci device bound by its Vulkan device
UUID, which, unlike enumeration order, survives reboots and driver updates.
With no GPU, llvmpipe is listed as a CPU device, so there is always one.

  list_gpus.py                     a table: index, UUID, vendor:device, type, local memory, name
  list_gpus.py --qemu              one -device argument pair per GPU, for a QEMU command line
  list_gpus.py --libvirt           a libvirt domain fragment passing the same arguments
  list_gpus.py --vulkaninfo FILE   read captured `vulkaninfo` output instead of running it
"""
import argparse
import re
import subprocess
import sys
from xml.sax.saxutils import quoteattr

DEVICE_TYPES = {
    "PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU": "integrated",
    "PHYSICAL_DEVICE_TYPE_DISCRETE_GPU": "discrete",
    "PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU": "virtual",
    "PHYSICAL_DEVICE_TYPE_CPU": "cpu",
    "PHYSICAL_DEVICE_TYPE_OTHER": "other",
}
SECTION = re.compile(r"^GPU(\d+):$", re.M)
FIELD = re.compile(r"^\s*(deviceName|vendorID|deviceID|deviceType|deviceUUID)\s*=\s*(.+?)\s*$", re.M)
HEAP = re.compile(r"^\s*memoryHeaps\[\d+\]:\s*$")
HEAP_SIZE = re.compile(r"^\s*size\s*=\s*(\d+)")
UUID = re.compile(r"^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$")
LIBVIRT = """<domain type='kvm' xmlns:qemu='http://libvirt.org/schemas/domain/qemu/1.0'>
  <memoryBacking><source type='memfd'/><access mode='shared'/></memoryBacking>
  <devices><video><model type='none'/></video></devices>
  <qemu:commandline>
{arguments}
  </qemu:commandline>
</domain>"""


class Gpu:
    def __init__(self, index, fields, local_memory):
        self.index = index
        self.name = fields["deviceName"]
        self.vendor = int(fields["vendorID"], 16)
        self.device = int(fields["deviceID"], 16)
        self.kind = DEVICE_TYPES.get(fields["deviceType"], "other")
        self.uuid = fields["deviceUUID"]
        self.local_memory = local_memory

    def device_argument(self):
        return f"virtio-gpu-rvk-pci,host-gpu={self.uuid},max_outputs=1"


def local_memory(section):
    """Bytes in heaps flagged MEMORY_HEAP_DEVICE_LOCAL_BIT."""
    total, size, local = 0, 0, False
    for line in section.splitlines() + [""]:
        if HEAP.match(line) or line.startswith("memoryTypes"):
            total += size if local else 0
            size, local = 0, False
            continue
        match = HEAP_SIZE.match(line)
        if match:
            size = int(match.group(1))
        local = local or "MEMORY_HEAP_DEVICE_LOCAL_BIT" in line
    return total + (size if local else 0)


def parse(text):
    """The GPUs in full `vulkaninfo` output, from its per-device sections."""
    starts = list(SECTION.finditer(text))
    gpus = []
    for position, start in enumerate(starts):
        end = starts[position + 1].start() if position + 1 < len(starts) else len(text)
        section = text[start.end():end]
        fields = {}
        for match in FIELD.finditer(section):
            fields.setdefault(match.group(1), match.group(2))
        missing = {"deviceName", "vendorID", "deviceID", "deviceType", "deviceUUID"} - fields.keys()
        if missing:
            raise ValueError(f"GPU{start.group(1)}: no {', '.join(sorted(missing))}")
        if not UUID.match(fields["deviceUUID"]):
            raise ValueError(f"GPU{start.group(1)}: malformed deviceUUID {fields['deviceUUID']}")
        gpus.append(Gpu(int(start.group(1)), fields, local_memory(section)))
    return gpus


def table(gpus):
    lines = [f"{'#':>2}  {'uuid':36}  {'id':9}  {'type':10}  {'local':>9}  name"]
    for gpu in gpus:
        memory = f"{gpu.local_memory / (1 << 30):.1f} GiB"
        lines.append(f"{gpu.index:>2}  {gpu.uuid:36}  {gpu.vendor:04x}:{gpu.device:04x}  "
                     f"{gpu.kind:10}  {memory:>9}  {gpu.name}")
    return "\n".join(lines)


def qemu(gpus):
    return "\n".join(f"-device {gpu.device_argument()}" for gpu in gpus)


def libvirt(gpus):
    arguments = []
    for gpu in gpus:
        arguments.append("    <qemu:arg value='-device'/>")
        arguments.append(f"    <qemu:arg value={quoteattr(gpu.device_argument())}/>")
    return LIBVIRT.format(arguments="\n".join(arguments))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    output = parser.add_mutually_exclusive_group()
    output.add_argument("--qemu", action="store_true")
    output.add_argument("--libvirt", action="store_true")
    parser.add_argument("--vulkaninfo", metavar="FILE")
    arguments = parser.parse_args()
    if arguments.vulkaninfo:
        try:
            with open(arguments.vulkaninfo) as captured:
                text = captured.read()
        except OSError as error:
            sys.exit(f"cannot read {arguments.vulkaninfo}: {error.strerror}")
    else:
        try:
            text = subprocess.run(["vulkaninfo"], capture_output=True, text=True, check=True).stdout
        except (OSError, subprocess.CalledProcessError) as error:
            sys.exit(f"vulkaninfo failed: {error}")
    try:
        gpus = parse(text)
    except ValueError as error:
        sys.exit(f"vulkaninfo output: {error}")
    if not gpus:
        sys.exit("no Vulkan physical device found")
    print(qemu(gpus) if arguments.qemu else libvirt(gpus) if arguments.libvirt else table(gpus))


if __name__ == "__main__":
    main()
