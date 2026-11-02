import os
import struct
import time
import unittest

import virtio_qtest
from av_context import FEATURE_CONTEXT_INIT, PAGE, RESP_OK_NODATA, response_type, av_device

BUILD = os.environ.get("AV_QEMU_BUILD", "")
WANTED = virtio_qtest.FEATURE_VERSION_1 | virtio_qtest.FEATURE_EDID | FEATURE_CONTEXT_INIT
PRESENT_MAGIC = 0x00505641
PRESENT_ENGINE = 0x02F6
QUEUED_PRESENT = 0x02F7
PRESENT_QUEUE = 67
RESP_ERR_INVALID_PARAMETER = 0x1205
BLT = 1
WIDTH = 16
ROWS = 64
PITCH = WIDTH * 4


def wait_for(condition, seconds=10.0):
    deadline = time.monotonic() + seconds
    while not condition():
        if time.monotonic() > deadline:
            return False
        time.sleep(0.001)
    return True


def request(kind, body):
    return struct.pack("<IIQII", kind, 0, 0, 0, 0) + body


def blt(ticket, source, destination):
    fixed = struct.pack("<QIIQIIIIIIII", ticket, 0, 0, 0, BLT, 0, 0, 1, 1, 1, PITCH, PITCH)
    entries = struct.pack("<QII", source, PAGE, 0) + struct.pack("<QII", destination, PAGE, 0)
    return request(QUEUED_PRESENT, fixed + entries + struct.pack("<6I", 0, 0, 0, 0, WIDTH, ROWS))


@unittest.skipUnless(BUILD, "run by the qemu_virtio_gpu ctest")
class VirtioGpuPresentEngine(unittest.TestCase):
    def qtest(self):
        qtest = virtio_qtest.Qtest(BUILD, av_device(",vectors=4"))
        self.addCleanup(qtest.close)
        return qtest

    def registered(self, gpu):
        page = gpu.device.memory.allocate(PAGE)
        answer = gpu.control(request(PRESENT_ENGINE, struct.pack("<Q", page)))
        self.assertEqual(hex(response_type(answer)), hex(RESP_OK_NODATA))
        return page

    @staticmethod
    def progress(gpu, page):
        return struct.unpack("<QQ", gpu.qtest.read_bytes(page + 8, 16))

    def surfaces(self, gpu):
        source = gpu.device.memory.allocate(PAGE)
        destination = gpu.device.memory.allocate(PAGE)
        pixels = bytes((at * 7 + 3) & 0xFF for at in range(PAGE))
        gpu.qtest.write_bytes(source, pixels)
        gpu.qtest.write_bytes(destination, bytes(PAGE))
        return source, destination, pixels

    def test_a_queued_blt_is_answered_at_once_and_moved_by_its_ticket(self):
        gpu = virtio_qtest.VirtioGpu(self.qtest(), WANTED)
        page = self.registered(gpu)
        magic, version = struct.unpack("<II", gpu.qtest.read_bytes(page, 8))
        self.assertEqual((hex(magic), version), (hex(PRESENT_MAGIC), 1))
        self.assertEqual(self.progress(gpu, page), (0, 0))
        source, destination, pixels = self.surfaces(gpu)
        answer = gpu.control(blt(1, source, destination))
        self.assertEqual(hex(response_type(answer)), hex(RESP_OK_NODATA))
        self.assertTrue(wait_for(lambda: self.progress(gpu, page)[0] == 1))
        self.assertEqual(self.progress(gpu, page), (1, 0))
        self.assertEqual(gpu.qtest.read_bytes(destination, PAGE), pixels)

    def test_a_refused_ticket_completes_in_its_turn_and_is_counted(self):
        gpu = virtio_qtest.VirtioGpu(self.qtest(), WANTED)
        page = self.registered(gpu)
        source, destination, _ = self.surfaces(gpu)
        answer = gpu.control(blt(5, source, destination))
        self.assertEqual(hex(response_type(answer)), hex(RESP_ERR_INVALID_PARAMETER))
        self.assertEqual(self.progress(gpu, page), (1, 1))
        self.assertEqual(gpu.qtest.read_bytes(destination, 8), bytes(8))
        answer = gpu.control(blt(2, source, destination))
        self.assertEqual(hex(response_type(answer)), hex(RESP_OK_NODATA))
        self.assertTrue(wait_for(lambda: self.progress(gpu, page) == (2, 1)))

    def test_nothing_is_taken_without_a_page(self):
        gpu = virtio_qtest.VirtioGpu(self.qtest(), WANTED)
        source, destination, _ = self.surfaces(gpu)
        answer = gpu.control(blt(1, source, destination))
        self.assertEqual(hex(response_type(answer)), hex(RESP_ERR_INVALID_PARAMETER))
        for body in (struct.pack("<Q", source + 8), struct.pack("<Q", 1 << 40)):
            self.assertEqual(hex(response_type(gpu.control(request(PRESENT_ENGINE, body)))),
                             hex(RESP_ERR_INVALID_PARAMETER))

    def test_the_present_line_pends_its_vector_as_tickets_complete(self):
        qtest = self.qtest()
        msix = {}

        def configure(device):
            msix["table"] = virtio_qtest.Msix(qtest, device.pci)
            msix["table"].enable()
            self.assertEqual(device.set_queue_vector(PRESENT_QUEUE, 3), 3)

        gpu = virtio_qtest.VirtioGpu(qtest, WANTED, configure)
        table = msix["table"]
        page = self.registered(gpu)
        self.assertFalse(table.pending(3))
        source, destination, _ = self.surfaces(gpu)
        gpu.control(blt(1, source, destination))
        self.assertTrue(wait_for(lambda: table.pending(3)))
        self.assertEqual(self.progress(gpu, page), (1, 0))


if __name__ == "__main__":
    unittest.main()
