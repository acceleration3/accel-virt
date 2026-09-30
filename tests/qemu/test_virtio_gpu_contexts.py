"""The rvk capset and context lifetime against the real device: the capset
describes the host GPU the device is bound to, contexts are created only for the
rvk capset, a context setup binds a ring and a fence blob that then cannot be
unreferenced, and a reset forgets every context before every blob.

Run by the qemu_virtio_gpu ctest in the qemu configuration, which sets
AV_QEMU_BUILD.
"""
import os
import pathlib
import struct
import subprocess
import sys
import unittest

import rvk_context
import virtio_qtest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools" / "host"))
import list_gpus  # noqa: E402

BUILD = os.environ.get("AV_QEMU_BUILD", "")

CMD_RESOURCE_UNREF = 0x0102
CMD_GET_CAPSET_INFO = 0x0108
CMD_GET_CAPSET = 0x0109
CMD_RESOURCE_CREATE_BLOB = 0x010C
CMD_CTX_CREATE = 0x0200
CMD_CTX_DESTROY = 0x0201
CMD_SUBMIT_3D = 0x0207
RESP_OK_NODATA = 0x1100
RESP_OK_CAPSET_INFO = 0x1102
RESP_OK_CAPSET = 0x1103
RESP_ERR_OUT_OF_MEMORY = 0x1201
RESP_ERR_INVALID_RESOURCE_ID = 0x1203
RESP_ERR_INVALID_CONTEXT_ID = 0x1204
RESP_ERR_INVALID_PARAMETER = 0x1205
FEATURE_CONTEXT_INIT = 1 << 4
BLOB_MEM_GUEST = 1
BLOB_MEM_HOST3D = 2
BLOB_FLAG_MAPPABLE = 1
CONFIG_NUM_CAPSETS = 12

CAPSET_ID = 0x72
CAPSET_VERSION = 1
CAPSET_SIZE = 128
CAPSET_MAGIC = 0x314B5652
SETUP_MAGIC = 0x434B5652
SUB_API_VULKAN = 1
SUB_API_OPENGL = 2
MAX_CONTEXTS = 64
FEATURE_HOST_VISIBLE_MEMORY = 1
FEATURE_GPU_EXPORTED_MEMORY = 2
FEATURE_GUEST_MEMORY_IMPORT = 4
PROBED_FEATURES = FEATURE_GPU_EXPORTED_MEMORY | FEATURE_GUEST_MEMORY_IMPORT
"""Bits feature 039's probes decide on the host the test runs on: this suite
says what does not depend on the host, and the probe tests say the rest."""
# The device's own, offered on every host (docs/protocol.md, "Present engine").
FEATURE_PRESENT_ENGINE = 16
DEVICE_TYPES = {"other": 0, "integrated": 1, "discrete": 2, "virtual": 3, "cpu": 4}

HOSTMEM = 64 << 20
PAGE = 0x1000
RING, FENCE, SPARE = 20, 21, 22


def header(command, context=0):
    return struct.pack("<IIQI4x", command, 0, 0, context)


def response_type(response):
    return struct.unpack_from("<I", response)[0]


def create_context(context, context_init=CAPSET_ID, name=b"vulkan"):
    padded = name.ljust(64, b"\0")[:64]
    return header(CMD_CTX_CREATE, context) + struct.pack("<II", len(name), context_init) + padded


def setup(ring=RING, fence=FENCE, order=12, major=1, minor=0, sub_api=SUB_API_VULKAN):
    return struct.pack("<IHHIIB3xIQ", SETUP_MAGIC, major, minor, sub_api, ring, order, fence, 0)


def submit(context, command, size=None):
    return header(CMD_SUBMIT_3D, context) + struct.pack("<I4x", len(command) if size is None else size) + command


def host_blob(resource, size):
    return header(CMD_RESOURCE_CREATE_BLOB) + struct.pack("<IIIIQQ", resource, BLOB_MEM_HOST3D,
                                                          BLOB_FLAG_MAPPABLE, 0, 0, size)


def unref(resource):
    return header(CMD_RESOURCE_UNREF) + struct.pack("<I4x", resource)


def last_host_gpu():
    """The host's last Vulkan device, so binding by UUID is what is exercised, not auto."""
    text = subprocess.run(["vulkaninfo"], capture_output=True, text=True, check=True).stdout
    return list_gpus.parse(text)[-1]


@unittest.skipUnless(BUILD, "run by the qemu_virtio_gpu ctest")
class VirtioGpuContexts(unittest.TestCase):
    def open(self, device=rvk_context.rvk_device(f",hostmem={HOSTMEM}")):
        qtest = virtio_qtest.Qtest(BUILD, device)
        self.addCleanup(qtest.close)
        return virtio_qtest.VirtioGpu(qtest, virtio_qtest.FEATURE_VERSION_1 | virtio_qtest.FEATURE_EDID |
                                      FEATURE_CONTEXT_INIT)

    def expect(self, gpu, request, answer, response_size=24):
        self.assertEqual(hex(response_type(gpu.control(request, response_size))), hex(answer), request[:32].hex())

    def bindable(self, gpu, context=1):
        """A context, a guest-backed ring with a fresh header and a fence blob it can be set up with."""
        self.expect(gpu, create_context(context), RESP_OK_NODATA)
        address = gpu.device.memory.allocate(2 * PAGE)
        rvk_context.GuestRing(gpu.qtest, address)
        self.expect(gpu, rvk_context.guest_blob(RING, address, 2 * PAGE), RESP_OK_NODATA)
        self.expect(gpu, host_blob(FENCE, PAGE), RESP_OK_NODATA)

    def test_without_an_executor_there_is_no_capset_and_no_context(self):
        qtest = virtio_qtest.Qtest(BUILD, "virtio-gpu-rvk-pci,host-gpu=auto")
        self.addCleanup(qtest.close)
        gpu = virtio_qtest.VirtioGpu(qtest, virtio_qtest.FEATURE_VERSION_1 | FEATURE_CONTEXT_INIT)
        self.assertFalse(gpu.features & FEATURE_CONTEXT_INIT)
        self.assertEqual(gpu.device.device_config(32, CONFIG_NUM_CAPSETS), 0)
        self.expect(gpu, header(CMD_GET_CAPSET_INFO) + struct.pack("<II", 0, 0), RESP_ERR_INVALID_PARAMETER, 40)
        self.expect(gpu, create_context(1), RESP_ERR_INVALID_PARAMETER)

    def test_context_init_and_one_capset_are_offered_and_accepted(self):
        stock_qtest = virtio_qtest.Qtest(BUILD, "virtio-gpu-pci")
        self.addCleanup(stock_qtest.close)
        stock = virtio_qtest.VirtioGpu(stock_qtest)
        rvk = self.open()
        self.assertTrue(rvk.features & FEATURE_CONTEXT_INIT)
        self.assertEqual(rvk.device.device_config(32, CONFIG_NUM_CAPSETS), 1)
        self.assertEqual(stock.device.device_config(32, CONFIG_NUM_CAPSETS), 0)

    def test_capset_info_describes_the_one_rvk_capset(self):
        gpu = self.open()
        response = gpu.control(header(CMD_GET_CAPSET_INFO) + struct.pack("<II", 0, 0), 40)
        self.assertEqual(hex(response_type(response)), hex(RESP_OK_CAPSET_INFO))
        self.assertEqual(struct.unpack_from("<III", response, 24), (CAPSET_ID, CAPSET_VERSION, CAPSET_SIZE))
        for request in (header(CMD_GET_CAPSET_INFO) + struct.pack("<II", 1, 0),
                        header(CMD_GET_CAPSET_INFO) + struct.pack("<II", 0xFFFFFFFF, 0),
                        header(CMD_GET_CAPSET_INFO) + b"\0\0"):
            self.expect(gpu, request, RESP_ERR_INVALID_PARAMETER, 40)

    def capset(self, gpu):
        response = gpu.control(header(CMD_GET_CAPSET) + struct.pack("<II", CAPSET_ID, CAPSET_VERSION),
                               24 + CAPSET_SIZE)
        self.assertEqual(hex(response_type(response)), hex(RESP_OK_CAPSET))
        self.assertEqual(len(response), 24 + CAPSET_SIZE)
        return response[24:]

    def test_the_capset_describes_the_host_gpu_the_device_is_bound_to(self):
        host = last_host_gpu()
        gpu = self.open(f"virtio-gpu-rvk-pci,host-gpu={host.uuid},executor={rvk_context.EXECUTOR},hostmem={HOSTMEM}")
        capset = self.capset(gpu)
        magic, major, minor, sub_apis, contexts, min_order, max_order = struct.unpack_from("<IHHIIBB", capset)
        self.assertEqual((magic, major, minor), (CAPSET_MAGIC, 1, 1))
        self.assertEqual((sub_apis, contexts, min_order, max_order), (SUB_API_VULKAN, MAX_CONTEXTS, 12, 24))
        self.assertEqual(capset[18:24], bytes(6))
        features = struct.unpack_from("<Q", capset, 24)[0]
        self.assertEqual(features & ~PROBED_FEATURES, FEATURE_HOST_VISIBLE_MEMORY | FEATURE_PRESENT_ENGINE)
        vendor, device, device_type, reserved = struct.unpack_from("<IIII", capset, 32)
        self.assertEqual((vendor, device, device_type, reserved), (host.vendor, host.device,
                                                                   DEVICE_TYPES[host.kind], 0))
        self.assertEqual(capset[48:64], bytes.fromhex(host.uuid.replace("-", "")))
        local_memory, shared_memory = struct.unpack_from("<QQ", capset, 64)
        self.assertEqual(local_memory, host.local_memory)
        self.assertGreaterEqual(shared_memory, 0)
        name = capset[80:128]
        self.assertEqual(name.rstrip(b"\0"), host.adapter_name().encode())
        self.assertEqual(name[len(name.rstrip(b"\0")):], bytes(48 - len(name.rstrip(b"\0"))))

    def test_host_visible_memory_is_offered_only_with_a_hostmem_region(self):
        gpu = self.open(rvk_context.rvk_device())
        features = struct.unpack_from("<Q", self.capset(gpu), 24)[0]
        self.assertEqual(features & ~PROBED_FEATURES, FEATURE_PRESENT_ENGINE)
        # Nothing can be exported into a region that is not there.
        self.assertEqual(features & FEATURE_GPU_EXPORTED_MEMORY, 0)

    def test_get_capset_for_another_id_or_version_is_refused(self):
        gpu = self.open()
        for capset_id, version in ((CAPSET_ID, 0), (CAPSET_ID, 2), (4, 1), (0, 0)):
            self.expect(gpu, header(CMD_GET_CAPSET) + struct.pack("<II", capset_id, version),
                        RESP_ERR_INVALID_PARAMETER, 24 + CAPSET_SIZE)
        self.expect(gpu, header(CMD_GET_CAPSET) + b"\x72", RESP_ERR_INVALID_PARAMETER, 24 + CAPSET_SIZE)

    def test_contexts_are_created_for_the_rvk_capset_only(self):
        gpu = self.open()
        self.expect(gpu, create_context(1), RESP_OK_NODATA)
        self.expect(gpu, create_context(2, name=b"x" * 64), RESP_OK_NODATA)
        refused = (
            (create_context(0), RESP_ERR_INVALID_CONTEXT_ID),
            (create_context(1), RESP_ERR_INVALID_CONTEXT_ID),
            (create_context(3, context_init=4), RESP_ERR_INVALID_PARAMETER),
            (create_context(3, context_init=0), RESP_ERR_INVALID_PARAMETER),
            (create_context(3, context_init=CAPSET_ID | 0x100), RESP_ERR_INVALID_PARAMETER),
            (header(CMD_CTX_CREATE, 3) + struct.pack("<II", 65, CAPSET_ID) + bytes(64), RESP_ERR_INVALID_PARAMETER),
            (create_context(3)[:40], RESP_ERR_INVALID_PARAMETER),
        )
        for request, answer in refused:
            self.expect(gpu, request, answer)
        self.expect(gpu, header(CMD_CTX_DESTROY, 1), RESP_OK_NODATA)
        self.expect(gpu, header(CMD_CTX_DESTROY, 1), RESP_ERR_INVALID_CONTEXT_ID)
        self.expect(gpu, header(CMD_CTX_DESTROY, 0), RESP_ERR_INVALID_CONTEXT_ID)
        self.expect(gpu, create_context(1), RESP_OK_NODATA)

    def test_every_context_slot_taken_refuses_the_next_context(self):
        gpu = self.open()
        for context in range(1, MAX_CONTEXTS + 1):
            self.expect(gpu, create_context(context), RESP_OK_NODATA)
        self.expect(gpu, create_context(MAX_CONTEXTS + 1), RESP_ERR_OUT_OF_MEMORY)
        self.expect(gpu, header(CMD_CTX_DESTROY, 7), RESP_OK_NODATA)
        self.expect(gpu, create_context(MAX_CONTEXTS + 1), RESP_OK_NODATA)

    def test_a_bound_context_keeps_its_blobs_until_it_is_destroyed(self):
        gpu = self.open()
        self.bindable(gpu)
        # A later minor carries at least the options minor's 48 bytes.
        self.expect(gpu, submit(1, setup(minor=7) + bytes(16)), RESP_OK_NODATA)
        self.expect(gpu, unref(RING), RESP_ERR_INVALID_PARAMETER)
        self.expect(gpu, unref(FENCE), RESP_ERR_INVALID_PARAMETER)
        self.expect(gpu, submit(1, setup()), RESP_ERR_INVALID_PARAMETER)
        self.expect(gpu, header(CMD_CTX_DESTROY, 1), RESP_OK_NODATA)
        self.expect(gpu, unref(RING), RESP_OK_NODATA)
        self.expect(gpu, unref(FENCE), RESP_OK_NODATA)

    def test_setups_the_capset_or_the_blobs_do_not_allow_leave_the_context_unbound(self):
        gpu = self.open()
        self.bindable(gpu)
        self.expect(gpu, host_blob(SPARE, PAGE), RESP_OK_NODATA)
        good = setup()
        corrupted = bytearray(good)
        corrupted[0] ^= 0xFF
        refused = (
            (submit(1, setup(major=2)), RESP_ERR_INVALID_PARAMETER),
            (submit(1, setup(sub_api=SUB_API_OPENGL)), RESP_ERR_INVALID_PARAMETER),
            (submit(1, setup(sub_api=SUB_API_VULKAN | SUB_API_OPENGL)), RESP_ERR_INVALID_PARAMETER),
            (submit(1, setup(order=11)), RESP_ERR_INVALID_PARAMETER),
            (submit(1, setup(order=25)), RESP_ERR_INVALID_PARAMETER),
            (submit(1, setup(order=13)), RESP_ERR_INVALID_PARAMETER),          # 32 + 8192 > two pages
            (submit(1, setup(ring=SPARE)), RESP_ERR_INVALID_PARAMETER),        # 32 + 4096 > one page
            (submit(1, setup(ring=99)), RESP_ERR_INVALID_RESOURCE_ID),
            (submit(1, setup(fence=99)), RESP_ERR_INVALID_RESOURCE_ID),
            (submit(1, setup(fence=RING)), RESP_ERR_INVALID_PARAMETER),
            (submit(1, bytes(corrupted)), RESP_ERR_INVALID_PARAMETER),
            (submit(1, good, size=31), RESP_ERR_INVALID_PARAMETER),
            (submit(1, good, size=33), RESP_ERR_INVALID_PARAMETER),
            (submit(1, good[:16]), RESP_ERR_INVALID_PARAMETER),
            (submit(2, good), RESP_ERR_INVALID_CONTEXT_ID),
            (submit(0, good), RESP_ERR_INVALID_CONTEXT_ID),
            (header(CMD_SUBMIT_3D, 1) + b"\x20", RESP_ERR_INVALID_PARAMETER),
        )
        for request, answer in refused:
            self.expect(gpu, request, answer)
            self.expect(gpu, unref(SPARE), RESP_OK_NODATA)
            self.expect(gpu, host_blob(SPARE, PAGE), RESP_OK_NODATA)
        self.expect(gpu, submit(1, good), RESP_OK_NODATA)

    def test_a_reset_destroys_every_context_before_releasing_its_blobs(self):
        gpu = self.open()
        self.bindable(gpu, context=1)
        self.expect(gpu, submit(1, setup()), RESP_OK_NODATA)
        self.expect(gpu, create_context(2), RESP_OK_NODATA)
        again = virtio_qtest.VirtioGpu(gpu.qtest, gpu.features)   # a new driver resets the device first
        self.expect(again, header(CMD_CTX_DESTROY, 1), RESP_ERR_INVALID_CONTEXT_ID)
        self.expect(again, header(CMD_CTX_DESTROY, 2), RESP_ERR_INVALID_CONTEXT_ID)
        self.expect(again, unref(RING), RESP_ERR_INVALID_RESOURCE_ID)
        self.bindable(again, context=1)
        self.expect(again, submit(1, setup()), RESP_OK_NODATA)


if __name__ == "__main__":
    unittest.main()
