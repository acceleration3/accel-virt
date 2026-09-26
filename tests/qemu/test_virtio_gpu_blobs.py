"""Blob resources against the real device: guest-backed blobs held by the
device, and device-allocated blobs the guest reads and writes through the
hostmem BAR once mapped. Stock virtio-gpu-pci with blob=off refuses every blob
request; virtio-gpu-rvk-pci answers them itself without offering
VIRTIO_GPU_F_RESOURCE_BLOB.

Run by the qemu_virtio_gpu ctest in the qemu configuration, which sets
AV_QEMU_BUILD.
"""
import os
import struct
import unittest

import virtio_qtest

BUILD = os.environ.get("AV_QEMU_BUILD", "")

CMD_RESOURCE_CREATE_2D = 0x0101
CMD_RESOURCE_UNREF = 0x0102
CMD_RESOURCE_CREATE_BLOB = 0x010C
CMD_SET_SCANOUT_BLOB = 0x010D
CMD_RESOURCE_MAP_BLOB = 0x0208
CMD_RESOURCE_UNMAP_BLOB = 0x0209
RESP_OK_NODATA = 0x1100
RESP_OK_MAP_INFO = 0x1106
RESP_ERR_UNSPEC = 0x1200
RESP_ERR_INVALID_RESOURCE_ID = 0x1203
RESP_ERR_INVALID_PARAMETER = 0x1205
BLOB_MEM_GUEST = 1
BLOB_MEM_HOST3D = 2
BLOB_FLAG_MAPPABLE = 1
MAP_CACHE_CACHED = 1
SHM_ID_HOST_VISIBLE = 1
FEATURE_CONTEXT_INIT = 1 << 4
HOSTMEM = 64 << 20
PAGE = 0x1000
UNASSIGNED = 0xD0000000

STOCK = f"virtio-gpu-pci,hostmem={HOSTMEM}"
RVK = f"virtio-gpu-rvk-pci,host-gpu=auto,hostmem={HOSTMEM}"


def header(command):
    return struct.pack("<IIQI4x", command, 0, 0, 0)


def response_type(response):
    return struct.unpack_from("<I", response)[0]


def create_blob(resource, memory, flags, size, entries=(), blob_id=0):
    request = header(CMD_RESOURCE_CREATE_BLOB) + struct.pack("<IIIIQQ", resource, memory, flags, len(entries),
                                                             blob_id, size)
    return request + b"".join(struct.pack("<QI4x", address, length) for address, length in entries)


def map_blob(resource, offset):
    return header(CMD_RESOURCE_MAP_BLOB) + struct.pack("<I4xQ", resource, offset)


def named(command, resource):
    return header(command) + struct.pack("<I4x", resource)


@unittest.skipUnless(BUILD, "run by the qemu_virtio_gpu ctest")
class VirtioGpuBlobs(unittest.TestCase):
    def open(self, device):
        qtest = virtio_qtest.Qtest(BUILD, device)
        self.addCleanup(qtest.close)
        return virtio_qtest.VirtioGpu(qtest)

    def send(self, gpu, request, response_size=24):
        return response_type(gpu.control(request, response_size))

    def hostmem(self, gpu):
        address, length = gpu.device.shared_memory[SHM_ID_HOST_VISIBLE]
        self.assertEqual(length, HOSTMEM)
        return address

    def test_the_rvk_device_offers_the_same_hostmem_region_and_no_blob_feature(self):
        stock = self.open(STOCK)
        rvk = self.open(RVK)
        self.assertEqual(rvk.device.offered_features(), stock.device.offered_features())
        self.assertEqual(rvk.device.shared_memory[SHM_ID_HOST_VISIBLE][1], HOSTMEM)
        self.assertEqual(stock.device.shared_memory[SHM_ID_HOST_VISIBLE][1], HOSTMEM)

    def test_stock_virtio_gpu_without_blobs_refuses_every_blob_request(self):
        gpu = self.open(STOCK)
        for request in (create_blob(1, BLOB_MEM_HOST3D, BLOB_FLAG_MAPPABLE, PAGE),
                        header(CMD_SET_SCANOUT_BLOB) + bytes(48)):
            self.assertEqual(hex(self.send(gpu, request)), hex(RESP_ERR_INVALID_PARAMETER))
        for request in (map_blob(1, 0), named(CMD_RESOURCE_UNMAP_BLOB, 1)):
            self.assertEqual(hex(self.send(gpu, request)), hex(RESP_ERR_UNSPEC))

    def test_a_guest_blob_is_held_until_unreferenced(self):
        gpu = self.open(RVK)
        pages = gpu.device.memory.allocate(3 * PAGE)
        entries = ((pages, PAGE), (pages + PAGE, 2 * PAGE))
        self.assertEqual(hex(self.send(gpu, create_blob(1, BLOB_MEM_GUEST, 0, 3 * PAGE, entries))),
                         hex(RESP_OK_NODATA))
        self.assertEqual(hex(self.send(gpu, create_blob(1, BLOB_MEM_GUEST, 0, 3 * PAGE, entries))),
                         hex(RESP_ERR_INVALID_RESOURCE_ID))
        self.assertEqual(hex(self.send(gpu, map_blob(1, 0))), hex(RESP_ERR_INVALID_PARAMETER))
        self.assertEqual(hex(self.send(gpu, named(CMD_RESOURCE_UNREF, 1))), hex(RESP_OK_NODATA))
        self.assertEqual(hex(self.send(gpu, named(CMD_RESOURCE_UNREF, 1))), hex(RESP_ERR_INVALID_RESOURCE_ID))

    def test_a_host_blob_is_read_and_written_through_the_hostmem_bar_once_mapped(self):
        gpu = self.open(RVK)
        hostmem = self.hostmem(gpu)
        pattern = bytes(range(256)) * 8
        self.assertEqual(hex(self.send(gpu, create_blob(2, BLOB_MEM_HOST3D, BLOB_FLAG_MAPPABLE, 2 * PAGE))),
                         hex(RESP_OK_NODATA))
        response = gpu.control(map_blob(2, 4 * PAGE), 32)
        self.assertEqual(hex(response_type(response)), hex(RESP_OK_MAP_INFO))
        self.assertEqual(struct.unpack_from("<I", response, 24)[0], MAP_CACHE_CACHED)
        self.assertEqual(gpu.qtest.read_bytes(hostmem + 4 * PAGE, 2 * PAGE), bytes(2 * PAGE))
        gpu.qtest.write_bytes(hostmem + 5 * PAGE, pattern)
        self.assertEqual(gpu.qtest.read_bytes(hostmem + 5 * PAGE, len(pattern)), pattern)
        self.assertEqual(hex(self.send(gpu, named(CMD_RESOURCE_UNMAP_BLOB, 2))), hex(RESP_OK_NODATA))
        self.assertEqual(gpu.qtest.read_bytes(hostmem + 5 * PAGE, len(pattern)), bytes(len(pattern)))
        self.assertEqual(hex(self.send(gpu, named(CMD_RESOURCE_UNMAP_BLOB, 2))), hex(RESP_ERR_INVALID_PARAMETER))
        self.assertEqual(hex(response_type(gpu.control(map_blob(2, 10 * PAGE), 32))), hex(RESP_OK_MAP_INFO))
        self.assertEqual(gpu.qtest.read_bytes(hostmem + 11 * PAGE, len(pattern)), pattern)
        self.assertEqual(hex(self.send(gpu, named(CMD_RESOURCE_UNREF, 2))), hex(RESP_OK_NODATA))
        self.assertEqual(gpu.qtest.read_bytes(hostmem + 11 * PAGE, len(pattern)), bytes(len(pattern)))

    def test_a_blob_as_large_as_the_window_is_placed_at_its_start(self):
        """The largest chunk the driver's host window can hand out
        (docs/host-window.md) is the whole BAR, placed at offset 0."""
        gpu = self.open(RVK)
        hostmem = self.hostmem(gpu)
        resource = 0x1000
        self.assertEqual(hex(self.send(gpu, create_blob(resource, BLOB_MEM_HOST3D, BLOB_FLAG_MAPPABLE, HOSTMEM))),
                         hex(RESP_OK_NODATA))
        response = gpu.control(map_blob(resource, 0), 32)
        self.assertEqual(hex(response_type(response)), hex(RESP_OK_MAP_INFO))
        self.assertEqual(struct.unpack_from("<I", response, 24)[0], MAP_CACHE_CACHED)
        # Every offset VidMm could place an allocation at is the host's memory:
        # the first page, the last, and one in the middle.
        pattern = bytes(range(256))
        for offset in (0, HOSTMEM // 2, HOSTMEM - len(pattern)):
            gpu.qtest.write_bytes(hostmem + offset, pattern)
            self.assertEqual(gpu.qtest.read_bytes(hostmem + offset, len(pattern)), pattern)
        # Nothing else fits beside it, which is what one blob for the whole
        # region means.
        self.assertEqual(hex(self.send(gpu, create_blob(resource + 1, BLOB_MEM_HOST3D, BLOB_FLAG_MAPPABLE, PAGE))),
                         hex(RESP_OK_NODATA))
        self.assertEqual(hex(self.send(gpu, map_blob(resource + 1, 0))), hex(RESP_ERR_INVALID_PARAMETER))
        self.assertEqual(hex(self.send(gpu, map_blob(resource + 1, HOSTMEM - PAGE))),
                         hex(RESP_ERR_INVALID_PARAMETER))
        # And the driver's own teardown order - unmap, then unref - is what the
        # device accepts.
        self.assertEqual(hex(self.send(gpu, named(CMD_RESOURCE_UNMAP_BLOB, resource))), hex(RESP_OK_NODATA))
        self.assertEqual(hex(self.send(gpu, named(CMD_RESOURCE_UNREF, resource))), hex(RESP_OK_NODATA))
        self.assertEqual(gpu.qtest.read_bytes(hostmem, len(pattern)), bytes(len(pattern)))

    def test_mappings_stay_inside_the_region_and_apart(self):
        gpu = self.open(RVK)
        for resource in (3, 4):
            self.assertEqual(hex(self.send(gpu, create_blob(resource, BLOB_MEM_HOST3D, BLOB_FLAG_MAPPABLE,
                                                            2 * PAGE))), hex(RESP_OK_NODATA))
        self.assertEqual(hex(response_type(gpu.control(map_blob(3, 0), 32))), hex(RESP_OK_MAP_INFO))
        refused = (
            (map_blob(3, 8 * PAGE), RESP_ERR_INVALID_PARAMETER),           # already mapped
            (map_blob(4, PAGE), RESP_ERR_INVALID_PARAMETER),               # overlaps
            (map_blob(4, 2 * PAGE + 1), RESP_ERR_INVALID_PARAMETER),       # misaligned
            (map_blob(4, HOSTMEM - PAGE), RESP_ERR_INVALID_PARAMETER),     # past the region
            (map_blob(9, 8 * PAGE), RESP_ERR_INVALID_RESOURCE_ID),
        )
        for request, error in refused:
            self.assertEqual(hex(response_type(gpu.control(request, 32))), hex(error))
        self.assertEqual(hex(response_type(gpu.control(map_blob(4, 2 * PAGE), 32))), hex(RESP_OK_MAP_INFO))

    def test_requests_the_protocol_does_not_allow_are_refused_with_the_documented_errors(self):
        gpu = self.open(RVK)
        pages = gpu.device.memory.allocate(PAGE)
        self.assertEqual(hex(self.send(gpu, create_blob(5, BLOB_MEM_HOST3D, BLOB_FLAG_MAPPABLE, PAGE))),
                         hex(RESP_OK_NODATA))
        refused = (
            (create_blob(0, BLOB_MEM_HOST3D, BLOB_FLAG_MAPPABLE, PAGE), RESP_ERR_INVALID_RESOURCE_ID),
            (create_blob(5, BLOB_MEM_HOST3D, BLOB_FLAG_MAPPABLE, PAGE), RESP_ERR_INVALID_RESOURCE_ID),
            (create_blob(6, BLOB_MEM_HOST3D, BLOB_FLAG_MAPPABLE, PAGE, blob_id=7), RESP_ERR_INVALID_PARAMETER),
            (create_blob(6, BLOB_MEM_HOST3D, 0, PAGE), RESP_ERR_INVALID_PARAMETER),
            (create_blob(6, BLOB_MEM_HOST3D, BLOB_FLAG_MAPPABLE, PAGE + 1), RESP_ERR_INVALID_PARAMETER),
            (create_blob(6, 3, BLOB_FLAG_MAPPABLE, PAGE), RESP_ERR_INVALID_PARAMETER),
            (create_blob(6, BLOB_MEM_GUEST, 0, 2 * PAGE, ((pages, PAGE),)), RESP_ERR_INVALID_PARAMETER),
            (create_blob(6, BLOB_MEM_GUEST, 0, PAGE, ((UNASSIGNED, PAGE),)), RESP_ERR_UNSPEC),
            (create_blob(6, BLOB_MEM_GUEST, 0, PAGE)[:40], RESP_ERR_INVALID_PARAMETER),
            (header(CMD_RESOURCE_CREATE_2D) + struct.pack("<IIII", 5, 1, 8, 8), RESP_ERR_INVALID_RESOURCE_ID),
            (header(CMD_SET_SCANOUT_BLOB) + bytes(48), RESP_ERR_INVALID_PARAMETER),
        )
        for request, error in refused:
            self.assertEqual(hex(self.send(gpu, request)), hex(error), request[:48].hex())
        self.assertEqual(hex(self.send(gpu, header(CMD_RESOURCE_CREATE_2D) + struct.pack("<IIII", 6, 1, 8, 8))),
                         hex(RESP_OK_NODATA))
        self.assertEqual(hex(self.send(gpu, create_blob(6, BLOB_MEM_HOST3D, BLOB_FLAG_MAPPABLE, PAGE))),
                         hex(RESP_ERR_INVALID_RESOURCE_ID))

    def test_a_reset_releases_every_blob_and_mapping(self):
        gpu = self.open(RVK)
        hostmem = self.hostmem(gpu)
        self.assertEqual(hex(self.send(gpu, create_blob(7, BLOB_MEM_HOST3D, BLOB_FLAG_MAPPABLE, PAGE))),
                         hex(RESP_OK_NODATA))
        self.assertEqual(hex(response_type(gpu.control(map_blob(7, 0), 32))), hex(RESP_OK_MAP_INFO))
        gpu.qtest.write_bytes(hostmem, b"\xAA" * 16)
        again = virtio_qtest.VirtioGpu(gpu.qtest)   # a new driver resets the device first
        self.assertEqual(gpu.qtest.read_bytes(hostmem, 16), bytes(16))
        self.assertEqual(hex(self.send(again, create_blob(7, BLOB_MEM_HOST3D, BLOB_FLAG_MAPPABLE, PAGE))),
                         hex(RESP_OK_NODATA))
        self.assertEqual(hex(response_type(again.control(map_blob(7, 0), 32))), hex(RESP_OK_MAP_INFO))

    def test_blob_on_is_refused_at_realize(self):
        qtest = virtio_qtest.Qtest(BUILD, "virtio-gpu-rvk-pci,host-gpu=auto,blob=on")
        with self.assertRaises(RuntimeError) as refused:
            qtest.command("readb 0x0")
        qtest.close()
        self.assertIn("virtio-gpu-rvk: blob=on is not supported; the device answers blob requests itself",
                      str(refused.exception))


if __name__ == "__main__":
    unittest.main()
