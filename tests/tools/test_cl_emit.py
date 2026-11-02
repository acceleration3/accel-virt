import pathlib
import re
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

CL_XML = ROOT / "third_party" / "khronos" / "cl.xml"

ADDED = ("clEnqueueMapBuffer", "clEnqueueUnmapMemObject", "clSVMAlloc", "clSVMFree",
         "clEnqueueSVMMap", "clEnqueueSVMUnmap", "clEnqueueSVMMemcpy", "clEnqueueSVMMemFill",
         "clSetKernelArgSVMPointer", "clSetEventCallback")


def function_body(text, name):
    for spelling in (f"decode_{name}(", f"av_{name}(", f"on_{name}("):
        if spelling in text:
            start = text.rindex("\n", 0, text.index(spelling)) + 1
            break
    else:
        raise AssertionError(f"no {name} in the emitted text")
    opened = text.index("{", start)
    depth = 0
    for index in range(opened, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    raise AssertionError(f"{name} does not close")


class Emitted(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.out = pathlib.Path(tempfile.mkdtemp())
        registry = cl_model.Registry()
        registry.handles = {"cl_command_queue", "cl_mem", "cl_context", "cl_kernel", "cl_event"}
        registry.commands["clAllocTest"] = cl_model.Command(
            name="clAllocTest",
            return_type=CType(base="void", pointer_depth=1),
            cmd_id=0x20100,
            since="CL_VERSION_2_0",
            params=[cl_model.Param("context", CType(base="cl_context"), cl_model.Role.HANDLE,
                                   element="cl_context"),
                    cl_model.Param("size", CType(base="size_t"), cl_model.Role.SCALAR)])
        registry.commands["clFillTest"] = cl_model.Command(
            name="clFillTest",
            return_type=CType(base="cl_int"),
            cmd_id=0x20101,
            since="CL_VERSION_2_0",
            params=[cl_model.Param("command_queue", CType(base="cl_command_queue"),
                                   cl_model.Role.HANDLE, element="cl_command_queue"),
                    cl_model.Param("svm_ptr", CType(base="void", pointer_depth=1),
                                   cl_model.Role.SVM_POINTER, len_param="size"),
                    cl_model.Param("size", CType(base="size_t"), cl_model.Role.SCALAR)])
        cl_emit.emit_client(registry, cls.out)
        cl_emit.emit_server(registry, cls.out)
        cl_emit.emit_default_dispatch(registry, cls.out)
        cls.client = (cls.out / "cl_dispatch_client.cpp").read_text()
        cls.client_header = (cls.out / "cl_dispatch_client.h").read_text()
        cls.server = (cls.out / "cl_dispatch_server.cpp").read_text()
        cls.server_header = (cls.out / "cl_dispatch_server.h").read_text()
        cls.default = (cls.out / "cl_default_dispatch.h").read_text()

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.out)

    def test_an_address_returning_client_answers_a_wire_word_not_a_pointer(self):
        self.assertIn("uint64_t av_clAllocTest(", self.client_header)
        self.assertNotIn("void* av_clAllocTest(", self.client_header)

    def test_an_address_returning_client_fails_with_zero(self):
        body = function_body(self.client, "clAllocTest")
        self.assertNotIn("CL_OUT_OF_RESOURCES", body)
        self.assertIn("return 0;", body)

    def test_an_address_returning_client_reads_the_reply_whole(self):
        body = function_body(self.client, "clAllocTest")
        self.assertIn("read_pod(ret)", body)
        self.assertNotIn("read_varint", body)

    def test_an_address_returning_server_writes_the_pointer_it_got(self):
        body = function_body(self.server, "clAllocTest")
        self.assertIn("const void* ret = s.on_clAllocTest(", body)
        self.assertRegex(body, r"reply\.write_pod<uint64_t>\(.*reinterpret_cast<uintptr_t>\(ret\)")

    def test_an_address_returning_default_answers_null(self):
        body = function_body(self.default, "clAllocTest")
        self.assertIn("return nullptr;", body)

    def test_an_svm_pointer_crosses_as_a_wire_word(self):
        self.assertIn("uint64_t svm_ptr", self.client_header)

    def test_an_svm_pointer_is_written_whole(self):
        body = function_body(self.client, "clFillTest")
        self.assertRegex(body, r"write_pod<uint64_t>\(svm_ptr\)")

    def test_the_server_resolves_an_svm_pointer_against_its_extent(self):
        body = function_body(self.server, "clFillTest")
        self.assertIn("svm_for(", body)
        self.assertRegex(body, r"svm_for\(\s*svm_ptr_wire\s*,[^)]*size")

    def test_a_refused_svm_pointer_stops_the_command(self):
        body = function_body(self.server, "clFillTest")
        self.assertRegex(body, r"if \(svm_ptr == nullptr\)[\s\S]*return false;")

    def test_svm_for_is_on_the_server_interface(self):
        self.assertRegex(self.server_header,
                         r"virtual void\s*\*\s*svm_for\(uint64_t address, uint64_t bytes\) const = 0;")


class LoweredFromTheRegistry(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not CL_XML.is_file():
            raise unittest.SkipTest("no cl.xml; run tools/fetch_khronos_registries.py")
        cls.raw = cl_parse.parse(CL_XML)
        cls.profile = cl_profile.load(
            ROOT / "tools" / "codegen" / "profiles" / "opencl.toml",
            ROOT / "tools" / "codegen" / "profiles" / "opencl_ids.toml")

    def test_every_added_command_is_carried(self):
        self.assertTrue(set(ADDED).issubset(set(self.profile.commands)),
                        sorted(set(ADDED) - set(self.profile.commands)))

    def test_every_added_command_lowers(self):
        registry = cl_lower.lower(self.raw, self.profile)
        for name in ADDED:
            with self.subTest(command=name):
                self.assertIn(name, registry.commands)

    def test_an_extension_is_carried_only_where_every_command_of_it_is(self):
        registry = cl_lower.lower(self.raw, self.profile)
        carried = set(registry.carried_extensions)
        self.assertIn("cl_khr_external_memory", carried)
        self.assertIn("cl_khr_external_memory_win32", carried)
        self.assertIn("cl_khr_gl_sharing", carried)
        self.assertNotIn("cl_khr_gl_event", carried)
        self.assertNotIn("cl_khr_command_buffer", carried)

    def test_an_extension_whose_dependency_is_not_carried_is_not_carried(self):
        profile = cl_profile.load(
            ROOT / "tools" / "codegen" / "profiles" / "opencl.toml",
            ROOT / "tools" / "codegen" / "profiles" / "opencl_ids.toml")
        profile.driver_implemented = set()
        carried = set(cl_lower.lower(self.raw, profile).carried_extensions)
        self.assertNotIn("cl_khr_external_memory", carried)
        self.assertNotIn("cl_khr_external_memory_win32", carried)

    def test_the_svm_pointers_lower_as_svm_pointers(self):
        registry = cl_lower.lower(self.raw, self.profile)
        wanted = {
            "clEnqueueUnmapMemObject": "mapped_ptr",
            "clSVMFree": "svm_pointer",
            "clEnqueueSVMMap": "svm_ptr",
            "clEnqueueSVMUnmap": "svm_ptr",
            "clEnqueueSVMMemFill": "svm_ptr",
            "clSetKernelArgSVMPointer": "arg_value",
        }
        for command, parameter in wanted.items():
            with self.subTest(command=command):
                found = [p for p in registry.commands[command].params if p.name == parameter]
                self.assertEqual(len(found), 1, parameter)
                self.assertEqual(found[0].role, cl_model.Role.SVM_POINTER)

    def test_a_memcpy_names_both_of_its_addresses(self):
        registry = cl_lower.lower(self.raw, self.profile)
        params = {p.name: p for p in registry.commands["clEnqueueSVMMemcpy"].params}
        for name in ("dst_ptr", "src_ptr"):
            self.assertEqual(params[name].role, cl_model.Role.SVM_POINTER, name)
            self.assertEqual(params[name].len_param, "size", name)

    def test_a_fill_pattern_is_still_bytes_with_its_own_length(self):
        registry = cl_lower.lower(self.raw, self.profile)
        pattern = {p.name: p for p in registry.commands["clEnqueueSVMMemFill"].params}["pattern"]
        self.assertEqual(pattern.role, cl_model.Role.BYTES_IN)
        self.assertEqual(pattern.len_param, "pattern_size")

    def test_the_two_map_and_alloc_commands_return_an_address(self):
        registry = cl_lower.lower(self.raw, self.profile)
        for name in ("clEnqueueMapBuffer", "clSVMAlloc"):
            with self.subTest(command=name):
                command = registry.commands[name]
                self.assertTrue(command.returns_address)
                self.assertFalse(command.returns_handle)
                self.assertFalse(command.returns_void)

    def test_the_ids_of_the_added_commands_are_new_and_in_opencl_s_range(self):
        for name in ADDED:
            with self.subTest(command=name):
                cmd_id = self.profile.command_id(name)
                self.assertEqual(cmd_id >> 16, 2, hex(cmd_id))
                self.assertGreaterEqual(cmd_id, 0x2006A, hex(cmd_id))


class CommittedTreeCarriesThem(unittest.TestCase):
    def test_the_client_declares_every_added_command(self):
        header = (ROOT / "src" / "proto" / "generated" / "opencl" / "cl_dispatch_client.h").read_text()
        for name in ADDED:
            self.assertIn(f"av_{name}(", header, name)

    def test_the_protocol_ids_carry_every_added_command(self):
        ids = (ROOT / "src" / "proto" / "generated" / "opencl" / "cl_protocol_ids.h").read_text()
        for name in ADDED:
            self.assertRegex(ids, rf"kCmd_{name} = 0x[0-9A-F]+U;", name)

    def test_the_server_interface_carries_svm_for(self):
        header = (ROOT / "src" / "proto" / "generated" / "opencl" / "cl_dispatch_server.h").read_text()
        self.assertIn("svm_for(", header)

    def test_the_carried_extensions_are_emitted_for_the_policy_gate(self):
        header = (ROOT / "src" / "proto" / "generated" / "opencl"
                  / "cl_supported_extensions.h").read_text()
        self.assertIn("kCarriedExtensions[]", header)
        self.assertIn('"cl_khr_external_memory_win32",', header)
        self.assertIn("extension_is_carried(const char* name)", header)

    def test_exec_info_resolves_svm_pointers_and_refuses_the_ones_nothing_carries(self):
        server = (ROOT / "src" / "proto" / "generated" / "opencl" / "cl_dispatch_server.cpp").read_text()
        self.assertNotIn("if (param_name == CL_KERNEL_EXEC_INFO_SVM_PTRS) return false;", server)
        self.assertIn("!resolve_svm_pointers(s, arena, param_value_size, param_value)) return false;", server)
        self.assertIn("resolved[i] = s.svm_for(address, 1);", server)
        self.assertIn("if (param_name == 0x40B8) return false;  // CL_KERNEL_EXEC_INFO_SVM_PTRS_ARM", server)
        self.assertIn("if (param_name == 0x4203) return false;  // CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL", server)

    def test_an_argument_the_server_cannot_vouch_for_is_answered_not_faulted(self):
        server = (ROOT / "src" / "proto" / "generated" / "opencl" / "cl_dispatch_server.cpp").read_text()
        self.assertIn("reply.write_varint(static_cast<uint64_t>(static_cast<int64_t>(CL_INVALID_ARG_VALUE)));", server)
        self.assertIn("if (!s.declared_argument(kernel, index, argument)) argument = KernelArgument::unknown;", server)
        header = (ROOT / "src" / "proto" / "generated" / "opencl" / "cl_dispatch_server.h").read_text()
        self.assertIn("virtual bool declared_argument(cl_kernel kernel, cl_uint index, KernelArgument& argument) = 0;",
                      header)


if __name__ == "__main__":
    unittest.main()
