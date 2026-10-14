#!/usr/bin/env python3
"""List the host's Vulkan physical devices, and generate the configuration that
gives each one its own guest adapter (docs/kb/11-implementation-plan.md, "One
adapter per host GPU").

Each host GPU becomes one virtio-gpu-av-pci device bound by its Vulkan device
UUID, which, unlike enumeration order, survives reboots and driver updates.
With no GPU, llvmpipe is listed as a CPU device, so there is always one.

  list_gpus.py                     a table: index, UUID, vendor:device, type, local memory, name
  list_gpus.py --qemu              one -device argument pair per GPU, for a QEMU command line
  list_gpus.py --libvirt           a libvirt domain fragment passing the same arguments
  list_gpus.py --adapter-names     what provision.ps1 names each GPU's INF model with (C:\\av\\adapters.txt):
                                   "<vendor>\t<device>\t<name>" a line, hex in lowercase, CPU devices left out
  list_gpus.py --vulkaninfo FILE   read captured `vulkaninfo` output instead of running it

What a guest will call each one is here too, by the device's own rules: the
adapter name the capset carries (src/device/gpu/adapter_name.hpp) and the PCI
identity the function presents (src/device/gpu/host_pci_identity.hpp), which
tools/guest/run_guest_suite.py --all-gpus checks Windows against.
"""
import argparse
import pathlib
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
FIELD = re.compile(r"^\s*(deviceName|vendorID|deviceID|deviceType|deviceUUID|pciDomain|pciBus|pciDevice|pciFunction)"
                   r"\s*=\s*(.+?)\s*$", re.M)
PCI_FIELDS = ("pciDomain", "pciBus", "pciDevice", "pciFunction")
SYSFS_PCI = "/sys/bus/pci/devices"
# The capset's name field: 48 bytes, one of them the NUL (GpuIdentity::name_bytes).
NAME_BYTES = 47
# virtio-gpu's own identity, which the function keeps for a GPU with no PCI function.
VIRTIO_GPU = (0x1AF4, 0x1050, 0x01)
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

        self.pci_address = None
        if all(name in fields for name in PCI_FIELDS):
            domain, bus, device, function = (int(fields[name]) for name in PCI_FIELDS)
            self.pci_address = f"{domain & 0xFFFF:04x}:{bus & 0xFF:02x}:{device & 0x1F:02x}.{function & 0x7:x}"

    def device_argument(self):
        return f"virtio-gpu-av-pci,host-gpu={self.uuid},max_outputs=1"

    def adapter_name(self, sysfs=SYSFS_PCI):
        """What Windows would call the adapter: AMD's own product table first,
        then, for an integrated GPU (which RADV names after its processor),
        integrated_name; else the Vulkan name, trimmed. The same choice the
        device makes (virtio-gpu-av-host.c)."""
        vendor, device, revision = self.pci_identity(sysfs)
        name = amdgpu_name(vendor, device, revision)
        if name is None and self.kind == "integrated":
            name = integrated_name(vendor, device)
        return adapter_name(name if name is not None else self.name)

    def pci_identity(self, sysfs=SYSFS_PCI):
        """(vendor, device, revision) the device's PCI function presents: its
        GPU's, read from sysfs, or virtio-gpu's own when the GPU has no PCI
        function or sysfs does not describe it."""
        if self.pci_address is None:
            return VIRTIO_GPU
        try:
            read = [sysfs_number(sysfs, self.pci_address, name) for name in ("vendor", "device", "revision")]
        except (OSError, ValueError):
            return VIRTIO_GPU
        if read[0] > 0xFFFF or read[1] > 0xFFFF or read[2] > 0xFF:
            return VIRTIO_GPU
        return tuple(read)


def sysfs_number(sysfs, address, attribute):
    with open(f"{sysfs}/{address}/{attribute}") as value:
        text = value.read().strip()
    if not text.lower().startswith("0x"):
        raise ValueError(f"{address}/{attribute}: {text!r} is not hex")
    return int(text, 16)


AMDGPU_IDS = "/usr/share/libdrm/amdgpu.ids"
PCI_IDS = ("/usr/share/hwdata/pci.ids", "/usr/share/misc/pci.ids")
# The prefix each vendor's Windows driver puts before a product, and the word
# the PCI ID database may already start a product with, which the prefix
# replaces rather than repeats ("Intel Graphics" is "Intel(R) Graphics").
VENDOR_NAMES = {0x1002: ("AMD", "AMD"), 0x8086: ("Intel(R)", "Intel"), 0x10DE: ("NVIDIA", "NVIDIA")}
# An AMD integrated GPU AMD's table leaves out. The PCI ID database's bracket
# lists whatever the device ID was sold as ("Granite Ridge [Radeon 610M]" for
# a desktop Ryzen 9000's GPU, never called that), and AMD's own Windows
# driver names these "AMD Radeon(TM) Graphics".
AMD_INTEGRATED = "AMD Radeon(TM) Graphics"


def amdgpu_name(vendor, device, revision, table=AMDGPU_IDS):
    if vendor != 0x1002:
        return None
    try:
        lines = pathlib.Path(table).read_text().splitlines()
    except OSError:
        return None
    for line in lines:
        fields = [field.strip() for field in line.split(",", 2)]
        try:
            if len(fields) == 3 and int(fields[0], 16) == device and int(fields[1], 16) == revision:
                return fields[2]
        except ValueError:
            continue
    return None


def integrated_name(vendor, device, databases=PCI_IDS):
    """An integrated GPU AMD's table does not name: AMD's generic name for an
    AMD one, the PCI ID database's for another vendor's."""
    return AMD_INTEGRATED if vendor == 0x1002 else pci_ids_name(vendor, device, databases)


def pci_ids_name(vendor, device, databases=PCI_IDS):
    prefix, word = VENDOR_NAMES.get(vendor, (None, None))
    for database in databases if prefix else ():
        try:
            lines = pathlib.Path(database).read_text(errors="replace").splitlines()
        except OSError:
            continue
        in_vendor = False
        for line in lines:
            if line and not line.startswith(("\t", "#")):
                in_vendor = line.startswith(f"{vendor:04x}  ")
                continue
            if in_vendor and line.startswith(f"\t{device:04x}  "):
                product = line[len(f"\t{device:04x}  "):]
                bracketed = re.search(r"\[([^\]]+)\][^\[]*$", product)
                picked = bracketed.group(1) if bracketed else product
                return f"{prefix} {picked.removeprefix(word + ' ')}"
    return None


def adapter_name(vulkan_name):
    """The Vulkan deviceName without a trailing parenthesised group set off by
    a space - the driver suffix Mesa appends - cut to the capset's field at a
    UTF-8 character boundary: AdapterName's rule, byte for byte."""
    name = vulkan_name.encode()
    kept = len(name)
    end = len(name.rstrip(b" "))
    if end and name[end - 1:end] == b")":
        depth, opening = 0, end
        while True:
            opening -= 1
            depth += {b")": 1, b"(": -1}.get(name[opening:opening + 1], 0)
            if depth == 0 or opening == 0:
                break
        before = len(name[:opening].rstrip(b" "))
        if depth == 0 and before not in (0, opening):
            kept = before
    if kept > NAME_BYTES:
        kept = NAME_BYTES
        while kept and name[kept] & 0xC0 == 0x80:
            kept -= 1
    return name[:kept].decode()


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


def adapter_names(gpus, sysfs=SYSFS_PCI):
    lines = []
    for gpu in gpus:
        if gpu.kind == "cpu":
            continue
        vendor, device, _revision = gpu.pci_identity(sysfs)
        lines.append(f"{vendor:04x}\t{device:04x}\t{gpu.adapter_name(sysfs)}\n")
    return "".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    output = parser.add_mutually_exclusive_group()
    output.add_argument("--qemu", action="store_true")
    output.add_argument("--libvirt", action="store_true")
    output.add_argument("--adapter-names", action="store_true")
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
    if arguments.adapter_names:
        sys.stdout.write(adapter_names(gpus))
        return
    print(qemu(gpus) if arguments.qemu else libvirt(gpus) if arguments.libvirt else table(gpus))


if __name__ == "__main__":
    main()
