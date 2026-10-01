"""The device with its contexts in a renderer process (docs/renderer.md): the
echo executor runs in av-renderer, not in QEMU. A renderer that crashes - the
executor aborting, or the process killed from outside - takes its contexts
with it: each bound context's fence page reads faulted with device_lost and its
fence queue interrupts, QEMU goes on answering, and a new renderer runs the
next context. A guest whose RAM is no shared memfd cannot have one.

Run by the qemu_virtio_gpu ctest in the qemu configuration, which sets
AV_QEMU_BUILD, AV_EXECUTOR and AV_RENDERER.
"""
import os
import signal
import subprocess
import time
import unittest

import virtio_qtest
from av_context import ECHO, command, av_device
from test_virtio_gpu_faults import DEVICE_LOST, FENCE_QUEUE_FIRST, WANTED, Context, wait_for

BUILD = os.environ.get("AV_QEMU_BUILD", "")
EXECUTOR = os.environ.get("AV_EXECUTOR", "")
RENDERER = os.environ.get("AV_RENDERER", "")
CRASH = 0x10007


def renderers_of(pid):
    """The renderer processes QEMU `pid` started."""
    listing = subprocess.run(["pgrep", "-P", str(pid), "-f", "av-renderer"], capture_output=True, text=True)
    return [int(line) for line in listing.stdout.split()]


@unittest.skipUnless(BUILD and EXECUTOR and RENDERER, "run by the qemu_virtio_gpu ctest")
class VirtioGpuRenderer(unittest.TestCase):
    def gpu(self, vectors=None):
        qtest = virtio_qtest.Qtest(BUILD, av_device(f",vectors=4,renderer={RENDERER}"), shared_ram=True)
        self.addCleanup(qtest.close)
        msix = {}

        def configure(device):
            msix["table"] = virtio_qtest.Msix(qtest, device.pci)
            msix["table"].enable()
            for doorbell, vector in (vectors or {}).items():
                device.set_queue_vector(FENCE_QUEUE_FIRST + doorbell, vector)

        return virtio_qtest.VirtioGpu(qtest, WANTED, configure), msix["table"]

    def setUp(self):
        os.environ["AV_ECHO_ALLOW_CRASH"] = "1"
        self.addCleanup(os.environ.pop, "AV_ECHO_ALLOW_CRASH", None)

    def test_a_context_runs_in_the_renderer(self):
        gpu, _ = self.gpu()
        self.assertEqual(len(renderers_of(gpu.qtest.process.pid)), 1)
        context = Context(self, gpu, 1, 1)
        self.assertTrue(context.completed(context.send(command(ECHO, b"\x01\x02\x03\x04"))))

    def lost_and_recovered(self, gpu, msix, context, end):
        """`end` ends the renderer; the context is lost, QEMU answers, and a
        context made after it runs in a renderer that is not the first."""
        first = renderers_of(gpu.qtest.process.pid)
        self.assertTrue(context.completed(context.send(command(ECHO))))
        self.assertTrue(wait_for(lambda: msix.pending(3)))
        msix.mask(3, False)
        self.assertTrue(wait_for(lambda: not msix.pending(3)))
        msix.mask(3)
        end(context)
        self.assertTrue(context.faulted_with(DEVICE_LOST), context.fence_page())
        # The completed fence stays where it was: the guest never sees one go back.
        self.assertEqual(context.fence_page()[2], 1)
        self.assertTrue(wait_for(lambda: msix.pending(3)))
        self.assertTrue(wait_for(lambda: len(renderers_of(gpu.qtest.process.pid)) == 1))
        self.assertNotEqual(renderers_of(gpu.qtest.process.pid), first)
        context.destroy()
        second = Context(self, gpu, 2, 2)
        self.assertTrue(second.completed(second.send(command(ECHO))))

    def test_an_executor_crash_loses_the_contexts_and_a_new_renderer_runs_the_next(self):
        gpu, msix = self.gpu({1: 3})
        context = Context(self, gpu, 1, 1)
        self.lost_and_recovered(gpu, msix, context, lambda c: c.send(command(CRASH)))
        self.assertIn("renderer", gpu.qtest.printed())

    def test_a_renderer_killed_from_outside_is_the_same_loss(self):
        gpu, msix = self.gpu({1: 3})
        context = Context(self, gpu, 1, 1)

        def kill(_):
            for pid in renderers_of(gpu.qtest.process.pid):
                os.kill(pid, signal.SIGKILL)

        self.lost_and_recovered(gpu, msix, context, kill)

    def test_guest_ram_that_is_not_a_shared_memfd_has_no_renderer(self):
        qtest = virtio_qtest.Qtest(BUILD, av_device(f",renderer={RENDERER}"), shared_ram=False)
        deadline = time.monotonic() + 30.0
        while qtest.process.poll() is None and time.monotonic() < deadline:
            time.sleep(0.05)
        printed = qtest.close()
        self.assertIsNotNone(qtest.process.returncode)
        self.assertIn("renderer", printed)


if __name__ == "__main__":
    unittest.main()
