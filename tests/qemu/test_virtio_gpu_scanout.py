"""Blob scanout against the real device, in the configuration a qtest runs in:
no display backend at all.

That is not an accident of the harness, it is the case that must never regress.
A console with no display backend has no GL context - it is the backend that
calls qemu_console_set_display_gl_ctx, not the device - so this device offers no
dma-buf scanout, says so in its capset, and refuses every SET_SCANOUT_BLOB with
ERR_UNSPEC while answering every malformed one exactly as ScanoutFramebuffer
says. Guest-authored 2D transfers still work, but accelerated desktop images
are acknowledged without GPU export or CPU readback. WGC clients capture the
desktop independently of QEMU's display backend.

What a GL display does with the same commands needs one, and the guest suite is
where it is proven (T029).

Run by the qemu_virtio_gpu ctest in the qemu configuration, which sets
AV_QEMU_BUILD.
"""
import os
import struct
import unittest

import av_context
import virtio_qtest

BUILD = os.environ.get("AV_QEMU_BUILD", "")

CMD_RESOURCE_UNREF = 0x0102
CMD_RESOURCE_CREATE_2D = 0x0101
CMD_RESOURCE_ATTACH_BACKING = 0x0106
CMD_SCANOUT_IMAGE = 0x02F0
CMD_SET_SCANOUT = 0x0103
CMD_RESOURCE_FLUSH = 0x0104
CMD_RESOURCE_CREATE_BLOB = 0x010C
CMD_SET_SCANOUT_BLOB = 0x010D
CMD_GET_CAPSET = 0x0109
RESP_OK_NODATA = 0x1100
RESP_OK_CAPSET = 0x1103
RESP_ERR_UNSPEC = 0x1200
RESP_ERR_INVALID_RESOURCE_ID = 0x1203
RESP_ERR_INVALID_PARAMETER = 0x1205
BLOB_MEM_GUEST = 1
FEATURE_CONTEXT_INIT = 1 << 4
FEATURE_DMABUF_SCANOUT = 8
CAPSET_ID = 0x72
CAPSET_VERSION = 1
CAPSET_SIZE = 128
HOSTMEM = 64 << 20
PAGE = 0x1000
FORMAT_B8G8R8A8 = 1
WIDTH = 64
HEIGHT = 16
STRIDE = WIDTH * 4
FRAME = STRIDE * HEIGHT
MAX_HEADS = 16


def header(command):
    return struct.pack("<IIQI4x", command, 0, 0, 0)


def response_type(response):
    return struct.unpack_from("<I", response)[0]


def guest_blob(resource, address, size):
    return (header(CMD_RESOURCE_CREATE_BLOB)
            + struct.pack("<IIIIQQ", resource, BLOB_MEM_GUEST, 0, 1, 0, size)
            + struct.pack("<QI4x", address, size))


def scanout_blob(head, resource, x=0, y=0, rect_width=WIDTH, rect_height=HEIGHT, width=WIDTH, height=HEIGHT,
                 fmt=FORMAT_B8G8R8A8, padding=0, strides=(STRIDE, 0, 0, 0), offsets=(0, 0, 0, 0)):
    return (header(CMD_SET_SCANOUT_BLOB)
            + struct.pack("<4I", x, y, rect_width, rect_height)
            + struct.pack("<6I", head, resource, width, height, fmt, padding)
            + struct.pack("<4I", *strides) + struct.pack("<4I", *offsets))


def flush(resource, x=0, y=0, width=WIDTH, height=HEIGHT, padding=0):
    return header(CMD_RESOURCE_FLUSH) + struct.pack("<6I", x, y, width, height, resource, padding)


def named(command, resource):
    return header(command) + struct.pack("<I4x", resource)


@unittest.skipUnless(BUILD, "run by the qemu_virtio_gpu ctest")
class VirtioGpuScanout(unittest.TestCase):
    def open(self):
        qtest = virtio_qtest.Qtest(BUILD, av_context.av_device(f",hostmem={HOSTMEM}"))
        self.addCleanup(qtest.close)
        return virtio_qtest.VirtioGpu(qtest, virtio_qtest.FEATURE_VERSION_1 | virtio_qtest.FEATURE_EDID |
                                      FEATURE_CONTEXT_INIT)

    def expect(self, gpu, request, answer):
        self.assertEqual(hex(response_type(gpu.control(request, 24))), hex(answer), request[:48].hex())

    def blob(self, gpu, resource):
        address = gpu.device.memory.allocate(FRAME)
        self.expect(gpu, guest_blob(resource, address, FRAME), RESP_OK_NODATA)
        return address

    def test_a_device_with_no_display_backend_offers_no_dmabuf_scanout(self):
        gpu = self.open()
        response = gpu.control(header(CMD_GET_CAPSET) + struct.pack("<II", CAPSET_ID, CAPSET_VERSION),
                               24 + CAPSET_SIZE)
        self.assertEqual(hex(response_type(response)), hex(RESP_OK_CAPSET))
        features = struct.unpack_from("<Q", response, 24 + 24)[0]
        self.assertEqual(features & FEATURE_DMABUF_SCANOUT, 0)

    def test_accelerated_desktop_without_gl_never_requires_a_readable_image(self):
        gpu = self.open()
        self.expect(gpu, header(CMD_RESOURCE_CREATE_2D)
                    + struct.pack("<4I", 1, FORMAT_B8G8R8A8, WIDTH, HEIGHT), RESP_OK_NODATA)
        address = gpu.device.memory.allocate(FRAME)
        self.expect(gpu, header(CMD_RESOURCE_ATTACH_BACKING)
                    + struct.pack("<IIQI4x", 1, 1, address, FRAME), RESP_OK_NODATA)
        self.expect(gpu, header(CMD_SET_SCANOUT)
                    + struct.pack("<6I", 0, 0, WIDTH, HEIGHT, 0, 1), RESP_OK_NODATA)
        # The echo executor cannot export or read this key. A CPU-readback
        # fallback would fail here; headless presentation must still succeed.
        for frame in (0, 1, 2, 0xFFFFFFFF):
            self.expect(gpu, header(CMD_SCANOUT_IMAGE)
                        + struct.pack("<IIQ", 1, frame, 0xDEADBEEF), RESP_OK_NODATA)
        self.expect(gpu, flush(1), RESP_OK_NODATA)

    def test_accelerated_desktop_still_rejects_truncation_and_unknown_resources(self):
        gpu = self.open()
        request = header(CMD_SCANOUT_IMAGE) + struct.pack("<IIQ", 99, 0, 1)
        self.expect(gpu, request[:-1], RESP_ERR_INVALID_PARAMETER)
        self.expect(gpu, request, RESP_ERR_INVALID_RESOURCE_ID)
        self.blob(gpu, 1)
        self.expect(gpu, header(CMD_SCANOUT_IMAGE) + struct.pack("<IIQ", 1, 0, 1),
                    RESP_ERR_INVALID_RESOURCE_ID)

    def test_a_well_formed_scanout_is_the_hosts_refusal_and_never_the_guests(self):
        gpu = self.open()
        self.blob(gpu, 1)
        self.expect(gpu, scanout_blob(0, 1), RESP_ERR_UNSPEC)
        self.expect(gpu, scanout_blob(MAX_HEADS - 1, 1), RESP_ERR_UNSPEC)
        # Refused means nothing was taken: the guest may still release the blob.
        self.expect(gpu, named(CMD_RESOURCE_UNREF, 1), RESP_OK_NODATA)

    def test_hiding_a_head_needs_no_display_and_is_never_an_error(self):
        gpu = self.open()
        disable = scanout_blob(0, 0, rect_width=0, rect_height=0, width=0, height=0, fmt=0,
                               strides=(0, 0, 0, 0))
        self.expect(gpu, disable, RESP_OK_NODATA)
        self.expect(gpu, disable, RESP_OK_NODATA)

    def test_every_structural_refusal_of_the_record(self):
        gpu = self.open()
        self.blob(gpu, 1)
        for request in (scanout_blob(0, 1)[:95],
                        scanout_blob(0, 1, padding=1),
                        scanout_blob(0, 1, strides=(STRIDE, STRIDE, 0, 0)),
                        scanout_blob(0, 1, offsets=(0, PAGE, 0, 0)),
                        scanout_blob(MAX_HEADS, 1),
                        scanout_blob(0xFFFFFFFF, 1)):
            self.expect(gpu, request, RESP_ERR_INVALID_PARAMETER)

    def test_every_framebuffer_that_does_not_fit_the_blob(self):
        gpu = self.open()
        self.blob(gpu, 1)
        for request in (scanout_blob(0, 1, fmt=0),
                        scanout_blob(0, 1, fmt=0xFFFFFFFF),
                        scanout_blob(0, 1, width=0),
                        scanout_blob(0, 1, height=0),
                        scanout_blob(0, 1, strides=(STRIDE - 1, 0, 0, 0)),
                        scanout_blob(0, 1, strides=(0x80000000, 0, 0, 0)),
                        scanout_blob(0, 1, offsets=(4, 0, 0, 0)),
                        scanout_blob(0, 1, height=HEIGHT + 1),
                        scanout_blob(0, 1, x=1),
                        scanout_blob(0, 1, rect_width=0)):
            self.expect(gpu, request, RESP_ERR_INVALID_PARAMETER)

    def test_a_resource_that_is_not_a_blob_is_an_unknown_resource(self):
        gpu = self.open()
        self.expect(gpu, scanout_blob(0, 9), RESP_ERR_INVALID_RESOURCE_ID)

    def test_a_flush_and_a_set_scanout_the_core_does_not_own_reach_the_base_class(self):
        gpu = self.open()
        self.blob(gpu, 1)
        # A blob no head shows: the core's, and refused.
        self.expect(gpu, flush(1), RESP_ERR_INVALID_PARAMETER)
        self.expect(gpu, flush(1, padding=1), RESP_ERR_INVALID_RESOURCE_ID)
        # A resource neither the core nor the base class holds: the base
        # class's answer, unchanged by this feature.
        self.expect(gpu, flush(9), RESP_ERR_INVALID_RESOURCE_ID)
        # SET_SCANOUT naming a blob is deliberately not intercepted; the base
        # class already answers for a resource it does not hold.
        self.expect(gpu, header(CMD_SET_SCANOUT) + struct.pack("<6I", 0, 0, WIDTH, HEIGHT, 0, 1),
                    RESP_ERR_INVALID_RESOURCE_ID)
