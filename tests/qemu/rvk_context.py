"""The rvk requests a guest driver sends to set a context up, and the guest's
side of its ring (docs/protocol.md), for the qtest suites."""
import os
import struct
import time

# The echo executor (tests/qemu/echo_executor.cpp) the qemu_virtio_gpu ctest builds.
EXECUTOR = os.environ.get("AV_EXECUTOR", "")
# This project's executor library, the one that answers on the host's GPU.
REAL_EXECUTOR = os.environ.get("AV_REAL_EXECUTOR", "")

CMD_RESOURCE_UNREF = 0x0102
CMD_RESOURCE_CREATE_BLOB = 0x010C
CMD_RESOURCE_MAP_BLOB = 0x0208
CMD_CTX_CREATE = 0x0200
CMD_CTX_DESTROY = 0x0201
CMD_SUBMIT_3D = 0x0207
RESP_OK_NODATA = 0x1100
RESP_OK_MAP_INFO = 0x1106
RESP_ERR_INVALID_RESOURCE_ID = 0x1203
RESP_ERR_INVALID_CONTEXT_ID = 0x1204
RESP_ERR_INVALID_PARAMETER = 0x1205
FEATURE_CONTEXT_INIT = 1 << 4
BLOB_MEM_GUEST = 1
BLOB_MEM_HOST3D = 2
BLOB_FLAG_MAPPABLE = 1

CAPSET_ID = 0x72
SETUP_MAGIC = 0x434B5652
RING_MAGIC = 0x524B5652
BATCH_MAGIC = 0x424B5652
SUB_API_VULKAN = 1
DOORBELL_SHM_ID = 0x72
DOORBELLS = 64
PAGE = 0x1000
RING_HEADER = 32
TAIL_OFFSET = 8
HEAD_OFFSET = 16


def rvk_device(options="", executor=EXECUTOR):
    """virtio-gpu-rvk-pci on the first host GPU, executing with the echo executor unless told another."""
    return f"virtio-gpu-rvk-pci,host-gpu=auto,executor={executor}{options}"


def header(command, context=0):
    return struct.pack("<IIQI4x", command, 0, 0, context)


def response_type(response):
    return struct.unpack_from("<I", response)[0]


def create_context(context, context_init=CAPSET_ID, name=b"vulkan"):
    padded = name.ljust(64, b"\0")[:64]
    return header(CMD_CTX_CREATE, context) + struct.pack("<II", len(name), context_init) + padded


def destroy_context(context):
    return header(CMD_CTX_DESTROY, context)


def setup(ring, fence, order=12, doorbell=0, major=1, minor=0, sub_api=SUB_API_VULKAN):
    return struct.pack("<IHHIIB3xIII", SETUP_MAGIC, major, minor, sub_api, ring, order, fence, doorbell, 0)


def submit(context, command, size=None):
    return header(CMD_SUBMIT_3D, context) + struct.pack("<I4x", len(command) if size is None else size) + command


def host_blob(resource, size):
    return header(CMD_RESOURCE_CREATE_BLOB) + struct.pack("<IIIIQQ", resource, BLOB_MEM_HOST3D,
                                                          BLOB_FLAG_MAPPABLE, 0, 0, size)


def guest_blob(resource, address, size):
    return header(CMD_RESOURCE_CREATE_BLOB) + struct.pack("<IIIIQQ", resource, BLOB_MEM_GUEST, 0, 1, 0, size) + \
        struct.pack("<QI4x", address, size)


def guest_blob_pages(resource, addresses):
    """A GUEST blob over one page at each of `addresses`, in that order."""
    return header(CMD_RESOURCE_CREATE_BLOB) + struct.pack("<IIIIQQ", resource, BLOB_MEM_GUEST, 0, len(addresses), 0,
                                                          PAGE * len(addresses)) + \
        b"".join(struct.pack("<QI4x", address, PAGE) for address in addresses)


def map_blob(resource, offset):
    return header(CMD_RESOURCE_MAP_BLOB) + struct.pack("<I4xQ", resource, offset)


def unref(resource):
    return header(CMD_RESOURCE_UNREF) + struct.pack("<I4x", resource)


def ring_header(order, tail=0, head=0):
    return struct.pack("<IB3xQQQ", RING_MAGIC, order, tail, head, 0)


NOP = 0x00000001
IMPORT_BLOB = 0x00000002
RELEASE_BLOB = 0x00000003
ECHO = 0x10001
UNDECODABLE = 0x10002
LOSE_DEVICE = 0x10003
COUNT = 0x10004
PEEK = 0x10005
POKE = 0x10006
GENERATE_REPLY = 1


def command(command_id, payload=b"", flags=0):
    """One command as the echo executor reads it: id, size with flags in the top byte, payload."""
    return struct.pack("<II", command_id, (8 + len(payload)) | flags << 24) + payload


def batch_of(fence, commands):
    body = struct.pack("<IIQ", BATCH_MAGIC, len(commands), fence) + commands
    return body + bytes(-len(body) % 8)


def batch(fence, length):
    """A batch of `length` payload bytes: one echo command when it can hold one."""
    if length < 8:
        return batch_of(fence, bytes((0xB0 + i) & 0xFF for i in range(length)))
    return batch_of(fence, command(ECHO, bytes((0xB0 + i) & 0xFF for i in range(length - 8))))


class GuestRing:
    """A ring in guest memory at `address`: the guest writes batches and
    publishes its tail; the device publishes head."""

    def __init__(self, qtest, address, order=12):
        self.qtest = qtest
        self.address = address
        self.order = order
        self.tail = 0
        qtest.write_bytes(address, ring_header(order))

    @property
    def capacity(self):
        return 1 << self.order

    @property
    def blob_size(self):
        return (RING_HEADER + self.capacity + PAGE - 1) & ~(PAGE - 1)

    def write(self, data):
        """Bytes at the unpublished tail, wrapping at the end of the data area."""
        position = self.tail % self.capacity
        first = data[:self.capacity - position]
        self.qtest.write_bytes(self.address + RING_HEADER + position, first)
        if len(data) > len(first):
            self.qtest.write_bytes(self.address + RING_HEADER, data[len(first):])
        self.tail += len(data)

    def write_batch(self, fence, length):
        self.write(batch(fence, length))

    def publish(self, tail=None):
        self.qtest.write(64, self.address + TAIL_OFFSET, self.tail if tail is None else tail)

    def head(self):
        return self.qtest.read(64, self.address + HEAD_OFFSET)

    def wait_head(self, expected, seconds=10.0):
        """Whether head reached `expected` in time."""
        deadline = time.monotonic() + seconds
        while self.head() != expected:
            if time.monotonic() > deadline:
                return False
            time.sleep(0.001)
        return True
