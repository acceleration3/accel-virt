import os
import struct
import time
import unittest

import virtio_qtest
from av_context import FEATURE_CONTEXT_INIT, PAGE, RESP_OK_NODATA, response_type, av_device

BUILD = os.environ.get("AV_QEMU_BUILD", "")
WANTED = virtio_qtest.FEATURE_VERSION_1 | virtio_qtest.FEATURE_EDID | FEATURE_CONTEXT_INIT
CLOCK_MAGIC = 0x00435641
DISPLAY_CLOCK = 0x02F4
DISPLAY_TIMING = 0x02F5
DISPLAY_QUEUE = 66
RESP_ERR_INVALID_PARAMETER = 0x1205
QUIET = 0.3


def wait_for(condition, seconds=10.0):
    deadline = time.monotonic() + seconds
    while not condition():
        if time.monotonic() > deadline:
            return False
        time.sleep(0.001)
    return True


def request(kind, body):
    return struct.pack("<IIQII", kind, 0, 0, 0, 0) + body


class Blank:
    def __init__(self, qtest, page, head):
        at = page + 32 + 64 * head
        for _ in range(1000):
            before = struct.unpack("<I", qtest.read_bytes(at, 4))[0]
            fields = qtest.read_bytes(at + 4, 32)
            after = struct.unpack("<I", qtest.read_bytes(at, 4))[0]
            if before == after and before % 2 == 0:
                break
        (self.latched, self.sequence, self.vblank, self.period, self.refresh,
         self.every) = struct.unpack("<IQQIII", fields)


@unittest.skipUnless(BUILD, "run by the qemu_virtio_gpu ctest")
class VirtioGpuDisplayClock(unittest.TestCase):
    def qtest(self):
        qtest = virtio_qtest.Qtest(BUILD, av_device(",vectors=4"))
        self.addCleanup(qtest.close)
        return qtest

    def registered(self, gpu):
        page = gpu.device.memory.allocate(PAGE)
        answer = gpu.control(request(DISPLAY_CLOCK, struct.pack("<Q", page)))
        self.assertEqual(hex(response_type(answer)), hex(RESP_OK_NODATA))
        return page

    def test_a_registered_page_is_headed_and_its_blanks_come_at_sixty_hertz(self):
        gpu = virtio_qtest.VirtioGpu(self.qtest(), WANTED)
        page = self.registered(gpu)
        magic, version, heads = struct.unpack("<III", gpu.qtest.read_bytes(page, 12))
        self.assertEqual((hex(magic), version, heads), (hex(CLOCK_MAGIC), 1, 16))
        first = Blank(gpu.qtest, page, 0)
        self.assertEqual((first.refresh, first.period), (60000, 16666666))
        time.sleep(0.5)
        second = Blank(gpu.qtest, page, 0)
        blanks = second.sequence - first.sequence
        self.assertGreaterEqual(blanks, 20)
        self.assertLessEqual(blanks, 40)
        self.assertAlmostEqual((second.vblank - first.vblank) / blanks, 1e9 / 60, delta=1.0)
        self.assertEqual(second.latched, 0)
        last = Blank(gpu.qtest, page, 15)
        self.assertGreaterEqual(last.sequence, second.sequence - 1)

    def test_a_new_rate_takes_over_and_the_count_carries_on(self):
        gpu = virtio_qtest.VirtioGpu(self.qtest(), WANTED)
        page = self.registered(gpu)
        before = Blank(gpu.qtest, page, 2)
        answer = gpu.control(request(DISPLAY_TIMING, struct.pack("<II", 2, 1000000)))
        self.assertEqual(hex(response_type(answer)), hex(RESP_OK_NODATA))
        self.assertTrue(wait_for(lambda: Blank(gpu.qtest, page, 2).refresh == 1000000))
        faster = Blank(gpu.qtest, page, 2)
        self.assertGreaterEqual(faster.sequence, before.sequence)
        self.assertEqual(faster.period, 1000000)
        time.sleep(0.2)
        self.assertGreaterEqual(Blank(gpu.qtest, page, 2).sequence - faster.sequence, 100)
        self.assertEqual(Blank(gpu.qtest, page, 0).refresh, 60000)

    def test_what_the_protocol_does_not_allow_is_refused(self):
        gpu = virtio_qtest.VirtioGpu(self.qtest(), WANTED)
        page = gpu.device.memory.allocate(PAGE)
        for body in (struct.pack("<Q", page + 8), struct.pack("<Q", 1 << 40)):
            self.assertEqual(hex(response_type(gpu.control(request(DISPLAY_CLOCK, body)))),
                             hex(RESP_ERR_INVALID_PARAMETER))
        for head, rate in ((16, 60000), (0, 999), (0, 1000001)):
            answer = gpu.control(request(DISPLAY_TIMING, struct.pack("<II", head, rate)))
            self.assertEqual(hex(response_type(answer)), hex(RESP_ERR_INVALID_PARAMETER))

    def test_the_display_line_pends_its_vector_on_the_blanks_the_guest_asked_for(self):
        qtest = self.qtest()
        msix = {}

        def configure(device):
            msix["table"] = virtio_qtest.Msix(qtest, device.pci)
            msix["table"].enable()
            self.assertEqual(device.set_queue_vector(DISPLAY_QUEUE, 3), 3)

        gpu = virtio_qtest.VirtioGpu(qtest, WANTED, configure)
        table = msix["table"]
        page = self.registered(gpu)
        time.sleep(QUIET)
        self.assertFalse(table.pending(3))
        gpu.qtest.write_bytes(page + 32 + 32, struct.pack("<I", 1))
        self.assertTrue(wait_for(lambda: table.pending(3)))
        self.assertFalse(any(table.pending(vector) for vector in range(3)))
        gpu.qtest.write_bytes(page + 32 + 32, struct.pack("<I", 0))
        table.mask(3, False)
        self.assertTrue(wait_for(lambda: not table.pending(3)))
        table.mask(3, True)
        time.sleep(QUIET)
        self.assertFalse(table.pending(3))


if __name__ == "__main__":
    unittest.main()
