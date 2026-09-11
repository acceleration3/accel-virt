"""The submission ring and doorbells against the real device: a store to a
bound context's doorbell page wakes its worker, which consumes every complete
batch in the ring and publishes head; a ring no valid guest could write stops
only its own context. The doorbell's ioeventfd is dispatched by QEMU itself
under qtest, as KVM would dispatch it in the kernel.

Run by the qemu_virtio_gpu ctest in the qemu configuration, which sets
AV_QEMU_BUILD.
"""
import os
import struct
import time
import unittest

import virtio_qtest
from rvk_context import (DOORBELL_SHM_ID, DOORBELLS, FEATURE_CONTEXT_INIT, PAGE, RESP_ERR_INVALID_PARAMETER,
                         RESP_OK_MAP_INFO, RESP_OK_NODATA, GuestRing, create_context, destroy_context, guest_blob,
                         host_blob, map_blob, response_type, ring_header, rvk_device, setup, submit, unref)

BUILD = os.environ.get("AV_QEMU_BUILD", "")
HOSTMEM = 64 << 20
SHM_ID_HOST_VISIBLE = 1
QUIET = 0.3


@unittest.skipUnless(BUILD, "run by the qemu_virtio_gpu ctest")
class VirtioGpuRing(unittest.TestCase):
    def open(self, device=rvk_device(f",hostmem={HOSTMEM}")):
        qtest = virtio_qtest.Qtest(BUILD, device)
        self.addCleanup(qtest.close)
        return virtio_qtest.VirtioGpu(qtest, virtio_qtest.FEATURE_VERSION_1 | virtio_qtest.FEATURE_EDID |
                                      FEATURE_CONTEXT_INIT)

    def expect(self, gpu, request, answer):
        self.assertEqual(hex(response_type(gpu.control(request))), hex(answer), request[:32].hex())

    def doorbells(self, gpu):
        address, length = gpu.device.shared_memory[DOORBELL_SHM_ID]
        return address

    def bind(self, gpu, context, doorbell, order=12, resource=None):
        """A context set up with a guest-backed ring and a fence blob: its ring."""
        ring_id = resource or 100 + 2 * context
        address = gpu.device.memory.allocate(((32 + (1 << order)) + PAGE - 1) & ~(PAGE - 1))
        ring = GuestRing(gpu.qtest, address, order)
        self.expect(gpu, create_context(context), RESP_OK_NODATA)
        self.expect(gpu, guest_blob(ring_id, address, ring.blob_size), RESP_OK_NODATA)
        self.expect(gpu, host_blob(ring_id + 1, PAGE), RESP_OK_NODATA)
        self.expect(gpu, submit(context, setup(ring_id, ring_id + 1, order, doorbell)), RESP_OK_NODATA)
        return ring

    def ring_doorbell(self, gpu, doorbell, width=32, offset=0):
        gpu.qtest.write(width, self.doorbells(gpu) + doorbell * PAGE + offset, 0xD00D)

    def attached(self, gpu, ring, doorbell):
        """One batch through the doorbell: after it, the worker has attached the
        ring, so nothing written later is consumed without a store."""
        ring.write_batch(ring.tail // 16 + 1, 0)
        ring.publish()
        self.ring_doorbell(gpu, doorbell)
        self.assertTrue(ring.wait_head(ring.tail))

    def stays(self, ring, head):
        time.sleep(QUIET)
        self.assertEqual(ring.head(), head)

    def test_the_doorbells_are_bar_0_one_page_per_context(self):
        gpu = self.open()
        address, length = gpu.device.shared_memory[DOORBELL_SHM_ID]
        self.assertEqual(address, gpu.device.pci.bars[0])
        self.assertEqual(length, DOORBELLS * PAGE)
        self.assertEqual(gpu.qtest.read(32, address + 3 * PAGE), 0)
        stock_qtest = virtio_qtest.Qtest(BUILD, f"virtio-gpu-pci,hostmem={HOSTMEM}")
        self.addCleanup(stock_qtest.close)
        stock = virtio_qtest.VirtioGpu(stock_qtest)
        self.assertNotIn(DOORBELL_SHM_ID, stock.device.shared_memory)
        self.assertNotIn(0, stock.device.pci.bars)

    def test_a_doorbell_store_consumes_every_complete_batch(self):
        gpu = self.open()
        ring = self.bind(gpu, 1, 3)
        for fence in range(1, 4):
            ring.write_batch(fence, 13 * fence)
        ring.publish()
        self.ring_doorbell(gpu, 3)
        self.assertTrue(ring.wait_head(ring.tail))
        ring.write_batch(4, 0)
        ring.publish()
        self.ring_doorbell(gpu, 3)
        self.assertTrue(ring.wait_head(ring.tail))

    def test_a_batch_the_guest_has_not_finished_is_consumed_once_published(self):
        gpu = self.open()
        ring = self.bind(gpu, 1, 0)
        ring.write_batch(1, 8)
        ring.write_batch(2, 200)
        ring.publish(24 + 100)
        self.ring_doorbell(gpu, 0)
        self.assertTrue(ring.wait_head(24))
        self.stays(ring, 24)
        ring.publish()
        self.ring_doorbell(gpu, 0)
        self.assertTrue(ring.wait_head(ring.tail))

    def test_batches_are_read_across_the_end_of_the_data_area(self):
        gpu = self.open()
        ring = self.bind(gpu, 1, 63)
        fence = 1
        while ring.tail < 3 * ring.capacity:
            for _ in range(5):
                ring.write_batch(fence, 300 + fence % 97)
                fence += 1
            ring.publish()
            self.ring_doorbell(gpu, 63)
            self.assertTrue(ring.wait_head(ring.tail))

    def test_stores_that_are_not_a_connected_doorbell_consume_nothing(self):
        gpu = self.open()
        ring = self.bind(gpu, 1, 5)
        self.attached(gpu, ring, 5)
        ring.write_batch(9, 0)
        ring.publish()
        self.ring_doorbell(gpu, 6)
        self.ring_doorbell(gpu, 5, offset=4)
        self.ring_doorbell(gpu, 5, width=8)
        self.ring_doorbell(gpu, 5, width=16)
        self.stays(ring, 16)
        self.ring_doorbell(gpu, 5)
        self.assertTrue(ring.wait_head(32))

    def test_each_doorbell_wakes_only_its_own_context(self):
        gpu = self.open()
        first = self.bind(gpu, 1, 10)
        second = self.bind(gpu, 2, 11)
        self.attached(gpu, first, 10)
        self.attached(gpu, second, 11)
        for ring in (first, second):
            ring.write_batch(9, 32)
            ring.publish()
        self.ring_doorbell(gpu, 11)
        self.assertTrue(second.wait_head(second.tail))
        self.stays(first, 16)
        self.ring_doorbell(gpu, 10)
        self.assertTrue(first.wait_head(first.tail))

    def test_a_hostile_ring_stops_only_its_own_context(self):
        spoilers = {
            "tail backwards": lambda ring: ring.publish(8),
            "tail past a whole ring": lambda ring: ring.publish(16 + ring.capacity + 8),
            "header magic": lambda ring: ring.qtest.write(32, ring.address, 0),
            "order changed": lambda ring: ring.qtest.write(8, ring.address + 4, 13),
            "fence repeated": lambda ring: (ring.write_batch(1, 0), ring.publish()),
            "batch magic": lambda ring: (ring.write(struct.pack("<IIQ", 0x11111111, 0, 2)), ring.publish()),
            "batch longer than the ring": lambda ring: (ring.write(struct.pack("<IIQ", 0x424B5652, 1 << 20, 2)),
                                                        ring.publish(16 + ring.capacity)),
        }
        for name, spoil in spoilers.items():
            with self.subTest(name):
                gpu = self.open()
                hostile = self.bind(gpu, 1, 1)
                healthy = self.bind(gpu, 2, 2)
                self.attached(gpu, hostile, 1)
                spoil(hostile)
                self.ring_doorbell(gpu, 1)
                deadline = time.monotonic() + 10
                while "context ring stopped" not in gpu.qtest.printed() and time.monotonic() < deadline:
                    time.sleep(0.01)
                self.assertIn("context ring stopped", gpu.qtest.printed())
                healthy.write_batch(1, 64)
                healthy.publish()
                self.ring_doorbell(gpu, 2)
                self.assertTrue(healthy.wait_head(healthy.tail))
                self.assertEqual(hostile.head(), 16)
                self.expect(gpu, destroy_context(1), RESP_OK_NODATA)

    def test_setups_naming_an_unusable_doorbell_or_ring_bind_nothing(self):
        gpu = self.open()
        self.bind(gpu, 1, 7)
        self.expect(gpu, create_context(2), RESP_OK_NODATA)
        address = gpu.device.memory.allocate(2 * PAGE)
        self.expect(gpu, guest_blob(50, address, 2 * PAGE), RESP_OK_NODATA)
        self.expect(gpu, host_blob(51, PAGE), RESP_OK_NODATA)
        headers = (
            ("malformed", struct.pack("<I", 0x12345678) + ring_header(12)[4:]),
            ("another order", ring_header(13)),
            ("head moved", ring_header(12, 64, 16)),
            ("tail past a ring", ring_header(12, 4097)),
        )
        for name, written in headers:
            with self.subTest(name):
                gpu.qtest.write_bytes(address, written)
                self.expect(gpu, submit(2, setup(50, 51, 12, 8)), RESP_ERR_INVALID_PARAMETER)
        gpu.qtest.write_bytes(address, ring_header(12))
        self.expect(gpu, submit(2, setup(50, 51, 12, DOORBELLS)), RESP_ERR_INVALID_PARAMETER)
        self.expect(gpu, submit(2, setup(50, 51, 12, 7)), RESP_ERR_INVALID_PARAMETER)
        self.expect(gpu, unref(50), RESP_OK_NODATA)
        self.expect(gpu, unref(51), RESP_OK_NODATA)
        self.expect(gpu, destroy_context(1), RESP_OK_NODATA)
        ring = self.bind(gpu, 3, 7)
        ring.write_batch(1, 8)
        ring.publish()
        self.ring_doorbell(gpu, 7)
        self.assertTrue(ring.wait_head(ring.tail))

    def test_a_reset_disconnects_every_doorbell(self):
        gpu = self.open()
        old = self.bind(gpu, 1, 9)
        self.attached(gpu, old, 9)
        again = virtio_qtest.VirtioGpu(gpu.qtest, gpu.features)   # a new driver resets the device first
        old.write_batch(9, 0)
        old.publish()
        self.ring_doorbell(again, 9)
        self.stays(old, 16)
        ring = self.bind(again, 1, 9)
        ring.write_batch(1, 0)
        ring.publish()
        self.ring_doorbell(again, 9)
        self.assertTrue(ring.wait_head(16))

    def test_a_host_visible_ring_is_consumed_through_its_mapping(self):
        gpu = self.open()
        hostmem = gpu.device.shared_memory[SHM_ID_HOST_VISIBLE][0]
        self.expect(gpu, create_context(1), RESP_OK_NODATA)
        self.expect(gpu, host_blob(20, 2 * PAGE), RESP_OK_NODATA)
        self.assertEqual(hex(response_type(gpu.control(map_blob(20, 4 * PAGE), 32))), hex(RESP_OK_MAP_INFO))
        ring = GuestRing(gpu.qtest, hostmem + 4 * PAGE)
        self.expect(gpu, host_blob(21, PAGE), RESP_OK_NODATA)
        self.expect(gpu, submit(1, setup(20, 21, 12, 4)), RESP_OK_NODATA)
        ring.write_batch(1, 1000)
        ring.publish()
        self.ring_doorbell(gpu, 4)
        self.assertTrue(ring.wait_head(ring.tail))


if __name__ == "__main__":
    unittest.main()
