import pathlib
import re
import shutil
import sys
import tempfile
import unittest

from tool_run import ROOT

sys.path.insert(0, str(ROOT / "tools" / "codegen"))

from av_codegen.emit import client_dispatch  # noqa: E402
from av_codegen.ir import Command, CType, Param, Registry  # noqa: E402
from av_codegen.opencl import emit as opencl_emit  # noqa: E402
from av_codegen.opencl import model as opencl_model  # noqa: E402

SCOPE = "const ::av::SyncCallScope av_call{t};"


def vk_param(name, base, pointer_depth=0, is_const=False, direction="in"):
    return Param(name=name, type=CType(base=base, pointer_depth=pointer_depth, is_const=is_const),
                 direction=direction)


def function_body(source, name):
    match = re.search(r"\bav_" + name + r"\(.*?\) \{\n(.*?)\n\}\n", source, re.S)
    assert match is not None, name
    return match.group(1)


class VulkanClientEndsSyncCalls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.out = pathlib.Path(tempfile.mkdtemp())
        registry = Registry()
        registry.commands["vkGetFenceStatus"] = Command(
            name="vkGetFenceStatus", return_type=CType(base="VkResult"), cmd_id=1,
            params=[vk_param("device", "uint32_t"), vk_param("fence", "uint64_t")])
        registry.commands["vkCmdDraw"] = Command(
            name="vkCmdDraw", return_type=CType(base="void"), cmd_id=2, sync_mode="async",
            params=[vk_param("vertexCount", "uint32_t")])
        client_dispatch.emit(registry, cls.out)
        cls.source = (cls.out / "vk_dispatch_client.cpp").read_text()

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.out)

    def test_the_client_includes_the_scope(self):
        self.assertIn('#include "proto/client/sync_call_scope.hpp"', self.source)

    def test_a_sync_call_opens_its_scope_after_encoding_and_before_the_send(self):
        body = function_body(self.source, "vkGetFenceStatus")
        self.assertEqual(body.count(SCOPE), 1)
        scope = body.index(SCOPE)
        self.assertLess(body.index("t.request_writer()"), scope)
        self.assertLess(body.index("payload.write_pod(fence)"), scope)
        self.assertLess(scope, body.index("t.send_sync("))
        before = body[:scope]
        self.assertNotIn("return", before)

    def test_an_async_call_opens_no_scope(self):
        body = function_body(self.source, "vkCmdDraw")
        self.assertNotIn("SyncCallScope", body)
        self.assertIn("t.send_async(", body)


class OpenClClientEndsSyncCalls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.out = pathlib.Path(tempfile.mkdtemp())
        registry = opencl_model.Registry()
        scalar = opencl_model.Role.SCALAR
        registry.commands["clFinish"] = opencl_model.Command(
            name="clFinish", return_type=CType(base="cl_int"), cmd_id=1,
            params=[opencl_model.Param(name="value", type=CType(base="cl_uint"), role=scalar)])
        registry.commands["clFlush"] = opencl_model.Command(
            name="clFlush", return_type=CType(base="cl_int"), cmd_id=2, is_async=True,
            params=[opencl_model.Param(name="value", type=CType(base="cl_uint"), role=scalar)])
        opencl_emit.emit_client(registry, cls.out)
        cls.source = (cls.out / "cl_dispatch_client.cpp").read_text()

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.out)

    def test_the_client_includes_the_scope(self):
        self.assertIn('#include "proto/client/sync_call_scope.hpp"', self.source)

    def test_a_sync_call_opens_its_scope_after_encoding_and_before_the_send(self):
        body = function_body(self.source, "clFinish")
        self.assertEqual(body.count(SCOPE), 1)
        scope = body.index(SCOPE)
        self.assertLess(body.index("t.request_writer()"), scope)
        self.assertLess(body.index("value"), scope)
        self.assertLess(scope, body.index("t.send_sync("))
        self.assertNotIn("return", body[:scope])

    def test_an_async_call_opens_no_scope(self):
        body = function_body(self.source, "clFlush")
        self.assertNotIn("SyncCallScope", body)
        self.assertIn("t.send_async(", body)


if __name__ == "__main__":
    unittest.main()
