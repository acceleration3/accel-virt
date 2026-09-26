"""Execution against the real device: a bound context's batches run command by
command through the executor library the device loaded - here the echo
executor (echo_executor.cpp) - and the last answer asked for lands in the
context's reply slot before head passes the batch. The transport's NOP is
answered by the device, under the echo executor and this project's own, and
IMPORT_BLOB hands the echo executor a guest chunk's pages as one host pointer,
and a host chunk placed in the hostmem window (docs/host-window.md) as its own
mapping, with the contexts in QEMU or in a renderer.

Run by the qemu_virtio_gpu ctest in the qemu configuration, which sets
AV_QEMU_BUILD and AV_EXECUTOR.
"""
import os
import struct
import time
import unittest

import virtio_qtest
from rvk_context import (COUNT, DOORBELL_SHM_ID, ECHO, EXECUTOR, FEATURE_CONTEXT_INIT, GENERATE_REPLY, batch_header,
                         IMPORT_BLOB, LOSE_DEVICE, NOP, PAGE, PEEK, POKE, REAL_EXECUTOR, RELEASE_BLOB,
                         SUB_API_OPENGL, SUB_API_VULKAN, setup_with_options,
                         RESP_ERR_INVALID_PARAMETER, RESP_ERR_INVALID_RESOURCE_ID, RESP_OK_NODATA, UNDECODABLE,
                         RESP_OK_MAP_INFO, GuestRing, command, create_context, guest_blob, guest_blob_pages,
                         header, host_blob, map_blob, response_type, rvk_device, setup, submit, unref)

BUILD = os.environ.get("AV_QEMU_BUILD", "")
RENDERER = os.environ.get("AV_RENDERER", "")
CMD_RESOURCE_UNMAP_BLOB = 0x0209
SHM_ID_HOST_VISIBLE = 1
HOSTMEM = 64 << 20
WANTED = virtio_qtest.FEATURE_VERSION_1 | virtio_qtest.FEATURE_EDID | FEATURE_CONTEXT_INIT
REPLY_MAGIC = 0x594B5652
STATUS_OK, STATUS_TOO_LARGE = 0, 1
HANDLE_BIT = 0x4000000000000000
IMPORTED, REFUSED_REQUEST, UNKNOWN_RESOURCE, OUTSIDE_BLOB, UNMAPPABLE_PAGES, EXECUTOR_REFUSED = range(6)


def wait_for(condition, seconds=10.0):
    deadline = time.monotonic() + seconds
    while not condition():
        if time.monotonic() > deadline:
            return False
        time.sleep(0.001)
    return True


class Context:
    """A context with a guest-backed ring, fence page and, if asked, reply slot."""

    def __init__(self, test, gpu, context, doorbell, reply_bytes=PAGE, options=None, answer=RESP_OK_NODATA):
        """`options` is (sub_api, major, minor, profile, flags, share) for the options minor's setup, whose
        answer is `answer`."""
        self.ring_resource = 200 + 3 * context
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
        if options is None:
            bound = setup(ids[0], ids[1], 12, doorbell)[:-4] + struct.pack("<I", ids[2])
        else:
            bound = setup_with_options(ids[0], ids[1], doorbell, ids[2], *options)
        for request in requests:
            test.assertEqual(hex(response_type(gpu.control(request))), hex(RESP_OK_NODATA))
        test.assertEqual(hex(response_type(gpu.control(submit(context, bound)))), hex(answer))
        self.fence = 0

    def run(self, *commands):
        """One batch of these commands through the doorbell: whether head passed it."""
        self.fence += 1
        self.ring.write(batch_header(len(b"".join(commands)), self.fence) + b"".join(commands)
                        + bytes(-(32 + len(b"".join(commands))) % 8))
        self.ring.publish()
        self.gpu.qtest.write(32, self.gpu.device.shared_memory[DOORBELL_SHM_ID][0] + self.doorbell * PAGE, 1)
        return self.ring.wait_head(self.ring.tail, seconds=2.0)

    def answer(self):
        """(status, fence, size, required) and the bytes that follow."""
        header = struct.unpack("<IIQIIQ", self.gpu.qtest.read_bytes(self.reply, 32))
        self.test.assertEqual(header[0], REPLY_MAGIC)
        return header[1:5], self.gpu.qtest.read_bytes(self.reply + 32, header[3]) if header[3] else b""


def import_body(resource, offset, size, memory_type=0, reserved=0):
    return struct.pack("<IIQQII", resource, reserved, offset, size, memory_type, 0)


def peek_body(handle, offset, count):
    return struct.pack("<QQI", handle, offset, count)


OPTIONS_ASKED = 0x30007
PROFILE_CORE = 1
FLAG_DEBUG = 1


class EchoEnvironment:
    """The echo executor reads its controls from the environment QEMU starts in."""

    def __init__(self, test, **values):
        for name, value in values.items():
            previous = os.environ.get(name)
            os.environ[name] = value
            test.addCleanup(self.restore, name, previous)

    @staticmethod
    def restore(name, previous):
        if previous is None:
            os.environ.pop(name, None)
        else:
            os.environ[name] = previous


@unittest.skipUnless(BUILD and EXECUTOR, "run by the qemu_virtio_gpu ctest")
class VirtioGpuContextOptions(unittest.TestCase):
    """Feature 050: an OpenGL context's options and share context reach the executor library."""

    def open(self, **environment):
        EchoEnvironment(self, RVK_ECHO_OPENGL="1", **environment)
        qtest = virtio_qtest.Qtest(BUILD, rvk_device(), False)
        self.addCleanup(qtest.close)
        return virtio_qtest.VirtioGpu(qtest, WANTED)

    def asked(self, context):
        self.assertTrue(context.run(command(OPTIONS_ASKED, b"", GENERATE_REPLY)))
        (status, _fence, size, _required), answer = context.answer()
        self.assertEqual((status, size), (STATUS_OK, 12))
        return struct.unpack("<BBBBQ", answer)

    def test_a_context_is_made_with_its_options_and_one_shares_it(self):
        gpu = self.open()
        first = Context(self, gpu, 1, 0, options=(SUB_API_OPENGL, 4, 6, PROFILE_CORE, FLAG_DEBUG, 0))
        self.assertEqual(self.asked(first), (4, 6, PROFILE_CORE, FLAG_DEBUG, 0))
        second = Context(self, gpu, 2, 1, options=(SUB_API_OPENGL, 3, 3, PROFILE_CORE, 0, 1))
        self.assertEqual(self.asked(second), (3, 3, PROFILE_CORE, 0, 1))
        plain = Context(self, gpu, 3, 2, options=(SUB_API_OPENGL, 0, 0, 0, 0, 0))
        self.assertEqual(self.asked(plain), (0, 0, 0, 0, 0))

    def test_a_share_that_is_not_another_bound_opengl_context_is_refused(self):
        gpu = self.open()
        Context(self, gpu, 1, 0, options=(SUB_API_VULKAN, 0, 0, 0, 0, 0))
        for context, share in ((2, 1), (3, 9), (4, 4)):
            Context(self, gpu, context, context, options=(SUB_API_OPENGL, 4, 6, PROFILE_CORE, 0, share),
                    answer=RESP_ERR_INVALID_PARAMETER)
        # Options on another sub-API cannot even be written.
        Context(self, gpu, 5, 5, options=(SUB_API_VULKAN, 4, 6, PROFILE_CORE, 0, 0),
                answer=RESP_ERR_INVALID_PARAMETER)

    def test_a_version_3_library_makes_only_default_contexts(self):
        gpu = self.open(RVK_ECHO_ABI_VERSION_3="1")
        Context(self, gpu, 1, 0, options=(SUB_API_OPENGL, 4, 6, PROFILE_CORE, 0, 0),
                answer=RESP_ERR_INVALID_PARAMETER)
        plain = Context(self, gpu, 2, 1, options=(SUB_API_OPENGL, 0, 0, 0, 0, 0))
        self.assertEqual(self.asked(plain), (0, 0, 0, 0, 0))


@unittest.skipUnless(BUILD and EXECUTOR, "run by the qemu_virtio_gpu ctest")
class VirtioGpuExecution(unittest.TestCase):
    def open(self, device=rvk_device(), shared_ram=False):
        qtest = virtio_qtest.Qtest(BUILD, device, shared_ram)
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
        # 0x00007 is the transport id nothing answers: 0x4 to 0x6 became the
        # shared-object commands in feature 053, and a device that answered one
        # of those would be answering rather than skipping.
        self.assertTrue(context.run(command(ECHO, b"1"), command(0x10099), command(0x20001, b"cl"),
                                    command(0x00007), command(ECHO), command(COUNT, b"", GENERATE_REPLY)))
        (status, _, size, _), answer = context.answer()
        self.assertEqual((status, size), (STATUS_OK, 12))
        self.assertEqual(struct.unpack("<QI", answer), (2, 1))
        self.assertTrue(context.run(command(COUNT, b"", GENERATE_REPLY)))
        self.assertEqual(struct.unpack("<QI", context.answer()[1]), (3, 1))

    def test_the_device_answers_a_nop_itself_with_an_empty_reply(self):
        gpu = self.open()
        context = Context(self, gpu, 1, 1)
        self.assertTrue(context.run(command(NOP, b"", GENERATE_REPLY)))
        self.assertEqual(context.answer(), ((STATUS_OK, 1, 0, 0), b""))
        self.assertEqual(gpu.qtest.read_bytes(context.reply + 32, 1), b"\xEE")
        self.assertTrue(context.run(command(NOP), command(NOP), command(COUNT, b"", GENERATE_REPLY)))
        (status, fence, size, _), answer = context.answer()
        self.assertEqual((status, fence, size), (STATUS_OK, 2, 12))
        self.assertEqual(struct.unpack("<QI", answer), (0, 1))   # no NOP ever reached the executor
        self.assertTrue(context.run(command(NOP)))
        self.assertEqual(context.answer()[0][1], 2)   # asking for nothing leaves the slot as it was

    @unittest.skipUnless(REAL_EXECUTOR, "needs this project's executor library")
    def test_a_nop_is_answered_the_same_under_the_real_executor(self):
        gpu = self.open(rvk_device(executor=REAL_EXECUTOR))
        context = Context(self, gpu, 1, 1)
        self.assertTrue(context.run(command(0x0000FFFF, b"skipped"), command(NOP, b"", GENERATE_REPLY)))
        self.assertEqual(context.answer(), ((STATUS_OK, 1, 0, 0), b""))
        self.assertTrue(context.run(command(NOP, b"", GENERATE_REPLY)))
        self.assertEqual(context.answer(), ((STATUS_OK, 2, 0, 0), b""))
        self.assertNotIn("context faulted", gpu.qtest.printed())

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
            "a nop with a body": command(NOP, b"\x00"),
            "a nop with a whole word of body": command(NOP, bytes(8)),
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

    def test_a_fault_aimed_at_one_executor_leaves_the_other_running(self):
        """What the guest suite's tdr and fault runs need: a host that goes
        wrong for one context only, so that losing that context can be told
        apart from losing the device (feature 040, US3 and US4). The executors
        are created in the order their contexts are set up."""
        os.environ["RVK_ECHO_ONLY_NTH"], os.environ["RVK_ECHO_FAULT_AFTER"] = "2", "1"
        try:
            gpu = self.open()
        finally:
            del os.environ["RVK_ECHO_ONLY_NTH"], os.environ["RVK_ECHO_FAULT_AFTER"]
        healthy = Context(self, gpu, 1, 7)
        faulting = Context(self, gpu, 2, 8, reply_bytes=0)
        self.assertFalse(faulting.run(command(ECHO)))
        self.assertTrue(wait_for(lambda: "context faulted" in gpu.qtest.printed()))
        self.assertTrue(healthy.run(command(ECHO, b"unaffected", GENERATE_REPLY)))
        self.assertEqual(healthy.answer()[1], b"unaffected")

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


    def chunk(self, gpu, context, pages):
        """Guest pages allocated with a page between each, written 0xA0 + their
        index, as one GUEST blob in the context's first chunk slot, listed last
        page first: the resource id and the pages' addresses, in blob order."""
        addresses = [gpu.device.memory.allocate(2 * PAGE) for _ in range(pages)][::-1]
        for index, address in enumerate(addresses):
            gpu.qtest.write_bytes(address, bytes([0xA0 + index]) * PAGE)
        resource = context.ring_resource + 3
        self.expect(gpu, guest_blob_pages(resource, addresses), RESP_OK_NODATA)
        return resource, addresses

    def imported(self, context, *import_arguments):
        """(status, device_memory) IMPORT_BLOB answered, the context still running."""
        self.assertTrue(context.run(command(IMPORT_BLOB, import_body(*import_arguments), GENERATE_REPLY)))
        (status, _, size, _), answer = context.answer()
        self.assertEqual((status, size), (STATUS_OK, 16))
        code, reserved, handle = struct.unpack("<IIQ", answer)
        self.assertEqual(reserved, 0)
        return code, handle

    def test_a_guest_chunk_imports_as_one_pointer_over_the_guests_own_pages(self):
        gpu = self.open(shared_ram=True)
        context = Context(self, gpu, 1, 9)
        resource, addresses = self.chunk(gpu, context, 3)
        self.assertEqual(self.imported(context, resource, PAGE, 2 * PAGE, 5), (IMPORTED, 1 | HANDLE_BIT))
        handle = 1 | HANDLE_BIT
        # The executor reads, through the pointer it was handed, what the guest wrote at the second and third pages.
        self.assertTrue(context.run(command(PEEK, peek_body(handle, PAGE - 4, 8), GENERATE_REPLY)))
        memory_type, live, size = struct.unpack("<IIQ", context.answer()[1][:16])
        self.assertEqual((memory_type, live, size), (5, 1, 2 * PAGE))
        self.assertEqual(context.answer()[1][16:], bytes([0xA1]) * 4 + bytes([0xA2]) * 4)
        # And the guest reads what the executor writes, on its own page.
        self.assertTrue(context.run(command(POKE, struct.pack("<QQ", handle, PAGE + 3) + b"host gpu", GENERATE_REPLY)))
        self.assertEqual(context.answer()[1], struct.pack("<I", 1))
        self.assertEqual(gpu.qtest.read_bytes(addresses[2] + 3, 8), b"host gpu")
        # A second import of the same chunk shares its pages; the blob is kept while either holds it.
        self.assertEqual(self.imported(context, resource, 0, PAGE, 1), (IMPORTED, 2 | HANDLE_BIT))
        self.expect(gpu, unref(resource), RESP_ERR_INVALID_PARAMETER)
        self.assertTrue(context.run(command(RELEASE_BLOB, struct.pack("<Q", handle), GENERATE_REPLY)))
        self.assertEqual(context.answer()[0][2], 0)
        self.assertTrue(context.run(command(PEEK, peek_body(handle, 0, 1), GENERATE_REPLY)))
        self.assertEqual(context.answer()[1], bytes(16))   # released: the executor holds nothing under it
        self.assertTrue(context.run(command(RELEASE_BLOB, struct.pack("<Q", 2 | HANDLE_BIT)),
                                    command(RELEASE_BLOB, struct.pack("<Q", 0x1234))))
        self.expect(gpu, unref(resource), RESP_OK_NODATA)
        self.assertNotIn("context faulted", gpu.qtest.printed())

    def host_chunk(self, gpu, context, pages, offset):
        """A mappable HOST3D blob in the context's first chunk slot, placed at `offset` of the hostmem window
        as the driver places a window chunk: the resource id and the guest address of its first byte."""
        resource = context.ring_resource + 3
        self.expect(gpu, host_blob(resource, pages * PAGE), RESP_OK_NODATA)
        self.assertEqual(hex(response_type(gpu.control(map_blob(resource, offset), 32))), hex(RESP_OK_MAP_INFO))
        address, length = gpu.device.shared_memory[SHM_ID_HOST_VISIBLE]
        self.assertEqual(length, HOSTMEM)
        return resource, address + offset

    def host_chunk_imports(self, renderer):
        options = f",hostmem={HOSTMEM}" + (f",renderer={RENDERER}" if renderer else "")
        gpu = self.open(rvk_device(options), shared_ram=True)
        context = Context(self, gpu, 1, 14)
        resource, window = self.host_chunk(gpu, context, 3, 5 * PAGE)
        gpu.qtest.write_bytes(window + PAGE, bytes([0xB1]) * PAGE)
        gpu.qtest.write_bytes(window + 2 * PAGE, bytes([0xB2]) * PAGE)
        self.assertEqual(self.imported(context, resource, PAGE, 2 * PAGE, 3), (IMPORTED, 1 | HANDLE_BIT))
        handle = 1 | HANDLE_BIT
        # The executor reads, through what it was handed, what the guest wrote through the window.
        self.assertTrue(context.run(command(PEEK, peek_body(handle, PAGE - 4, 8), GENERATE_REPLY)))
        memory_type, live, size = struct.unpack("<IIQ", context.answer()[1][:16])
        self.assertEqual((memory_type, live, size), (3, 1, 2 * PAGE))
        self.assertEqual(context.answer()[1][16:], bytes([0xB1]) * 4 + bytes([0xB2]) * 4)
        # And the guest reads, through the window, what the executor writes.
        self.assertTrue(context.run(command(POKE, struct.pack("<QQ", handle, PAGE + 3) + b"host gpu", GENERATE_REPLY)))
        self.assertEqual(context.answer()[1], struct.pack("<I", 1))
        self.assertEqual(gpu.qtest.read_bytes(window + 2 * PAGE + 3, 8), b"host gpu")
        if not renderer:
            # QEMU's table holds the blob for the import; a renderer holds its own mirror instead.
            self.expect(gpu, unref(resource), RESP_ERR_INVALID_PARAMETER)
        self.assertTrue(context.run(command(RELEASE_BLOB, struct.pack("<Q", handle), GENERATE_REPLY)))
        self.assertEqual(context.answer()[0][2], 0)
        self.expect(gpu, header(CMD_RESOURCE_UNMAP_BLOB) + struct.pack("<I4x", resource), RESP_OK_NODATA)
        self.expect(gpu, unref(resource), RESP_OK_NODATA)
        self.assertNotIn("context faulted", gpu.qtest.printed())

    def test_a_host_chunk_in_the_window_imports_over_the_bytes_the_guest_writes_there(self):
        self.host_chunk_imports(renderer=False)

    @unittest.skipUnless(RENDERER, "run by the qemu_virtio_gpu ctest")
    def test_a_host_chunk_in_the_window_imports_in_the_renderer_too(self):
        self.host_chunk_imports(renderer=True)

    def test_every_refused_import_is_an_answer_and_the_context_runs_on(self):
        gpu = self.open(shared_ram=True)
        context = Context(self, gpu, 1, 10)
        resource, _ = self.chunk(gpu, context, 2)
        self.expect(gpu, host_blob(resource + 1, PAGE), RESP_OK_NODATA)
        refusals = {
            "a reserved field": ((resource, 0, PAGE, 0, 1), REFUSED_REQUEST),
            "an offset inside a page": ((resource, 1, PAGE), REFUSED_REQUEST),
            "no size": ((resource, 0, 0), REFUSED_REQUEST),
            "the ring": ((context.ring_resource, 0, PAGE), UNKNOWN_RESOURCE),
            "the reply slot": ((context.ring_resource + 2, 0, PAGE), UNKNOWN_RESOURCE),
            "past the last chunk slot": ((context.ring_resource + 67, 0, PAGE), UNKNOWN_RESOURCE),
            "a page past a host blob in a chunk slot": ((resource + 1, PAGE, PAGE), OUTSIDE_BLOB),
            "past the blob": ((resource, PAGE, 2 * PAGE), OUTSIDE_BLOB),
        }
        for name, (arguments, expected) in refusals.items():
            with self.subTest(name):
                self.assertEqual(self.imported(context, *arguments), (expected, 0))
        self.assertEqual(self.imported(context, resource, 0, 2 * PAGE)[0], IMPORTED)
        self.assertNotIn("context faulted", gpu.qtest.printed())

    def test_blob_commands_framed_with_the_wrong_body_fault_the_context(self):
        for name, framed in {"a short import": command(IMPORT_BLOB, bytes(24), GENERATE_REPLY),
                             "a long release": command(RELEASE_BLOB, bytes(16))}.items():
            with self.subTest(name):
                gpu = self.open(shared_ram=True)
                context = Context(self, gpu, 1, 11)
                self.assertFalse(context.run(framed))
                self.assertTrue(wait_for(lambda: "context faulted" in gpu.qtest.printed()))

    def test_guest_ram_with_no_file_behind_it_cannot_be_imported(self):
        gpu = self.open()
        context = Context(self, gpu, 1, 12)
        resource, _ = self.chunk(gpu, context, 1)
        self.assertEqual(self.imported(context, resource, 0, PAGE), (UNMAPPABLE_PAGES, 0))
        self.expect(gpu, unref(resource), RESP_OK_NODATA)   # a refused import holds nothing

    def test_a_version_1_library_is_refused_the_import_and_runs_on(self):
        os.environ["RVK_ECHO_ABI_VERSION_1"] = "1"
        try:
            gpu = self.open(shared_ram=True)
        finally:
            del os.environ["RVK_ECHO_ABI_VERSION_1"]
        context = Context(self, gpu, 1, 13)
        resource, _ = self.chunk(gpu, context, 1)
        self.assertEqual(self.imported(context, resource, 0, PAGE), (EXECUTOR_REFUSED, 0))
        self.assertTrue(context.run(command(ECHO, b"still", GENERATE_REPLY)))
        self.assertEqual(context.answer()[1], b"still")


if __name__ == "__main__":
    unittest.main()
