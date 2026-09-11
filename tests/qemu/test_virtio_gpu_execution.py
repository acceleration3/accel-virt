"""Execution against the real device: a bound context's batches run command by
command through the executor library the device loaded - here the echo
executor (echo_executor.cpp) - and the last answer asked for lands in the
context's reply slot before head passes the batch.

Run by the qemu_virtio_gpu ctest in the qemu configuration, which sets
AV_QEMU_BUILD and AV_EXECUTOR.
"""
import os
import struct
import time
import unittest

import virtio_qtest
from rvk_context import (COUNT, DOORBELL_SHM_ID, ECHO, EXECUTOR, FEATURE_CONTEXT_INIT, GENERATE_REPLY, LOSE_DEVICE,
                         PAGE, RESP_ERR_INVALID_PARAMETER, RESP_ERR_INVALID_RESOURCE_ID, RESP_OK_NODATA,
                         UNDECODABLE, GuestRing, command, create_context, guest_blob, host_blob, response_type,
                         rvk_device, setup, submit)

BUILD = os.environ.get("AV_QEMU_BUILD", "")
WANTED = virtio_qtest.FEATURE_VERSION_1 | virtio_qtest.FEATURE_EDID | FEATURE_CONTEXT_INIT
REPLY_MAGIC = 0x594B5652
STATUS_OK, STATUS_TOO_LARGE = 0, 1


def wait_for(condition, seconds=10.0):
    deadline = time.monotonic() + seconds
    while not condition():
        if time.monotonic() > deadline:
            return False
        time.sleep(0.001)
    return True


class Context:
    """A context with a guest-backed ring, fence page and, if asked, reply slot."""

    def __init__(self, test, gpu, context, doorbell, reply_bytes=PAGE):
        self.test = test
        self.gpu = gpu
        self.doorbell = doorbell
        memory = gpu.device.memory
        ring_address = memory.allocate(2 * PAGE)
        self.ring = GuestRing(gpu.qtest, ring_address)
        self.reply = memory.allocate(PAGE) if reply_bytes else 0
        ids = [200 + 3 * context, 201 + 3 * context, 202 + 3 * context if reply_bytes else 0]
        requests = [create_context(context), guest_blob(ids[0], ring_address, 2 * PAGE),
                    host_blob(ids[1], PAGE)]
        if reply_bytes:
            gpu.qtest.write_bytes(self.reply, b"\xEE" * 64)
            requests.append(guest_blob(ids[2], self.reply, reply_bytes))
        requests.append(submit(context, setup(ids[0], ids[1], 12, doorbell) [:-4] + struct.pack("<I", ids[2])))
        for request in requests:
            test.assertEqual(hex(response_type(gpu.control(request))), hex(RESP_OK_NODATA))
        self.fence = 0

    def run(self, *commands):
        """One batch of these commands through the doorbell: whether head passed it."""
        self.fence += 1
        self.ring.write(struct.pack("<IIQ", 0x424B5652, len(b"".join(commands)), self.fence) + b"".join(commands)
                        + bytes(-(16 + len(b"".join(commands))) % 8))
        self.ring.publish()
        self.gpu.qtest.write(32, self.gpu.device.shared_memory[DOORBELL_SHM_ID][0] + self.doorbell * PAGE, 1)
        return self.ring.wait_head(self.ring.tail, seconds=2.0)

    def answer(self):
        """(status, fence, size, required) and the bytes that follow."""
        header = struct.unpack("<IIQIIQ", self.gpu.qtest.read_bytes(self.reply, 32))
        self.test.assertEqual(header[0], REPLY_MAGIC)
        return header[1:5], self.gpu.qtest.read_bytes(self.reply + 32, header[3]) if header[3] else b""


@unittest.skipUnless(BUILD and EXECUTOR, "run by the qemu_virtio_gpu ctest")
class VirtioGpuExecution(unittest.TestCase):
    def open(self, device=rvk_device()):
        qtest = virtio_qtest.Qtest(BUILD, device)
        self.addCleanup(qtest.close)
        return virtio_qtest.VirtioGpu(qtest, WANTED)

    def expect(self, gpu, request, answer):
        self.assertEqual(hex(response_type(gpu.control(request))), hex(answer), request[:32].hex())

    def test_the_last_answer_asked_for_lands_in_the_reply_slot(self):
        gpu = self.open()
        context = Context(self, gpu, 1, 0)
        self.assertTrue(context.run(command(ECHO, b"first", GENERATE_REPLY), command(ECHO, b"second answer",
                                                                                   GENERATE_REPLY),
                                    command(ECHO, b"not asked for")))
        (status, fence, size, required), answer = context.answer()
        self.assertEqual((status, fence, size, required), (STATUS_OK, 1, 13, 13))
        self.assertEqual(answer, b"second answer")

    def test_commands_run_in_order_on_one_thread_and_unknown_ones_are_skipped(self):
        gpu = self.open()
        context = Context(self, gpu, 1, 2)
        self.assertTrue(context.run(command(ECHO, b"1"), command(0x10099), command(0x20001, b"cl"),
                                    command(0x00004), command(ECHO), command(COUNT, b"", GENERATE_REPLY)))
        (status, _, size, _), answer = context.answer()
        self.assertEqual((status, size), (STATUS_OK, 12))
        self.assertEqual(struct.unpack("<QI", answer), (2, 1))
        self.assertTrue(context.run(command(COUNT, b"", GENERATE_REPLY)))
        self.assertEqual(struct.unpack("<QI", context.answer()[1]), (3, 1))

    def test_an_answer_larger_than_the_slot_says_what_it_needed(self):
        gpu = self.open()
        context = Context(self, gpu, 1, 3, reply_bytes=64)
        self.assertTrue(context.run(command(ECHO, bytes(40), GENERATE_REPLY)))
        (status, fence, size, required), answer = context.answer()
        self.assertEqual((status, fence, size, required), (STATUS_TOO_LARGE, 1, 0, 40))
        self.assertTrue(context.run(command(ECHO, b"x" * 32, GENERATE_REPLY)))
        self.assertEqual(context.answer(), ((STATUS_OK, 2, 32, 32), b"x" * 32))

    def test_a_batch_that_cannot_run_stops_only_its_context(self):
        batches = {
            "undecodable": command(UNDECODABLE),
            "device lost": command(LOSE_DEVICE),
            "framing": command(ECHO) + b"\x01\x00\x01",
            "an answer with no reply slot": command(ECHO, b"", GENERATE_REPLY),
        }
        for name, commands in batches.items():
            with self.subTest(name):
                gpu = self.open()
                broken = Context(self, gpu, 1, 4, reply_bytes=0)
                healthy = Context(self, gpu, 2, 5)
                self.assertTrue(broken.run(command(ECHO)))
                self.assertFalse(broken.run(commands))
                self.assertTrue(wait_for(lambda: "context faulted" in gpu.qtest.printed()))
                self.assertTrue(healthy.run(command(ECHO, b"still", GENERATE_REPLY)))
                self.assertEqual(healthy.answer()[1], b"still")

    def test_a_context_whose_executor_cannot_be_created_consumes_nothing(self):
        os.environ["RVK_ECHO_FAIL_CREATE"] = "1"
        try:
            gpu = self.open()
        finally:
            del os.environ["RVK_ECHO_FAIL_CREATE"]
        context = Context(self, gpu, 1, 6)
        self.assertFalse(context.run(command(ECHO)))
        self.assertTrue(wait_for(lambda: "executor could not be created" in gpu.qtest.printed()))

    def test_setups_naming_an_unusable_reply_slot_are_refused(self):
        gpu = self.open()
        ring = gpu.device.memory.allocate(2 * PAGE)
        GuestRing(gpu.qtest, ring)
        small = gpu.device.memory.allocate(PAGE)
        for request in (create_context(1), guest_blob(10, ring, 2 * PAGE), host_blob(11, PAGE),
                        guest_blob(12, small, 63)):
            self.expect(gpu, request, RESP_OK_NODATA)
        for reply, answer in ((99, RESP_ERR_INVALID_RESOURCE_ID), (12, RESP_ERR_INVALID_PARAMETER),
                              (10, RESP_ERR_INVALID_PARAMETER), (11, RESP_ERR_INVALID_PARAMETER)):
            with self.subTest(reply=reply):
                self.expect(gpu, submit(1, setup(10, 11, 12, 0)[:-4] + struct.pack("<I", reply)), answer)
        self.expect(gpu, submit(1, setup(10, 11, 12, 0)), RESP_OK_NODATA)


if __name__ == "__main__":
    unittest.main()
