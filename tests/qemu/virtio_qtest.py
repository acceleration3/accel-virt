"""A virtio-gpu driver that talks to a real QEMU over qtest, with no guest.

qtest gives port I/O, MMIO and guest physical memory access on QEMU's stdin, so
this module does what a guest's firmware and driver would: assign the PCI
function's BARs, walk its virtio capabilities, negotiate features, lay split
virtqueues out in guest RAM, and exchange virtio-gpu requests on them.
Interrupts are not used: completion is polled from the used ring, which also
lets QEMU's main loop run the device's bottom halves between commands.
"""
import os
import struct
import subprocess
import tempfile
import time

SLOT = 5
RAM_MB = 256
FIRST_BAR_ADDRESS = 0xE0000000
FIRST_ALLOCATION = 0x01000000

PCI_COMMAND = 0x04
PCI_STATUS = 0x06
PCI_CAPABILITY_LIST = 0x34
PCI_COMMAND_MEMORY = 0x2
PCI_COMMAND_MASTER = 0x4
PCI_STATUS_CAPABILITIES = 0x10
PCI_CAPABILITY_VENDOR = 0x09

VIRTIO_PCI_CAP_COMMON = 1
VIRTIO_PCI_CAP_NOTIFY = 2
VIRTIO_PCI_CAP_ISR = 3
VIRTIO_PCI_CAP_DEVICE = 4
VIRTIO_PCI_CAP_SHARED_MEMORY = 8

STATUS_ACKNOWLEDGE = 1
STATUS_DRIVER = 2
STATUS_DRIVER_OK = 4
STATUS_FEATURES_OK = 8

VIRTQ_DESC_F_NEXT = 1
VIRTQ_DESC_F_WRITE = 2

FEATURE_VERSION_1 = 1 << 32
FEATURE_EDID = 1 << 1


class Qtest:
    """One QEMU process driven over qtest; stopped when closed."""

    def __init__(self, build, device):
        self.environment = dict(os.environ, QEMU_MODULE_DIR=build)
        command = [os.path.join(build, "qemu-system-x86_64"), "-M", "pc", "-m", f"{RAM_MB}M",
                   "-display", "none", "-nodefaults", "-machine", "accel=qtest", "-qtest", "stdio",
                   "-qtest-log", "/dev/null", "-device", f"{device},addr={SLOT:02x}.0"]
        # A file, not a pipe: nothing reads stderr while qtest runs, and a full pipe would stop QEMU.
        self.stderr = tempfile.TemporaryFile(mode="w+")
        self.process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=self.stderr, text=True, env=self.environment)

    def close(self):
        """Stops QEMU and answers what it printed on stderr."""
        self.process.kill()
        self.process.wait(timeout=60)
        self.process.stdin.close()
        self.process.stdout.close()
        self.stderr.seek(0)
        printed = self.stderr.read()
        self.stderr.close()
        return printed

    def printed(self):
        """What QEMU has printed on stderr so far. pread, because QEMU writes
        through the same file description and a seek would move its offset."""
        descriptor = self.stderr.fileno()
        return os.pread(descriptor, os.fstat(descriptor).st_size, 0).decode(errors="replace")

    def command(self, line):
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        while True:
            reply = self.process.stdout.readline()
            if not reply:
                self.stderr.seek(0)
                raise RuntimeError(f"qemu exited: {self.stderr.read()}")
            if reply.startswith("OK"):
                return reply.split()
            if reply.startswith("FAIL"):
                raise RuntimeError(f"qtest refused {line!r}: {reply.strip()}")

    def value(self, line):
        return int(self.command(line)[1], 16)

    def read(self, width, address):
        return self.value(f"read{'bwlq'[(8, 16, 32, 64).index(width)]} 0x{address:x}")

    def write(self, width, address, value):
        self.command(f"write{'bwlq'[(8, 16, 32, 64).index(width)]} 0x{address:x} 0x{value:x}")

    def read_bytes(self, address, size):
        return bytes.fromhex(self.command(f"read 0x{address:x} 0x{size:x}")[1][2:])

    def write_bytes(self, address, data):
        self.command(f"write 0x{address:x} 0x{len(data):x} 0x{data.hex()}")


class GuestMemory:
    """A bump allocator over guest RAM above the first 16 MiB."""

    def __init__(self, qtest):
        self.qtest = qtest
        self.next = FIRST_ALLOCATION

    def allocate(self, size, alignment=0x1000):
        address = (self.next + alignment - 1) & ~(alignment - 1)
        if address + size > RAM_MB << 20:
            raise MemoryError("guest RAM exhausted")
        self.next = address + size
        self.qtest.write_bytes(address, bytes(size))
        return address


class PciFunction:
    """Configuration space through 0xcf8/0xcfc, and BARs assigned as firmware would."""

    def __init__(self, qtest):
        self.qtest = qtest
        self.bars = {}

    def address(self, register):
        return 0x80000000 | (SLOT << 11) | register

    def read(self, width, register):
        self.qtest.command(f"outl 0xcf8 0x{self.address(register):x}")
        return self.qtest.value(f"in{'bwl'[(8, 16, 32).index(width)]} 0xcfc")

    def write(self, width, register, value):
        self.qtest.command(f"outl 0xcf8 0x{self.address(register):x}")
        self.qtest.command(f"out{'bwl'[(8, 16, 32).index(width)]} 0xcfc 0x{value:x}")

    def assign_bars(self):
        """Sizes every memory BAR, places it above FIRST_BAR_ADDRESS, and enables decoding."""
        next_address = FIRST_BAR_ADDRESS
        index = 0
        while index < 6:
            register = 0x10 + 4 * index
            original = self.read(32, register)
            self.write(32, register, 0xFFFFFFFF)
            mask = self.read(32, register)
            is_64 = original & 0x6 == 0x4
            if original & 1 or mask == 0:
                self.write(32, register, original)
                index += 1
                continue
            if is_64:
                self.write(32, register + 4, 0xFFFFFFFF)
                mask |= self.read(32, register + 4) << 32
            else:
                mask |= 0xFFFFFFFF << 32
            size = (~(mask & ~0xF) & 0xFFFFFFFFFFFFFFFF) + 1
            address = (next_address + size - 1) & ~(size - 1)
            self.write(32, register, address & 0xFFFFFFFF)
            if is_64:
                self.write(32, register + 4, address >> 32)
            self.bars[index] = address
            next_address = address + size
            index += 2 if is_64 else 1
        self.write(16, PCI_COMMAND, self.read(16, PCI_COMMAND) | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER)

    def capabilities(self):
        """(position, id) of each capability in the list."""
        if not self.read(16, PCI_STATUS) & PCI_STATUS_CAPABILITIES:
            return []
        found = []
        position = self.read(8, PCI_CAPABILITY_LIST) & ~3
        while position:
            found.append((position, self.read(8, position)))
            position = self.read(8, position + 1) & ~3
        return found


class SplitQueue:
    """A split virtqueue in guest memory, one request/response chain at a time."""

    def __init__(self, qtest, memory, index, size):
        self.qtest = qtest
        self.index = index
        self.size = size
        self.desc = memory.allocate(16 * size)
        self.avail = memory.allocate(6 + 2 * size)
        self.used = memory.allocate(6 + 8 * size)
        self.memory = memory
        self.next_avail = 0
        self.next_descriptor = 0
        self.last_used = 0

    def submit(self, notify_address, request, response_size):
        """Places request (and a writable response buffer, if any) and notifies."""
        request_address = self.memory.allocate(len(request), 8)
        self.qtest.write_bytes(request_address, request)
        head = self.next_descriptor % self.size
        chain = [(request_address, len(request), 0)]
        response_address = None
        if response_size:
            response_address = self.memory.allocate(response_size, 8)
            chain.append((response_address, response_size, VIRTQ_DESC_F_WRITE))
        for position, (address, length, flags) in enumerate(chain):
            descriptor = (head + position) % self.size
            following = (head + position + 1) % self.size if position + 1 < len(chain) else 0
            more = VIRTQ_DESC_F_NEXT if position + 1 < len(chain) else 0
            self.qtest.write_bytes(self.desc + 16 * descriptor,
                                   struct.pack("<QIHH", address, length, flags | more, following))
        self.next_descriptor += len(chain)
        self.qtest.write(16, self.avail + 4 + 2 * (self.next_avail % self.size), head)
        self.next_avail += 1
        self.qtest.write(16, self.avail + 2, self.next_avail & 0xFFFF)
        self.qtest.write(16, notify_address, self.index)
        return response_address

    def wait_used(self, seconds=10.0):
        """The (descriptor id, written length) of the next used element."""
        deadline = time.monotonic() + seconds
        while self.qtest.read(16, self.used + 2) == self.last_used & 0xFFFF:
            if time.monotonic() > deadline:
                raise TimeoutError(f"queue {self.index}: no used element")
        element = self.qtest.read_bytes(self.used + 4 + 8 * (self.last_used % self.size), 8)
        self.last_used += 1
        return struct.unpack("<II", element)


class VirtioPciDevice:
    """A modern virtio-pci function: capability walk, reset, negotiation, queues,
    and shared memory regions by id as (guest address, length)."""

    def __init__(self, qtest):
        self.qtest = qtest
        self.memory = GuestMemory(qtest)
        self.pci = PciFunction(qtest)
        self.pci.assign_bars()
        self.regions = {}
        self.shared_memory = {}
        self.notify_multiplier = 0
        for position, capability in self.pci.capabilities():
            if capability != PCI_CAPABILITY_VENDOR:
                continue
            kind = self.pci.read(8, position + 3)
            bar = self.pci.read(8, position + 4)
            offset = self.pci.read(32, position + 8)
            if kind == VIRTIO_PCI_CAP_SHARED_MEMORY and bar in self.pci.bars:
                offset |= self.pci.read(32, position + 16) << 32
                length = self.pci.read(32, position + 12) | self.pci.read(32, position + 20) << 32
                self.shared_memory[self.pci.read(8, position + 5)] = (self.pci.bars[bar] + offset, length)
                continue
            if kind in (VIRTIO_PCI_CAP_COMMON, VIRTIO_PCI_CAP_NOTIFY, VIRTIO_PCI_CAP_ISR, VIRTIO_PCI_CAP_DEVICE) \
                    and kind not in self.regions and bar in self.pci.bars:
                self.regions[kind] = self.pci.bars[bar] + offset
                if kind == VIRTIO_PCI_CAP_NOTIFY:
                    self.notify_multiplier = self.pci.read(32, position + 16)
        self.queues = {}

    def common(self, width, offset, value=None):
        address = self.regions[VIRTIO_PCI_CAP_COMMON] + offset
        if value is None:
            return self.qtest.read(width, address)
        self.qtest.write(width, address, value)
        return value

    def device_config(self, width, offset):
        return self.qtest.read(width, self.regions[VIRTIO_PCI_CAP_DEVICE] + offset)

    def offered_features(self):
        features = 0
        for select in (0, 1):
            self.common(32, 0x00, select)
            features |= self.common(32, 0x04) << (32 * select)
        return features

    def initialize(self, wanted):
        """Reset, acknowledge, negotiate `wanted` & offered, and answer what was accepted."""
        self.common(8, 0x14, 0)
        self.common(8, 0x14, STATUS_ACKNOWLEDGE)
        self.common(8, 0x14, STATUS_ACKNOWLEDGE | STATUS_DRIVER)
        accepted = self.offered_features() & wanted
        for select in (0, 1):
            self.common(32, 0x08, select)
            self.common(32, 0x0C, accepted >> (32 * select) & 0xFFFFFFFF)
        self.common(8, 0x14, STATUS_ACKNOWLEDGE | STATUS_DRIVER | STATUS_FEATURES_OK)
        if not self.common(8, 0x14) & STATUS_FEATURES_OK:
            raise RuntimeError("device refused FEATURES_OK")
        return accepted

    def activate_queue(self, index):
        self.common(16, 0x16, index)
        size = self.common(16, 0x18)
        queue = SplitQueue(self.qtest, self.memory, index, size)
        self.common(64, 0x20, queue.desc)
        self.common(64, 0x28, queue.avail)
        self.common(64, 0x30, queue.used)
        notify_offset = self.common(16, 0x1E)
        self.common(16, 0x1C, 1)
        queue.notify_address = self.regions[VIRTIO_PCI_CAP_NOTIFY] + notify_offset * self.notify_multiplier
        self.queues[index] = queue
        return queue

    def start(self):
        self.common(8, 0x14, STATUS_ACKNOWLEDGE | STATUS_DRIVER | STATUS_FEATURES_OK | STATUS_DRIVER_OK)


class VirtioGpu:
    """virtio-gpu's control and cursor queues over a started VirtioPciDevice."""

    CONTROL = 0
    CURSOR = 1

    def __init__(self, qtest, wanted=FEATURE_VERSION_1 | FEATURE_EDID):
        self.device = VirtioPciDevice(qtest)
        self.features = self.device.initialize(wanted)
        self.control_queue = self.device.activate_queue(self.CONTROL)
        self.cursor_queue = self.device.activate_queue(self.CURSOR)
        self.device.start()
        self.qtest = qtest

    def control(self, request, response_size=24):
        """The response to one control request."""
        queue = self.control_queue
        response = queue.submit(queue.notify_address, request, response_size)
        _, length = queue.wait_used()
        return self.qtest.read_bytes(response, length)

    def cursor(self, request):
        """The length the device reports for one cursor request."""
        queue = self.cursor_queue
        queue.submit(queue.notify_address, request, 0)
        return queue.wait_used()[1]
