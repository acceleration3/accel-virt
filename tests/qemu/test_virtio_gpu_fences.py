"""Fence pages and fence interrupts against the real device: a worker writes a
bound context's fence page after every batch it consumes, and interrupts the
guest through the guest notifier of the context's fence queue. Under qtest
virtio-pci raises the vector from its notifier handler, so a masked vector's
pending bit - and, with MSI-X off, the ISR - shows the interrupt arrived.

Run by the qemu_virtio_gpu ctest in the qemu configuration, which sets
AV_QEMU_BUILD.
"""
import os
import struct
import time
import unittest

import virtio_qtest
from rvk_context import (DOORBELL_SHM_ID, FEATURE_CONTEXT_INIT, PAGE, RESP_OK_NODATA, GuestRing, create_context,
                         guest_blob, response_type, rvk_device, setup, submit)

BUILD = os.environ.get("AV_QEMU_BUILD", "")
FENCE_MAGIC = 0x464B5652
FENCE_QUEUE_FIRST = 2
DOORBELLS = 64
WANTED = virtio_qtest.FEATURE_VERSION_1 | virtio_qtest.FEATURE_EDID | FEATURE_CONTEXT_INIT
QUIET = 0.3


def wait_for(condition, seconds=10.0):
    deadline = time.monotonic() + seconds
    while not condition():
        if time.monotonic() > deadline:
            return False
        time.sleep(0.001)
    return True


class Context:
    """A context bound to a guest-backed ring and a guest-backed fence page."""

    def __init__(self, test, gpu, context, doorbell):
        self.gpu = gpu
        self.doorbell = doorbell
        ring_address = gpu.device.memory.allocate(2 * PAGE)
        self.page = gpu.device.memory.allocate(PAGE)
        gpu.qtest.write_bytes(self.page, b"\xEE" * 32)
        self.ring = GuestRing(gpu.qtest, ring_address)
        ring_id, fence_id = 100 + 2 * context, 101 + 2 * context
        for request in (create_context(context), guest_blob(ring_id, ring_address, 2 * PAGE),
                        guest_blob(fence_id, self.page, PAGE), submit(context, setup(ring_id, fence_id, 12, doorbell))):
            test.assertEqual(hex(response_type(gpu.control(request))), hex(RESP_OK_NODATA))
        self.fence = 0

    def fence_page(self):
        return struct.unpack("<IIQQII", self.gpu.qtest.read_bytes(self.page, 32))

    def complete(self, batches=1):
        """Batches written and published and the doorbell rung: the last fence."""
        for _ in range(batches):
            self.fence += 1
            self.ring.write_batch(self.fence, 24)
        self.ring.publish()
        address = self.gpu.device.shared_memory[DOORBELL_SHM_ID][0] + self.doorbell * PAGE
        self.gpu.qtest.write(32, address, 1)
        return self.fence

    def completed(self, fence):
        return wait_for(lambda: self.fence_page()[2] == fence)


@unittest.skipUnless(BUILD, "run by the qemu_virtio_gpu ctest")
class VirtioGpuFences(unittest.TestCase):
    def qtest(self, device=rvk_device(",vectors=6")):
        qtest = virtio_qtest.Qtest(BUILD, device)
        self.addCleanup(qtest.close)
        return qtest

    def msix_gpu(self, qtest, vectors):
        """A driver with MSI-X on, every vector masked, and fence queue vectors
        assigned before DRIVER_OK: {doorbell: vector}."""
        msix = {}

        def configure(device):
            msix["table"] = virtio_qtest.Msix(qtest, device.pci)
            msix["table"].enable()
            for doorbell, vector in vectors.items():
                self.assertEqual(device.set_queue_vector(FENCE_QUEUE_FIRST + doorbell, vector), vector)

        gpu = virtio_qtest.VirtioGpu(qtest, WANTED, configure)
        return gpu, msix["table"]

    def test_the_device_has_a_fence_queue_per_doorbell(self):
        gpu = virtio_qtest.VirtioGpu(self.qtest(), WANTED)
        self.assertEqual(gpu.device.queue_count(), FENCE_QUEUE_FIRST + DOORBELLS)
        stock = virtio_qtest.VirtioGpu(self.qtest("virtio-gpu-pci"))
        self.assertEqual(stock.device.queue_count(), 2)

    def test_a_setup_writes_a_fresh_page_and_completions_advance_it(self):
        gpu = virtio_qtest.VirtioGpu(self.qtest(), WANTED)
        context = Context(self, gpu, 1, 4)
        self.assertEqual(context.fence_page(), (FENCE_MAGIC, 0, 0, 0, 0, 0))
        fence = context.complete(3)
        self.assertTrue(context.completed(fence))
        self.assertEqual(context.fence_page(), (FENCE_MAGIC, 0, 3, 6, 0, 0))   # a command in each batch
        fence = context.complete(2)
        self.assertTrue(context.completed(fence))
        self.assertEqual(context.fence_page()[3], 10)

    def test_a_page_the_guest_overwrites_is_rewritten_from_the_device_counters(self):
        gpu = virtio_qtest.VirtioGpu(self.qtest(), WANTED)
        context = Context(self, gpu, 1, 0)
        self.assertTrue(context.completed(context.complete()))
        gpu.qtest.write_bytes(context.page, b"\xFF" * 32)
        self.assertTrue(context.completed(context.complete()))
        self.assertEqual(context.fence_page(), (FENCE_MAGIC, 0, 2, 4, 0, 0))

    def test_a_faulted_ring_leaves_its_page_faulted(self):
        gpu = virtio_qtest.VirtioGpu(self.qtest(), WANTED)
        context = Context(self, gpu, 1, 0)
        self.assertTrue(context.completed(context.complete()))
        context.ring.publish(8)
        address = gpu.device.shared_memory[DOORBELL_SHM_ID][0]
        gpu.qtest.write(32, address, 1)
        self.assertTrue(wait_for(lambda: "context faulted" in gpu.qtest.printed()))
        self.assertEqual(context.fence_page(), (FENCE_MAGIC, 1, 1, 2, 2, 0))   # faulted: ring tail

    def test_a_completion_pends_the_masked_vector_of_its_fence_queue_until_unmasked(self):
        qtest = self.qtest()
        gpu, msix = self.msix_gpu(qtest, {5: 3})
        context = Context(self, gpu, 1, 5)
        self.assertFalse(msix.pending(3))
        self.assertTrue(context.completed(context.complete()))
        self.assertTrue(wait_for(lambda: msix.pending(3)))
        msix.mask(3, False)
        self.assertTrue(wait_for(lambda: not msix.pending(3)))

    def test_each_context_interrupts_on_its_own_vector(self):
        qtest = self.qtest()
        gpu, msix = self.msix_gpu(qtest, {1: 4, 2: 5})
        first = Context(self, gpu, 1, 1)
        second = Context(self, gpu, 2, 2)
        self.assertTrue(first.completed(first.complete(2)))
        self.assertTrue(wait_for(lambda: msix.pending(4)))
        time.sleep(QUIET)
        self.assertFalse(msix.pending(5))
        self.assertTrue(second.completed(second.complete()))
        self.assertTrue(wait_for(lambda: msix.pending(5)))

    def test_a_fence_queue_without_a_vector_raises_nothing(self):
        qtest = self.qtest()
        gpu, msix = self.msix_gpu(qtest, {})
        context = Context(self, gpu, 1, 7)
        self.assertTrue(context.completed(context.complete()))
        time.sleep(QUIET)
        self.assertFalse(any(msix.pending(vector) for vector in range(msix.vectors)))

    def test_with_msix_off_a_completion_sets_the_isr(self):
        gpu = virtio_qtest.VirtioGpu(self.qtest(), WANTED)
        context = Context(self, gpu, 1, 3)
        gpu.device.isr()   # the control responses' bit, cleared
        self.assertEqual(gpu.device.isr(), 0)
        self.assertTrue(context.completed(context.complete()))
        self.assertTrue(wait_for(lambda: gpu.device.isr() & 1))

    def test_after_a_reset_completions_interrupt_again(self):
        qtest = self.qtest()
        gpu, msix = self.msix_gpu(qtest, {6: 2})
        Context(self, gpu, 1, 6)
        again, msix = self.msix_gpu(qtest, {6: 2})
        context = Context(self, again, 1, 6)
        self.assertTrue(context.completed(context.complete()))
        self.assertTrue(wait_for(lambda: msix.pending(2)))


if __name__ == "__main__":
    unittest.main()
