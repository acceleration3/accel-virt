import pathlib
import re
import shutil
import sys
import tempfile
import unittest

from tool_run import ROOT

sys.path.insert(0, str(ROOT / "tools" / "codegen"))

from av_codegen.emit import client_dispatch, icd_entries, server_dispatch  # noqa: E402
from av_codegen.ir import Command, CType, Param, Registry  # noqa: E402
from av_codegen.opencl import emit as opencl_emit  # noqa: E402
from av_codegen.opencl import model as opencl_model  # noqa: E402

INCLUDE = '#include "core/trace/function_scope.hpp"'
GENERATED = ROOT / "src" / "proto" / "generated"


def body_start(source, at):
    brace = re.compile(r"\{\n([^\n]*)\n").search(source, at)
    assert brace is not None, source[at:at + 80]
    return brace.group(1).strip()


def first_line_after(source, opening):
    match = re.search(opening, source)
    assert match is not None, opening
    return body_start(source, match.start())


def vk_param(name, base, pointer_depth=0, is_const=False, direction="in"):
    return Param(name=name, type=CType(base=base, pointer_depth=pointer_depth, is_const=is_const),
                 direction=direction)


def vulkan_registry():
    registry = Registry()
    registry.commands["vkCmdDraw"] = Command(
        name="vkCmdDraw", return_type=CType(base="void"), cmd_id=1, sync_mode="async",
        params=[vk_param("commandBuffer", "VkCommandBuffer"), vk_param("vertexCount", "uint32_t")])
    registry.commands["vkDeviceWaitIdle"] = Command(
        name="vkDeviceWaitIdle", return_type=CType(base="VkResult"), cmd_id=2,
        params=[vk_param("device", "VkDevice")])
    return registry


class VulkanEmittersOpenOneScopePerCall(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.out = pathlib.Path(tempfile.mkdtemp())
        registry = vulkan_registry()
        icd_entries.emit(registry, cls.out)
        client_dispatch.emit(registry, cls.out)
        server_dispatch.emit(registry, cls.out)
        cls.entries = (cls.out / "vk_icd_entries.cpp").read_text()
        cls.client = (cls.out / "vk_dispatch_client.cpp").read_text()
        cls.server = (cls.out / "vk_dispatch_server.cpp").read_text()

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.out)

    def test_each_icd_entry_opens_the_api_name_first(self):
        for name in ("vkCmdDraw", "vkDeviceWaitIdle"):
            self.assertEqual(first_line_after(self.entries, r"Wrap_" + name + r"\("), f'AV_SCOPE("{name}");')
        self.assertEqual(self.entries.count("AV_SCOPE("), 2)
        self.assertIn(INCLUDE, self.entries)

    def test_each_client_function_opens_its_av_name_first(self):
        for name in ("vkCmdDraw", "vkDeviceWaitIdle"):
            self.assertEqual(first_line_after(self.client, r"\bav_" + name + r"\("), f'AV_SCOPE("av_{name}");')
        self.assertEqual(self.client.count("AV_SCOPE("), 2)
        self.assertIn(INCLUDE, self.client)

    def test_each_server_case_opens_its_own_scope_inside_its_block(self):
        for name in ("vkCmdDraw", "vkDeviceWaitIdle"):
            self.assertEqual(first_line_after(self.server, r"case ::av::vkproto::kCmd_" + name + r": \{"),
                             f'AV_SCOPE("server:{name}");')
        self.assertEqual(self.server.count("AV_SCOPE("), 2)
        self.assertIn(INCLUDE, self.server)


class OpenClEmittersOpenOneScopePerCall(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.out = pathlib.Path(tempfile.mkdtemp())
        registry = opencl_model.Registry()
        scalar = opencl_model.Role.SCALAR
        registry.commands["clFinish"] = opencl_model.Command(
            name="clFinish", return_type=CType(base="cl_int"), cmd_id=1,
            params=[opencl_model.Param(name="value", type=CType(base="cl_uint"), role=scalar)])
        opencl_emit.emit_client(registry, cls.out)
        opencl_emit.emit_server(registry, cls.out)
        cls.client = (cls.out / "cl_dispatch_client.cpp").read_text()
        cls.server = (cls.out / "cl_dispatch_server.cpp").read_text()

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.out)

    def test_the_client_and_the_decoder_each_open_one_scope(self):
        self.assertEqual(first_line_after(self.client, r"\bav_clFinish\("), 'AV_SCOPE("av_clFinish");')
        self.assertEqual(first_line_after(self.server, r"static bool decode_clFinish\("),
                         'AV_SCOPE("server:clFinish");')
        self.assertEqual(self.client.count("AV_SCOPE("), 1)
        self.assertEqual(self.server.count("AV_SCOPE("), 1)
        self.assertIn(INCLUDE, self.client)
        self.assertIn(INCLUDE, self.server)


class CommittedTreesMarkEveryCall(unittest.TestCase):
    def scopes_of(self, path, definition, mark):
        source = (GENERATED / path).read_text()
        found = 0
        for match in re.finditer(definition, source, re.M):
            found += 1
            self.assertEqual(body_start(source, match.start()), f'AV_SCOPE("{mark.format(match.group(1))}");',
                             match.group(0))
        self.assertGreater(found, 0, path)
        self.assertEqual(source.count("AV_SCOPE("), found, path)
        self.assertIn(INCLUDE, source)

    def test_opengl(self):
        self.scopes_of("opengl/gl_dispatch_client.cpp", r"^\S.* av_(gl\w+)\(", "av_{}")
        self.scopes_of("opengl/gl_dispatch_server.cpp", r"^static bool decode_(gl\w+)\(", "server:{}")
        self.scopes_of("opengl/gl_icd_entries.cpp", r"^\S.* APIENTRY Wrap_(gl\w+)\(", "{}")

    def test_opencl(self):
        self.scopes_of("opencl/cl_dispatch_client.cpp", r"^\S.* av_(cl\w+)\(", "av_{}")
        self.scopes_of("opencl/cl_dispatch_server.cpp", r"^static bool decode_(cl\w+)\(", "server:{}")

    def test_vulkan(self):
        self.scopes_of("vulkan/vk_dispatch_client.cpp", r"^\S.* av_(vk\w+)\(", "av_{}")
        self.scopes_of("vulkan/vk_icd_entries.cpp", r"^VKAPI_ATTR .* Wrap_(vk\w+)\(", "{}")
        self.scopes_of("vulkan/vk_dispatch_server.cpp", r"^    case ::av::vkproto::kCmd_(vk\w+): \{", "server:{}")


if __name__ == "__main__":
    unittest.main()
