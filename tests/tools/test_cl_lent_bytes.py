import pathlib
import shutil
import sys
import tempfile
import unittest

from tool_run import ROOT

sys.path.insert(0, str(ROOT / "tools" / "codegen"))

from av_codegen.ir import CType                                  # noqa: E402
from av_codegen.opencl import emit as cl_emit                    # noqa: E402
from av_codegen.opencl import lower as cl_lower                  # noqa: E402
from av_codegen.opencl import model as cl_model                  # noqa: E402
from av_codegen.opencl import parse as cl_parse                  # noqa: E402
from av_codegen.opencl import profile as cl_profile              # noqa: E402

from test_cl_emit import function_body                           # noqa: E402

CL_XML = ROOT / "third_party" / "khronos" / "cl.xml"

LENT = {("clEnqueueReadBuffer", "ptr"), ("clEnqueueWriteBuffer", "ptr"),
        ("clEnqueueReadImage", "ptr"), ("clEnqueueWriteImage", "ptr"),
        ("clEnqueueReadBufferRect", "ptr"), ("clEnqueueWriteBufferRect", "ptr")}


def transfer(name, cmd_id, role, lent):
    return cl_model.Command(
        name=name, return_type=CType(base="cl_int"), cmd_id=cmd_id, since="CL_VERSION_1_0",
        params=[cl_model.Param("command_queue", CType(base="cl_command_queue"), cl_model.Role.HANDLE,
                               element="cl_command_queue"),
                cl_model.Param("size", CType(base="size_t"), cl_model.Role.SCALAR),
                cl_model.Param("ptr", CType(base="void", pointer_depth=1), role, len_param="size",
                               lent=lent)])


class Emitted(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.out = pathlib.Path(tempfile.mkdtemp())
        registry = cl_model.Registry()
        registry.handles = {"cl_command_queue"}
        for name, cmd_id, role, lent in (("clLentRead", 0x20200, cl_model.Role.BYTES_OUT, True),
                                         ("clLentWrite", 0x20201, cl_model.Role.BYTES_IN, True),
                                         ("clSentRead", 0x20202, cl_model.Role.BYTES_OUT, False),
                                         ("clSentWrite", 0x20203, cl_model.Role.BYTES_IN, False)):
            registry.commands[name] = transfer(name, cmd_id, role, lent)
        cl_emit.emit_client(registry, cls.out)
        cl_emit.emit_server(registry, cls.out)
        cls.client = (cls.out / "cl_dispatch_client.cpp").read_text()
        cls.server = (cls.out / "cl_dispatch_server.cpp").read_text()
        cls.server_header = (cls.out / "cl_dispatch_server.h").read_text()

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.out)

    def test_a_lent_parameter_is_named_by_offset_when_the_arena_holds_it(self):
        for name in ("clLentRead", "clLentWrite"):
            body = function_body(self.client, name)
            self.assertIn("t.blobs().holds(ptr, static_cast<size_t>(size), ptr_lent_at)", body, name)
            self.assertIn("b.write_pod<uint8_t>(ptr == nullptr ? 0 : (ptr_lent ? 2 : 1));", body, name)
            self.assertIn("b.write_varint(ptr_lent_at);", body, name)

    def test_a_lent_write_carries_its_bytes_only_when_not_lent(self):
        body = function_body(self.client, "clLentWrite")
        self.assertIn("} else if (ptr != nullptr) {", body)
        self.assertLess(body.index("if (ptr_lent) {"), body.index("b.write(ptr,"))

    def test_a_lent_read_takes_no_bytes_back(self):
        body = function_body(self.client, "clLentRead")
        self.assertIn("if (wire_count != 0 && !ptr_lent && !r.read(ptr,", body)

    def test_the_server_resolves_a_lent_parameter_and_refuses_one_outside_the_arena(self):
        write = function_body(self.server, "clLentWrite")
        self.assertIn("if (wire_present == 2) {", write)
        self.assertIn("void* wire_bytes = s.guest_bytes(wire_offset, wire_count);", write)
        self.assertIn("if (wire_bytes == nullptr) return false;", write)
        read = function_body(self.server, "clLentRead")
        self.assertIn("ptr_lent = s.guest_bytes(wire_offset, static_cast<uint64_t>(ptr_capacity));", read)
        self.assertIn("if (ptr_lent == nullptr) return false;", read)
        self.assertIn("if (wire_written != 0 && ptr_lent == nullptr) reply.write(ptr,", read)

    def test_bytes_not_lent_cross_as_they_always_have(self):
        for name in ("clSentRead", "clSentWrite"):
            self.assertNotIn("holds(", function_body(self.client, name), name)
            self.assertNotIn("guest_bytes(", function_body(self.server, name), name)
        self.assertIn("if (wire_written != 0) reply.write(ptr,", function_body(self.server, "clSentRead"))

    def test_guest_bytes_is_on_the_server_interface(self):
        self.assertIn("uint8_t* guest_bytes(uint64_t offset, uint64_t bytes) const;", self.server_header)


class LoweredFromTheRegistry(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not CL_XML.is_file():
            raise unittest.SkipTest("no cl.xml; run tools/fetch_khronos_registries.py")
        cls.raw = cl_parse.parse(CL_XML)
        cls.profile = cl_profile.load(
            ROOT / "tools" / "codegen" / "profiles" / "opencl.toml",
            ROOT / "tools" / "codegen" / "profiles" / "opencl_ids.toml")

    def test_only_the_transfers_host_memory_is_lent(self):
        registry = cl_lower.lower(self.raw, self.profile)
        lent = {(cmd.name, p.name) for cmd in registry.commands.values() for p in cmd.params if p.lent}
        self.assertEqual(lent, LENT)

    def test_lending_anything_but_bytes_is_refused(self):
        profile = self.profile

        class Lending:
            def __getattr__(self, name):
                return getattr(profile, name)

            def param_override(self, command, param):
                override = dict(profile.param_override(command, param) or {})
                if (command, param) == ("clEnqueueReadBuffer", "offset"):
                    override["lent"] = True
                return override

        with self.assertRaisesRegex(ValueError, "clEnqueueReadBuffer.offset is lent but is not bytes"):
            cl_lower.lower(self.raw, Lending())


if __name__ == "__main__":
    unittest.main()
