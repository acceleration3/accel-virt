import os
import struct
import time
import unittest

import virtio_qtest
from av_context import (COUNT, DOORBELL_SHM_ID, ECHO, EXECUTOR, FEATURE_CONTEXT_INIT, GENERATE_REPLY, LOSE_DEVICE,
                         PAGE, RESP_OK_NODATA, UNDECODABLE, GuestRing, batch, batch_header, command, create_context,
                         destroy_context, guest_blob, response_type, ring_header, av_device, setup, submit)

BUILD = os.environ.get("AV_QEMU_BUILD", "")
WANTED = virtio_qtest.FEATURE_VERSION_1 | virtio_qtest.FEATURE_EDID | FEATURE_CONTEXT_INIT
FENCE_QUEUE_FIRST = 2
FAULTED = 1
COMMANDS_PER_HEARTBEAT = 256
RING_HEADER, RING_TAIL, BATCH_HEADER, BATCH_ORDER, COMMAND_FRAMING, UNDECODABLE_CODE, DEVICE_LOST, NO_EXECUTOR = \
    range(1, 9)


def wait_for(condition, seconds=10.0):
    deadline = time.monotonic() + seconds
    while not condition():
        if time.monotonic() > deadline:
            return False
        time.sleep(0.001)
    return True


class Context:
    def __init__(self, test, gpu, context, doorbell):
        self.test, self.gpu, self.context, self.doorbell = test, gpu, context, doorbell
        memory = gpu.device.memory
        self.ring_address = memory.allocate(2 * PAGE)
        self.page = memory.allocate(PAGE)
        self.reply = memory.allocate(PAGE)
        self.ids = (300 + 3 * context, 301 + 3 * context, 302 + 3 * context)
        for request in (guest_blob(self.ids[0], self.ring_address, 2 * PAGE), guest_blob(self.ids[1], self.page, PAGE),
                        guest_blob(self.ids[2], self.reply, PAGE)):
            test.assertEqual(hex(response_type(gpu.control(request))), hex(RESP_OK_NODATA))
        self.set_up()

    def set_up(self):
        self.ring = GuestRing(self.gpu.qtest, self.ring_address)
        self.fence = 0
        ring_id, fence_id, reply_id = self.ids
        for request in (create_context(self.context),
                        submit(self.context, setup(ring_id, fence_id, 12, self.doorbell)[:-4] + struct.pack("<I", reply_id))):
            self.test.assertEqual(hex(response_type(self.gpu.control(request))), hex(RESP_OK_NODATA))

    def destroy(self):
        self.test.assertEqual(hex(response_type(self.gpu.control(destroy_context(self.context)))), hex(RESP_OK_NODATA))

    def fence_page(self):
        return struct.unpack("<IIQQII", self.gpu.qtest.read_bytes(self.page, 32))

    def ring_doorbell(self):
        self.gpu.qtest.write(32, self.gpu.device.shared_memory[DOORBELL_SHM_ID][0] + self.doorbell * PAGE, 1)

    def send(self, *commands):
        self.fence += 1
        return self.send_raw(batch_header(len(b"".join(commands)), self.fence) + b"".join(commands)
                             + bytes(-(32 + len(b"".join(commands))) % 8))

    def send_raw(self, data):
        self.ring.write(data)
        self.ring.publish()
        self.ring_doorbell()
        return self.fence

    def completed(self, fence):
        return wait_for(lambda: self.fence_page()[2] == fence)

    def faulted_with(self, code):
        return wait_for(lambda: self.fence_page()[1] == FAULTED and self.fence_page()[4] == code)

    def answer(self):
        size = struct.unpack_from("<I", self.gpu.qtest.read_bytes(self.reply + 16, 4))[0]
        return self.gpu.qtest.read_bytes(self.reply + 32, size)


@unittest.skipUnless(BUILD and EXECUTOR, "run by the qemu_virtio_gpu ctest")
class VirtioGpuFaults(unittest.TestCase):
    def gpu(self, vectors=None, qtest=None):
        if qtest is None:
            qtest = virtio_qtest.Qtest(BUILD, av_device(",vectors=4"))
            self.addCleanup(qtest.close)
        msix = {}

        def configure(device):
            msix["table"] = virtio_qtest.Msix(qtest, device.pci)
            msix["table"].enable()
            for doorbell, vector in (vectors or {}).items():
                device.set_queue_vector(FENCE_QUEUE_FIRST + doorbell, vector)

        return virtio_qtest.VirtioGpu(qtest, WANTED, configure), msix["table"]

    def test_each_reason_faults_the_context_with_its_code_and_interrupts(self):
        spoilers = {
            RING_HEADER: lambda c: (c.gpu.qtest.write(32, c.ring_address, 0), c.ring_doorbell()),
            RING_TAIL: lambda c: (c.ring.publish(8), c.ring_doorbell()),
            BATCH_HEADER: lambda c: c.send_raw(batch_header(0, 9, magic=0x12345678)),
            BATCH_ORDER: lambda c: c.send_raw(batch(1, 8)),
            COMMAND_FRAMING: lambda c: c.send(command(ECHO) + b"\x01\x02\x03"),
            UNDECODABLE_CODE: lambda c: c.send(command(UNDECODABLE)),
            DEVICE_LOST: lambda c: c.send(command(LOSE_DEVICE)),
        }
        for code, spoil in spoilers.items():
            with self.subTest(code=code):
                gpu, msix = self.gpu({1: 3})
                context = Context(self, gpu, 1, 1)
                echoes = (command(ECHO) for _ in range(COMMANDS_PER_HEARTBEAT))
                self.assertTrue(context.completed(context.send(*echoes)))
                self.assertEqual(context.fence_page()[2:4], (1, 2))
                self.assertTrue(wait_for(lambda: msix.pending(3)))
                msix.mask(3, False)
                self.assertTrue(wait_for(lambda: not msix.pending(3)))
                msix.mask(3)
                spoil(context)
                self.assertTrue(context.faulted_with(code), context.fence_page())
                self.assertEqual(context.fence_page()[2:4], (1, 3 if code == COMMAND_FRAMING else 2))
                self.assertTrue(wait_for(lambda: msix.pending(3)))
                self.assertIn(f"context faulted, code {code}", gpu.qtest.printed())

    def test_an_executor_that_cannot_be_created_faults_before_any_batch(self):
        os.environ["AV_ECHO_FAIL_CREATE"] = "1"
        try:
            gpu, _ = self.gpu()
        finally:
            del os.environ["AV_ECHO_FAIL_CREATE"]
        context = Context(self, gpu, 1, 2)
        context.send(command(ECHO))
        self.assertTrue(context.faulted_with(NO_EXECUTOR))
        self.assertEqual(context.fence_page()[2:4], (0, 0))

    def test_a_faulted_context_runs_nothing_more_and_others_carry_on(self):
        gpu, _ = self.gpu()
        broken = Context(self, gpu, 1, 4)
        healthy = Context(self, gpu, 2, 5)
        broken.send(command(UNDECODABLE))
        self.assertTrue(broken.faulted_with(UNDECODABLE_CODE))
        page = broken.fence_page()
        gpu.qtest.write_bytes(broken.reply, b"\xEE" * 32)
        broken.send(command(ECHO, b"ignored", GENERATE_REPLY))
        self.assertTrue(healthy.completed(healthy.send(command(ECHO, b"still here", GENERATE_REPLY))))
        self.assertEqual(healthy.answer(), b"still here")
        time.sleep(0.2)
        self.assertEqual(broken.fence_page(), page)
        self.assertEqual(gpu.qtest.read_bytes(broken.reply, 32), b"\xEE" * 32)

    def test_a_destroyed_and_recreated_context_starts_clean(self):
        gpu, _ = self.gpu()
        context = Context(self, gpu, 1, 6)
        self.assertTrue(context.completed(context.send(command(ECHO))))
        context.send(command(LOSE_DEVICE))
        self.assertTrue(context.faulted_with(DEVICE_LOST))
        context.destroy()
        context.set_up()
        self.assertEqual(context.fence_page()[1:5], (0, 0, 0, 0))
        self.assertTrue(context.completed(context.send(command(COUNT, b"", GENERATE_REPLY))))
        self.assertEqual(struct.unpack("<QI", context.answer()), (0, 1))

    def test_a_reset_releases_faulted_and_healthy_contexts_alike(self):
        qtest = virtio_qtest.Qtest(BUILD, av_device(",vectors=4"))
        self.addCleanup(qtest.close)
        gpu, _ = self.gpu(qtest=qtest)
        Context(self, gpu, 1, 7).send(command(UNDECODABLE))
        Context(self, gpu, 2, 8)
        again, _ = self.gpu(qtest=qtest)
        fresh = Context(self, again, 1, 7)
        self.assertTrue(fresh.completed(fresh.send(command(ECHO, b"after reset", GENERATE_REPLY))))
        self.assertEqual(fresh.answer(), b"after reset")

    def test_progress_moves_for_every_command_run(self):
        gpu, _ = self.gpu()
        context = Context(self, gpu, 1, 9)
        self.assertTrue(context.completed(context.send(*(command(ECHO) for _ in range(COMMANDS_PER_HEARTBEAT)))))
        self.assertEqual(context.fence_page()[3], 2)
        self.assertTrue(context.completed(context.send(command(0x10099), command(ECHO))))
        self.assertEqual(context.fence_page()[3], 4)
        self.assertTrue(context.completed(context.send(command(ECHO))))
        self.assertEqual(context.fence_page()[3], 5)


if __name__ == "__main__":
    unittest.main()
