"""The generated OpenGL ICD entries and their proc table, from a registry small
enough to read (tools/codegen/rvk_codegen/opengl/emit.py, feature 050).

What opengl32.dll relies on: the table DrvSetContext answers is Microsoft's
GLCLTPROCTABLE, in the order tools/codegen/profiles/opengl_proc_table.txt
records, with no empty slot; every other name is answered by DrvGetProcAddress.
An ICD exports no GL symbol, so nothing generated has a C name. And the client
it calls ends every synchronous call on every path from its writer, and a
one-way call whose payload it drops, so a locked transport is always let go."""
import pathlib
import re
import shutil
import sys
import tempfile
import unittest

from tool_run import ROOT

sys.path.insert(0, str(ROOT / "tools" / "codegen"))

from rvk_codegen.ir import CType  # noqa: E402
from rvk_codegen.opengl import emit  # noqa: E402
from rvk_codegen.opengl.model import Command, Length, LengthKind, Param, Registry, Role  # noqa: E402

SCOPE = "const ::av::SyncCallScope rvk_call{rvk_t};"

ORDER = """# opengl32.dll sha256 0000
0 glNewList
1 glBegin
2 glColor3b_InRGBA
3 glEnable
4 glGetPointerv
"""


def scalar(name, base):
    return Param(name=name, type=CType(base=base), role=Role.SCALAR)


def command(name, params, tier="core", returns="void", ident=0):
    return Command(name=name, return_type=CType(base=returns), params=params, tier=tier, command_id=ident)


def registry():
    reg = Registry()
    pixels = Param(name="data", type=CType(base="GLint", pointer_depth=1, is_const=True), role=Role.ARRAY_IN,
                   length=Length(LengthKind.PARAM, text="count"), element="GLint")
    for cmd in [
        command("glNewList", [scalar("list", "GLuint"), scalar("mode", "GLenum")], ident=1),
        command("glBegin", [scalar("mode", "GLenum")], tier="compat", ident=2),
        command("glColor3b", [scalar("red", "GLbyte"), scalar("green", "GLbyte"), scalar("blue", "GLbyte")],
                ident=3),
        command("glEnable", [scalar("cap", "GLenum")], ident=4),
        command("glDrawArrays", [scalar("mode", "GLenum"), scalar("first", "GLint"), scalar("count", "GLsizei")],
                ident=5),
        command("glIsEnabled", [scalar("cap", "GLenum")], returns="GLboolean", ident=6),
        command("glGetString", [scalar("name", "GLenum")], ident=7,
                returns="const GLubyte*"),
        command("glUniform1iv", [scalar("location", "GLint"), scalar("count", "GLsizei"), pixels], ident=8),
    ]:
        reg.commands[cmd.name] = cmd
    reg.commands["glGetString"].return_type = CType(base="GLubyte", pointer_depth=1, is_const=True)
    reg.aliases["glDrawArraysEXT"] = "glDrawArrays"
    reg.driver_entries["glGetPointerv"] = ("void", "GLenum pname, void **params")
    return reg


def body_of(source, name):
    match = re.search(r"\b" + name + r"\(.*?\) \{\n(.*?)\n\}\n", source, re.S)
    assert match is not None, name
    return match.group(1)


class GeneratedGlIcd(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.out = pathlib.Path(tempfile.mkdtemp())
        emit.emit_icd(registry(), cls.out, emit.read_proc_table(ORDER))
        emit.emit_client(registry(), cls.out)
        cls.table = (cls.out / "gl_icd_table.h").read_text()
        cls.header = (cls.out / "gl_icd_entries.h").read_text()
        cls.source = (cls.out / "gl_icd_entries.cpp").read_text()
        cls.client = (cls.out / "gl_dispatch_client.cpp").read_text()

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.out)

    def slots(self):
        return re.findall(r"table\.slots\[(\d+)\] = (.*?);", self.table)

    def test_the_table_is_the_order_files_with_every_slot_filled(self):
        self.assertIn("inline constexpr uint32_t kProcTableSlots = 5u;", self.table)
        self.assertIn("table.entries = static_cast<int32_t>(kProcTableSlots);", self.table)
        self.assertEqual(self.slots(), [
            ("0", "reinterpret_cast<GlProc>(&Wrap_glNewList)"),
            ("1", "reinterpret_cast<GlProc>(&Wrap_glBegin)"),
            ("2", "reinterpret_cast<GlProc>(&Wrap_glColor3b)"),
            ("3", "reinterpret_cast<GlProc>(&Wrap_glEnable)"),
            ("4", "reinterpret_cast<GlProc>(&Wrap_glGetPointerv)"),
        ])
        self.assertIn("GlProc slots[kProcTableSlots];", self.table)
        self.assertIn("int32_t entries;", self.table)

    def test_every_other_entry_is_answered_by_name_and_no_table_entry_is(self):
        rows = re.findall(r'\{"(\w+)", reinterpret_cast<GlProc>\(&(\w+)\)\}', self.table)
        self.assertEqual(sorted(rows), sorted([
            ("glDrawArrays", "Wrap_glDrawArrays"),
            ("glIsEnabled", "Wrap_glIsEnabled"),
            ("glGetString", "Wrap_glGetString"),
            ("glUniform1iv", "Wrap_glUniform1iv"),
            ("glDrawArraysEXT", "Wrap_glDrawArrays"),
        ]))

    def test_nothing_has_a_c_name(self):
        for text in (self.header, self.source, self.table):
            self.assertNotIn('extern "C"', text)
        self.assertIn("namespace av::glicd::generated {", self.header)
        self.assertIn("void APIENTRY Wrap_glEnable(GLenum cap);", self.header)
        self.assertIn("GLboolean APIENTRY Wrap_glIsEnabled(GLenum cap);", self.header)
        self.assertNotRegex(self.header, r"APIENTRY gl\w+\(")
        self.assertFalse((self.out / "gl_icd_exports.def").exists())

    def test_a_manual_entry_is_declared_for_the_icd_and_not_defined(self):
        self.assertIn("void APIENTRY Wrap_glGetPointerv(GLenum pname, void **params);", self.header)
        self.assertNotIn("Wrap_glGetPointerv(", self.source)
        self.assertNotIn("legacy_entry", self.table)

    def test_a_compatibility_entry_asks_the_granted_profile_before_it_sends(self):
        """Feature 051: a compat-tier command exists only where the host
        granted a compatibility profile, so the forwarder judges it and records
        what a core context would."""
        begin = body_of(self.source, "Wrap_glBegin")
        self.assertIn("if (!::av::glicd::legacy_calls_live()) {", begin)
        self.assertIn("::av::glicd::refuse_legacy();", begin)
        self.assertIn("::av::glproto::rvk_glBegin(*rvk_t, mode);", begin)
        # A core-tier command asks nothing.
        self.assertNotIn("legacy_calls_live", body_of(self.source, "Wrap_glEnable"))

    def test_forwarding_entries_are_defined_and_handwritten_ones_are_left_to_the_icd(self):
        self.assertIn("void APIENTRY Wrap_glEnable(GLenum cap) {", self.source)
        self.assertIn("::av::glproto::rvk_glEnable(*rvk_t, cap);", body_of(self.source, "Wrap_glEnable"))
        self.assertIn("const GLubyte* APIENTRY Wrap_glGetString(GLenum name);", self.header)
        self.assertNotIn("Wrap_glGetString(GLenum name) {", self.source)
        self.assertNotIn("Wrap_glDrawArrays(GLenum mode, GLint first, GLsizei count) {", self.source)

    def test_a_sync_call_ends_on_every_path_from_the_writer(self):
        body = body_of(self.client, "rvk_glIsEnabled")
        self.assertEqual(body.count(SCOPE), 1)
        scope = body.index(SCOPE)
        self.assertEqual(body.index("\n", body.index("rvk_t.request_writer()")) + 1,
                         body.rindex("\n", 0, scope) + 1)
        self.assertNotIn("return", body[:scope])
        self.assertLess(scope, body.index("rvk_t.send_sync("))
        self.assertNotIn("end_call", body)
        self.assertIn('#include "proto/sync_call_scope.hpp"', self.client)

    def test_a_one_way_call_ends_only_when_its_payload_is_dropped(self):
        for name in ("rvk_glEnable", "rvk_glUniform1iv"):
            body = body_of(self.client, name)
            self.assertNotIn("SyncCallScope", body, name)
            self.assertEqual(body.count("rvk_t.end_call();"), 1, name)
            dropped = body[body.index("if (rvk_b.status()"):body.index("rvk_t.send_async(")]
            self.assertIn("rvk_t.end_call();", dropped, name)
            self.assertLess(dropped.index("rvk_t.end_call();"), dropped.index("return;"), name)

    def test_the_client_leases_as_the_arena_class_names_it(self):
        body = body_of(self.client, "rvk_glUniform1iv")
        self.assertIn("::av::ClientBlobLease data_lease{};", body)
        self.assertIn("data_lease.taken()", body)
        self.assertIn("data_lease.data()", body)
        self.assertIn("data_lease.offset()", body)
        for gone in ("ClientBlobArena::Lease", ".ok()", "retire_after_last", "data_lease.data,",
                     "data_lease.offset)"):
            self.assertNotIn(gone, self.client)

    def test_a_one_way_call_keeps_its_leases_until_an_answer_retires_them(self):
        body = body_of(self.client, "rvk_glUniform1iv")
        after_send = body[body.index("rvk_t.send_async("):]
        self.assertNotIn("give_back", after_send)
        dropped = body[body.index("if (rvk_b.status()"):body.index("rvk_t.send_async(")]
        self.assertIn("rvk_t.blobs()", dropped)
        self.assertIn(".give_back(data_lease)", dropped)


class ProcTableOrder(unittest.TestCase):
    def test_a_table_row_the_registry_does_not_carry_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(emit.GenerationError, "glUnknown"):
                emit.emit_icd(registry(), pathlib.Path(directory),
                              emit.read_proc_table("0 glNewList\n1 glUnknown\n"))

    def test_order_files_out_of_sequence_or_malformed_are_refused(self):
        for text in ("1 glNewList\n", "0 glNewList\n0 glBegin\n", "0\n", "x glNewList\n", "0 glNewList extra\n",
                     "# nothing\n"):
            with self.subTest(text=text), self.assertRaises(emit.GenerationError):
                emit.read_proc_table(text)

    def test_the_rgba_suffix_names_the_same_command(self):
        self.assertEqual(emit.read_proc_table("0 glColor3b_InRGBA\n1 glEnable\n"), ["glColor3b", "glEnable"])


if __name__ == "__main__":
    unittest.main()
