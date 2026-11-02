import array
import mmap
import os
import select
import socket
import stat
import struct
import tempfile
import time
import unittest

import virtio_qtest
from av_context import PAGE, REAL_EXECUTOR, av_device, header, response_type

BUILD = os.environ.get("AV_QEMU_BUILD", "")
RENDERER = os.environ.get("AV_RENDERER", "")
SANDBOX = ("-sandbox", "on,obsolete=deny,elevateprivileges=deny,spawn=deny,resourcecontrol=deny")

REQUEST_MAGIC = 0x51585641
ANSWER_MAGIC = 0x41585641
UNREADABLE, HELLO, EXPORT, SYNC, RELEASE, TIMING, REVOKED = 0, 1, 2, 3, 4, 5, 0x80
OK, MALFORMED, NOT_SUPPORTED, NOT_FOUND, NO_SPACE, UNAVAILABLE = range(6)
RELEASED, RESET = 1, 2
CMD_SHARED_RELEASE = 0x02F1
CMD_DISPLAY_CLOCK = 0x02F4
RESP_OK_NODATA = 0x1100
DRM_ARGB8888 = 0x34325241


def request(operation, surface=0, tag=0, major=1):
    return struct.pack("<IHHIIQQ", REQUEST_MAGIC, major, 0, operation, 0, surface, tag)


def timing(shift_ns, tag):
    return struct.pack("<IHHIIqQ", REQUEST_MAGIC, 1, 5, TIMING, 0, shift_ns, tag)


class Answer:
    def __init__(self, data, descriptors):
        self.length = len(data)
        (self.magic, self.major, self.minor, self.operation, self.status, self.surface,
         self.tag) = struct.unpack_from("<IHHIIQQ", data)
        self.body = data[32:]
        self.descriptors = descriptors


class Consumer:
    def __init__(self, path):
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        self.socket.connect(path)

    def close(self):
        self.socket.close()

    def receive(self, seconds=10.0):
        ready, _, _ = select.select([self.socket], [], [], seconds)
        if not ready:
            return None
        fds = array.array("i")
        data, ancillary, _, _ = self.socket.recvmsg(128, socket.CMSG_SPACE(4 * fds.itemsize))
        for level, kind, payload in ancillary:
            if level == socket.SOL_SOCKET and kind == socket.SCM_RIGHTS:
                fds.frombytes(payload[:len(payload) - len(payload) % fds.itemsize])
        return Answer(data, list(fds)) if data else b""

    def ask(self, operation, surface=0, tag=0):
        self.socket.send(request(operation, surface, tag))
        return self.receive()


def wait_for(condition, seconds=10.0):
    deadline = time.monotonic() + seconds
    while not condition():
        if time.monotonic() > deadline:
            return False
        time.sleep(0.01)
    return True


@unittest.skipUnless(BUILD, "run by the qemu_virtio_gpu ctest")
class VirtioGpuExport(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.path = os.path.join(directory.name, "export.sock")

    def start(self, path=None, options="", extra=SANDBOX, shared_ram=False):
        qtest = virtio_qtest.Qtest(BUILD, av_device(f",export-socket={path or self.path}{options}"), extra=extra,
                                   shared_ram=shared_ram)
        self.addCleanup(qtest.close)
        return virtio_qtest.VirtioGpu(qtest)

    def consumer(self):
        consumer = Consumer(self.path)
        self.addCleanup(consumer.close)
        return consumer

    def expect(self, answer, operation, status, surface=0, tag=0, descriptors=0):
        self.assertIsInstance(answer, Answer)
        self.assertEqual(answer.length, 64)
        self.assertEqual((answer.magic, answer.major), (ANSWER_MAGIC, 1))
        self.assertEqual((answer.operation, answer.status, answer.surface, answer.tag),
                         (operation, status, surface, tag))
        self.assertEqual(len(answer.descriptors), descriptors)
        self.addCleanup(lambda: [os.close(fd) for fd in answer.descriptors])

    def release(self, gpu, surface):
        request_bytes = header(CMD_SHARED_RELEASE) + struct.pack("<Q", surface)
        self.assertEqual(hex(response_type(gpu.control(request_bytes))), hex(RESP_OK_NODATA))

    def test_the_socket_is_the_user_s_alone(self):
        self.start()
        mode = os.stat(self.path).st_mode
        self.assertTrue(stat.S_ISSOCK(mode))
        self.assertEqual(stat.S_IMODE(mode), 0o600)

    def test_hello_names_the_bound_gpu(self):
        self.start()
        answer = self.consumer().ask(HELLO, 0, 7)
        self.expect(answer, HELLO, OK, tag=7)
        uuid, domain, bus, device, function, flags, reserved = struct.unpack_from("<16sIBBBBQ", answer.body)
        self.assertNotEqual(uuid, bytes(16))
        self.assertEqual(reserved, 0)
        self.assertIn(flags, (0, 1))
        if flags == 0:
            self.assertEqual((domain, bus, device, function), (0, 0, 0, 0))

    def test_an_exported_surface_comes_with_its_dma_buf_and_a_sync_file(self):
        self.start()
        consumer = self.consumer()
        answer = consumer.ask(SYNC, 5, 1)
        self.expect(answer, SYNC, NOT_FOUND, 5, 1)
        answer = consumer.ask(EXPORT, 5, 2)
        self.expect(answer, EXPORT, OK, 5, 2, descriptors=1)
        width, height, fourcc, stride, offset, reserved, modifier = struct.unpack_from("<IIIIIIQ", answer.body)
        self.assertEqual((width, height, fourcc, stride, offset, reserved, modifier),
                         (64, 64, DRM_ARGB8888, 256, 0, 0, 0))
        with mmap.mmap(answer.descriptors[0], 64 * 256, prot=mmap.PROT_READ) as pixels:
            self.assertEqual(struct.unpack_from("<Q", pixels)[0], 5)
        answer = consumer.ask(SYNC, 5, 3)
        self.expect(answer, SYNC, OK, 5, 3, descriptors=1)
        self.assertEqual(struct.unpack_from("<Q", answer.body)[0], 10)
        self.assertEqual(answer.body[8:], bytes(24))
        self.assertTrue(select.select([answer.descriptors[0]], [], [], 5)[0])
        self.expect(consumer.ask(RELEASE, 5, 4), RELEASE, OK, 5, 4)
        self.expect(consumer.ask(SYNC, 5, 5), SYNC, NOT_FOUND, 5, 5)

    def test_what_cannot_be_read_is_answered_and_the_connection_goes_on(self):
        self.start()
        consumer = self.consumer()
        consumer.socket.send(request(EXPORT, 5)[:31])
        self.expect(consumer.receive(), UNREADABLE, MALFORMED)
        consumer.socket.send(request(EXPORT, 5, major=2))
        self.expect(consumer.receive(), UNREADABLE, NOT_SUPPORTED)
        consumer.socket.send(request(HELLO, 9))
        self.expect(consumer.receive(), UNREADABLE, MALFORMED)
        self.expect(consumer.ask(HELLO, 0, 1), HELLO, OK, tag=1)

    def test_a_timing_shifts_the_guest_s_display_clock_and_keeps_its_rate(self):
        gpu = self.start()
        page = gpu.device.memory.allocate(PAGE)
        answer = gpu.control(header(CMD_DISPLAY_CLOCK) + struct.pack("<Q", page))
        self.assertEqual(hex(response_type(answer)), hex(RESP_OK_NODATA))
        consumer = self.consumer()
        consumer.socket.send(timing(-12345, 9))
        self.expect(consumer.receive(), TIMING, OK, tag=9)
        time.sleep(0.2)

        def head_record():
            record = page + 32
            for _ in range(1000):
                before = struct.unpack("<I", gpu.qtest.read_bytes(record, 4))[0]
                fields = gpu.qtest.read_bytes(record + 4, 32)
                after = struct.unpack("<I", gpu.qtest.read_bytes(record, 4))[0]
                if before == after and before % 2 == 0:
                    break
            return struct.unpack("<IQQIII", fields)

        _, sequence, vblank, period, refresh, _ = head_record()
        self.assertEqual((refresh, period), (60000, 16666667))
        time.sleep(0.2)
        _, later_sequence, later_vblank, _, _, _ = head_record()
        blanks = later_sequence - sequence
        self.assertGreater(blanks, 0)
        self.assertLess(abs((later_vblank - vblank) - blanks * 1e9 / 60.0), 1000)

    def test_the_guest_s_release_and_a_reset_reach_the_consumer(self):
        gpu = self.start()
        consumer = self.consumer()
        self.expect(consumer.ask(EXPORT, 5, 1), EXPORT, OK, 5, 1, descriptors=1)
        self.expect(consumer.ask(EXPORT, 6, 2), EXPORT, OK, 6, 2, descriptors=1)
        self.release(gpu, 9)
        self.assertIsNone(consumer.receive(0.2))
        self.release(gpu, 5)
        answer = consumer.receive()
        self.expect(answer, REVOKED, OK, 5)
        self.assertEqual(struct.unpack_from("<I", answer.body)[0], RELEASED)
        self.expect(consumer.ask(EXPORT, 5, 3), EXPORT, NOT_FOUND, 5, 3)
        gpu.device.status(0)
        answer = consumer.receive()
        self.expect(answer, REVOKED, OK, 0)
        self.assertEqual(struct.unpack_from("<I", answer.body)[0], RESET)
        self.expect(consumer.ask(SYNC, 6, 4), SYNC, NOT_FOUND, 6, 4)
        self.expect(consumer.ask(EXPORT, 5, 5), EXPORT, OK, 5, 5, descriptors=1)

    def test_eight_consumers_are_served_and_a_ninth_is_closed(self):
        self.start()
        consumers = [self.consumer() for _ in range(8)]
        for index, consumer in enumerate(consumers):
            self.expect(consumer.ask(HELLO, 0, index), HELLO, OK, tag=index)
        ninth = self.consumer()
        self.assertEqual(ninth.receive(), b"")
        consumers[0].close()
        tenth = None

        def served():
            nonlocal tenth
            tenth = self.consumer()
            tenth.socket.send(request(HELLO, 0, 10))
            answer = tenth.receive(2.0)
            return isinstance(answer, Answer) and answer.status == OK
        self.assertTrue(wait_for(served))

    def test_a_dead_qemu_s_socket_is_replaced_and_a_live_one_s_is_not(self):
        first = virtio_qtest.Qtest(BUILD, av_device(f",export-socket={self.path}"), extra=SANDBOX)
        virtio_qtest.VirtioGpu(first)
        first.close()
        self.assertTrue(stat.S_ISSOCK(os.stat(self.path).st_mode))
        self.start()
        self.expect(self.consumer().ask(HELLO, 0, 1), HELLO, OK, tag=1)
        rival = virtio_qtest.Qtest(BUILD, av_device(f",export-socket={self.path}"), extra=SANDBOX)
        self.assertTrue(wait_for(lambda: rival.process.poll() is not None))
        self.assertIn("another process's, which is listening on it", rival.close())
        self.expect(self.consumer().ask(HELLO, 0, 2), HELLO, OK, tag=2)

    def test_a_path_that_is_no_socket_is_refused(self):
        with open(self.path, "w") as placeholder:
            placeholder.write("not a socket")
        qtest = virtio_qtest.Qtest(BUILD, av_device(f",export-socket={self.path}"), extra=SANDBOX)
        self.assertTrue(wait_for(lambda: qtest.process.poll() is not None))
        self.assertIn("exists and is not a socket", qtest.close())
        with open(self.path) as placeholder:
            self.assertEqual(placeholder.read(), "not a socket")

    @unittest.skipUnless(REAL_EXECUTOR, "this project's executor is built with the qemu configuration")
    def test_this_project_s_executor_exports_through_the_socket(self):
        qtest = virtio_qtest.Qtest(BUILD, av_device(f",export-socket={self.path}", executor=REAL_EXECUTOR),
                                   extra=SANDBOX)
        self.addCleanup(qtest.close)
        virtio_qtest.VirtioGpu(qtest)
        consumer = self.consumer()
        self.expect(consumer.ask(HELLO, 0, 1), HELLO, OK, tag=1)
        self.expect(consumer.ask(EXPORT, 0x51, 2), EXPORT, NOT_FOUND, 0x51, 2)

    @unittest.skipUnless(RENDERER, "the renderer is built with the qemu configuration")
    def test_a_device_whose_contexts_run_in_a_renderer_exports_nothing(self):
        self.start(options=f",renderer={RENDERER}", extra=(), shared_ram=True)
        consumer = self.consumer()
        self.expect(consumer.ask(HELLO, 0, 1), HELLO, UNAVAILABLE, tag=1)
        self.expect(consumer.ask(EXPORT, 5, 2), EXPORT, UNAVAILABLE, 5, 2)


if __name__ == "__main__":
    unittest.main()
