"""The virtio-gpu 2D protocol the driver talks to, against the real device: the
patched QEMU's stock virtio-gpu-pci and virtio-gpu-av-pci must answer alike,
because the av device leaves the 2D protocol to its base class. Requests the
driver sends are the driver's own bytes (gpu_request_bytes); the display
requests it will send next are laid out here from the virtio 1.2 structures.

Run by the qemu_virtio_gpu ctest in the qemu configuration, which sets
AV_QEMU_BUILD and AV_GPU_REQUEST_BYTES.
"""
import os
import struct
import subprocess
import unittest

import av_context
import virtio_qtest

BUILD = os.environ.get("AV_QEMU_BUILD", "")
REQUEST_BYTES = os.environ.get("AV_GPU_REQUEST_BYTES", "")

CMD_GET_DISPLAY_INFO = 0x0100
CMD_RESOURCE_CREATE_2D = 0x0101
CMD_RESOURCE_UNREF = 0x0102
CMD_SET_SCANOUT = 0x0103
CMD_RESOURCE_FLUSH = 0x0104
CMD_GET_EDID = 0x010A
RESP_OK_NODATA = 0x1100
RESP_OK_DISPLAY_INFO = 0x1101
RESP_OK_EDID = 0x1104
RESP_ERR_UNSPEC = 0x1200
RESP_ERR_INVALID_SCANOUT_ID = 0x1202
RESP_ERR_INVALID_RESOURCE_ID = 0x1203
FORMAT_B8G8R8A8 = 1
MAX_SCANOUTS = 16
FEATURE_VIRGL = 1 << 0
FEATURE_RESOURCE_BLOB = 1 << 3
FEATURE_CONTEXT_INIT = 1 << 4

DEVICES = {"stock": "virtio-gpu-pci", "av": "virtio-gpu-av-pci,host-gpu=auto",
           "av with an executor": av_context.av_device()}


def header(command):
    return struct.pack("<IIQI4x", command, 0, 0, 0)


def response_type(response):
    return struct.unpack_from("<I", response)[0]


def driver_request(*arguments):
    """The bytes avgpu.sys's GpuRequestWriter writes for a request."""
    result = subprocess.run([REQUEST_BYTES, *map(str, arguments)], capture_output=True, text=True, check=True)
    return bytes.fromhex(result.stdout.strip())


@unittest.skipUnless(BUILD and REQUEST_BYTES, "run by the qemu_virtio_gpu ctest")
class VirtioGpu2d(unittest.TestCase):
    def open(self, name):
        qtest = virtio_qtest.Qtest(BUILD, DEVICES[name])
        self.addCleanup(qtest.close)
        return virtio_qtest.VirtioGpu(qtest)

    def each_device(self):
        for name in DEVICES:
            with self.subTest(device=name):
                yield name, self.open(name)

    def test_the_av_device_offers_what_stock_offers_and_context_init_only_with_an_executor(self):
        stock = self.open("stock").device.offered_features()
        av = self.open("av").device.offered_features()
        executing = self.open("av with an executor").device.offered_features()
        self.assertEqual(av, stock)
        self.assertEqual(executing, stock | FEATURE_CONTEXT_INIT)
        self.assertTrue(av & virtio_qtest.FEATURE_EDID)
        self.assertTrue(av & virtio_qtest.FEATURE_VERSION_1)
        self.assertFalse(stock & FEATURE_CONTEXT_INIT)
        self.assertFalse(executing & (FEATURE_VIRGL | FEATURE_RESOURCE_BLOB))

    def test_one_output_at_the_drivers_preferred_mode(self):
        for _, gpu in self.each_device():
            response = gpu.control(header(CMD_GET_DISPLAY_INFO), 24 + 24 * MAX_SCANOUTS)
            self.assertEqual(response_type(response), RESP_OK_DISPLAY_INFO)
            modes = [struct.unpack_from("<IIIIII", response, 24 + 24 * index) for index in range(MAX_SCANOUTS)]
            self.assertEqual(modes[0], (0, 0, 1280, 800, 1, 0))
            self.assertTrue(all(mode[4] == 0 for mode in modes[1:]))

    def test_the_edid_is_a_valid_base_block(self):
        for _, gpu in self.each_device():
            response = gpu.control(header(CMD_GET_EDID) + struct.pack("<II", 0, 0), 32 + 1024)
            self.assertEqual(response_type(response), RESP_OK_EDID)
            size = struct.unpack_from("<I", response, 24)[0]
            block = response[32:32 + 128]
            self.assertGreaterEqual(size, 128)
            self.assertEqual(block[:8], bytes.fromhex("00ffffffffffff00"))
            self.assertEqual(sum(block) % 256, 0)

    def test_the_drivers_cursor_requests_are_accepted(self):
        for _, gpu in self.each_device():
            backing = gpu.device.memory.allocate(64 * 64 * 4)
            gpu.qtest.write_bytes(backing, bytes([0x40, 0x80, 0xC0, 0xFF]) * (64 * 64))
            for request in (driver_request("create_2d", 1, FORMAT_B8G8R8A8, 64, 64),
                            driver_request("attach_backing", 1, backing, 64 * 64 * 4),
                            driver_request("transfer_to_host_2d", 1, 64, 64)):
                self.assertEqual(response_type(gpu.control(request)), RESP_OK_NODATA)
            self.assertEqual(gpu.cursor(driver_request("update_cursor", 0, 100, 100, 1, 3, 4)), 0)
            self.assertEqual(gpu.cursor(driver_request("move_cursor", 0, 200, 50)), 0)
            self.assertEqual(gpu.cursor(driver_request("update_cursor", 0, 0, 0, 0, 0, 0)), 0)
            self.assertEqual(response_type(gpu.control(header(CMD_GET_DISPLAY_INFO), 24 + 24 * MAX_SCANOUTS)),
                             RESP_OK_DISPLAY_INFO)

    def test_a_primary_scans_out_and_flushes(self):
        for _, gpu in self.each_device():
            size = 1280 * 800 * 4
            backing = gpu.device.memory.allocate(size)
            self.assertEqual(response_type(gpu.control(driver_request("create_2d", 2, FORMAT_B8G8R8A8, 1280, 800))),
                             RESP_OK_NODATA)
            self.assertEqual(response_type(gpu.control(driver_request("attach_backing", 2, backing, size))),
                             RESP_OK_NODATA)
            rectangle = struct.pack("<IIII", 0, 0, 1280, 800)
            self.assertEqual(response_type(gpu.control(driver_request("transfer_to_host_2d", 2, 1280, 800))),
                             RESP_OK_NODATA)
            scanout = header(CMD_SET_SCANOUT) + rectangle + struct.pack("<II", 0, 2)
            self.assertEqual(response_type(gpu.control(scanout)), RESP_OK_NODATA)
            flush = header(CMD_RESOURCE_FLUSH) + rectangle + struct.pack("<II", 2, 0)
            self.assertEqual(response_type(gpu.control(flush)), RESP_OK_NODATA)
            disable = header(CMD_SET_SCANOUT) + struct.pack("<IIII", 0, 0, 0, 0) + struct.pack("<II", 0, 0)
            self.assertEqual(response_type(gpu.control(disable)), RESP_OK_NODATA)
            unref = header(CMD_RESOURCE_UNREF) + struct.pack("<II", 2, 0)
            self.assertEqual(response_type(gpu.control(unref)), RESP_OK_NODATA)

    def test_requests_the_driver_must_never_send_are_refused_with_the_documented_errors(self):
        for _, gpu in self.each_device():
            self.assertEqual(response_type(gpu.control(driver_request("create_2d", 3, FORMAT_B8G8R8A8, 8, 8))),
                             RESP_OK_NODATA)
            refused = (
                (driver_request("create_2d", 3, FORMAT_B8G8R8A8, 8, 8), RESP_ERR_INVALID_RESOURCE_ID),
                (driver_request("create_2d", 0, FORMAT_B8G8R8A8, 8, 8), RESP_ERR_INVALID_RESOURCE_ID),
                (header(CMD_SET_SCANOUT) + struct.pack("<IIIIII", 0, 0, 8, 8, 1, 3), RESP_ERR_INVALID_SCANOUT_ID),
                (header(CMD_RESOURCE_FLUSH) + struct.pack("<IIIIII", 0, 0, 8, 8, 99, 0), RESP_ERR_INVALID_RESOURCE_ID),
                (header(CMD_RESOURCE_UNREF) + struct.pack("<II", 99, 0), RESP_ERR_INVALID_RESOURCE_ID),
                (header(0x01FF), RESP_ERR_UNSPEC),
            )
            for request, error in refused:
                self.assertEqual(hex(response_type(gpu.control(request))), hex(error), request[:4].hex())


if __name__ == "__main__":
    unittest.main()
