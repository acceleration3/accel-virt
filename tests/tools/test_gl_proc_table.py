"""The OpenGL 1.1 table order is read from opengl32.dll's own table.

The fixtures are real images: a DLL clang-cl and lld-link build with a PDB,
holding a `glCltRGBAProcTable` of `glclt*` functions laid out as opengl32's is
(a u32 count, padding, then pointers), so the tool's PE and PDB reading is
judged, not a stand-in for it.
"""
import importlib.util
import pathlib
import tempfile
import unittest

from tool_run import ROOT, run

TOOL = ROOT / "tools" / "re" / "gl_proc_table.py"
COMMITTED = ROOT / "tools" / "codegen" / "profiles" / "opengl_proc_table.txt"
WORKSPACE_BINARY = ROOT / "re" / "bin" / "opengl32.dll"

spec = importlib.util.spec_from_file_location("gl_proc_table", TOOL)
gl_proc_table = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gl_proc_table)

ENTRIES = 336


def names(count=ENTRIES):
    return [f"glF{index}" for index in range(count)]


class Fixture:
    """opengl32's table shape: `count` in the header, then one pointer per
    element of `targets`, each naming a function the image defines."""

    def __init__(self, directory, count=ENTRIES, targets=None, functions=None, values=None, stubs=None):
        functions = functions or [f"glcltF{index}" for index in range(ENTRIES)]
        targets = targets or functions
        values = values or list(range(len(functions)))
        source = directory / "table.cpp"
        body = [f'extern "C" __declspec(dllexport) int {name}() {{ return {values[index]}; }}'
                for index, name in enumerate(functions)]
        body.append(f"struct Table {{ unsigned count; int (*entries[{len(targets)}])(); }};")
        body.append(f'extern "C" __declspec(dllexport) const Table glCltRGBAProcTable = {{{count}, {{'
                    + ", ".join(targets) + "}};")
        body.append('extern "C" int entry(void *, unsigned long, void *) { return 1; }')
        source.write_text("\n".join(body) + "\n")
        self.binary = directory / "opengl32.dll"
        self.step("clang-cl", "--target=x86_64-pc-windows-msvc", "/nologo", "/c", "/GS-", "/Z7", "/Gy",
                  f"/Fo{directory / 'table.obj'}", source)
        objects = [directory / "table.obj"]
        if stubs:
            objects.append(self.assemble(directory, stubs))
        self.step("lld-link", "/nologo", "/DLL", "/ENTRY:entry", "/NODEFAULTLIB", "/DEBUG",
                  "/OPT:NOREF,ICF", f"/PDB:{directory / 'opengl32.pdb'}", f"/OUT:{self.binary}", *objects)

    def assemble(self, directory, stubs):
        """opengl32's export shape: the thread's table, then a jump through
        the slot's offset in it."""
        lines = [".text", "slot_tls: .long 0x1480"]
        for name, slot in stubs.items():
            lines += [f".globl {name}", f"{name}:", "movq %gs:0x30, %rcx", "movl slot_tls(%rip), %eax",
                      "movq (%rcx,%rax), %rax", f"movq 0x{slot * 8:x}(%rax), %rax", "jmpq *%rax"]
        source = directory / "stubs.s"
        source.write_text("\n".join(lines) + "\n")
        output = directory / "stubs.obj"
        self.step("clang", "--target=x86_64-pc-windows-msvc", "-c", "-o", output, source)
        return output

    @staticmethod
    def step(*command):
        result = run(*command)
        if result.returncode != 0:
            raise AssertionError(result.stdout + result.stderr)


class ProcTableFileTest(unittest.TestCase):
    def table_of(self, lines):
        return gl_proc_table.ProcTableFile("\n".join(lines) + "\n").table()

    def test_reads_index_and_name_lines_after_the_header(self):
        lines = ["# opengl32.dll sha256 abc"] + [f"{index} {name}" for index, name in enumerate(names())]
        table = self.table_of(lines)
        self.assertEqual(table.binary_hash, "abc")
        self.assertEqual(table.names[0], "glF0")
        self.assertEqual(table.names[335], "glF335")
        self.assertEqual(table.render(), "\n".join(lines) + "\n")

    def test_refuses_a_file_without_the_header(self):
        with self.assertRaisesRegex(gl_proc_table.TableError, "no header"):
            self.table_of([f"{index} {name}" for index, name in enumerate(names())])

    def test_refuses_an_index_out_of_order_and_a_line_that_is_not_one_entry(self):
        lines = ["# opengl32.dll sha256 abc"] + [f"{index} {name}" for index, name in enumerate(names())]
        swapped = list(lines)
        swapped[5] = "5 glF5"
        swapped[6] = "4 glF4"
        with self.assertRaisesRegex(gl_proc_table.TableError, "line 6: index 5 where 4 was due"):
            self.table_of(swapped)
        extra = list(lines)
        extra[3] = "2 glF2 trailing"
        with self.assertRaisesRegex(gl_proc_table.TableError, "line 4"):
            self.table_of(extra)

    def test_refuses_a_short_table(self):
        lines = ["# opengl32.dll sha256 abc"] + [f"{index} {name}" for index, name in enumerate(names(335))]
        with self.assertRaisesRegex(gl_proc_table.TableError, "335 entries, not 336"):
            self.table_of(lines)

    def test_refuses_a_name_twice_and_a_name_that_is_no_gl_function(self):
        twice = names()
        twice[200] = twice[100]
        with self.assertRaisesRegex(gl_proc_table.TableError, "entry 200 names glF100 twice"):
            gl_proc_table.ProcTable("abc", twice)
        foreign = names()
        foreign[7] = "wglF7"
        with self.assertRaisesRegex(gl_proc_table.TableError, "entry 7 is 'wglF7'"):
            gl_proc_table.ProcTable("abc", foreign)


class BinaryTest(unittest.TestCase):
    def setUp(self):
        self.directory = pathlib.Path(tempfile.mkdtemp(prefix="gl-proc-table-"))
        self.out = self.directory / "opengl_proc_table.txt"

    def tearDown(self):
        for path in sorted(self.directory.rglob("*"), reverse=True):
            path.unlink() if path.is_file() else path.rmdir()
        self.directory.rmdir()

    def generate(self, fixture):
        return run(TOOL, "--binary", fixture.binary, "--out", self.out)

    def check(self, binary):
        return run(TOOL, "--binary", binary, "--out", self.out, "--check")

    def test_writes_the_order_the_binary_holds_and_check_then_matches(self):
        fixture = Fixture(self.directory)
        result = self.generate(fixture)
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = self.out.read_text().splitlines()
        self.assertEqual(len(lines), ENTRIES + 1)
        self.assertRegex(lines[0], r"^# opengl32.dll sha256 [0-9a-f]{64}$")
        self.assertEqual(lines[1], "0 glF0")
        self.assertEqual(lines[-1], "335 glF335")
        result = self.check(fixture.binary)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("matches", result.stdout)

    def test_reads_the_pointers_order_not_the_functions(self):
        functions = [f"glcltF{index}" for index in range(ENTRIES)]
        fixture = Fixture(self.directory, targets=list(reversed(functions)), functions=functions)
        self.assertEqual(self.generate(fixture).returncode, 0)
        lines = self.out.read_text().splitlines()
        self.assertEqual(lines[1], "0 glF335")
        self.assertEqual(lines[-1], "335 glF0")

    def test_check_names_the_first_entry_that_differs(self):
        fixture = Fixture(self.directory)
        self.assertEqual(self.generate(fixture).returncode, 0)
        text = self.out.read_text().replace("\n42 glF42\n", "\n42 glRenamed\n")
        self.out.write_text(text)
        result = self.check(fixture.binary)
        self.assertEqual(result.returncode, 1)
        self.assertIn("at entry 42", result.stderr)

    def test_check_names_a_header_from_another_binary(self):
        fixture = Fixture(self.directory)
        self.assertEqual(self.generate(fixture).returncode, 0)
        text = self.out.read_text()
        self.out.write_text("# opengl32.dll sha256 " + "0" * 64 + text[text.index("\n"):])
        result = self.check(fixture.binary)
        self.assertEqual(result.returncode, 1)
        self.assertIn("at the header", result.stderr)

    def test_check_without_a_committed_file_fails(self):
        fixture = Fixture(self.directory)
        result = self.check(fixture.binary)
        self.assertEqual(result.returncode, 1)
        self.assertIn("is missing", result.stderr)

    def test_a_table_whose_count_is_not_336_is_refused(self):
        fixture = Fixture(self.directory, count=3)
        result = self.generate(fixture)
        self.assertEqual(result.returncode, 1)
        self.assertIn("holds 3 entries, not 336", result.stderr)
        self.assertFalse(self.out.exists())

    def test_a_function_named_twice_is_refused(self):
        functions = [f"glcltF{index}" for index in range(ENTRIES)]
        targets = list(functions)
        targets[9] = functions[8]
        result = self.generate(Fixture(self.directory, targets=targets, functions=functions))
        self.assertEqual(result.returncode, 1)
        self.assertIn("entry 9 names glF8 twice", result.stderr)

    def test_a_pointer_to_a_function_without_the_client_prefix_is_refused(self):
        functions = [f"glcltF{index}" for index in range(ENTRIES)]
        functions[17] = "glF17"
        result = self.generate(Fixture(self.directory, functions=functions))
        self.assertEqual(result.returncode, 1)
        self.assertIn("entry 17 is glF17, not a glclt function", result.stderr)

    def test_a_pointer_at_no_public_symbol_is_refused(self):
        fixture = Fixture(self.directory)
        data = bytearray(fixture.binary.read_bytes())
        table = gl_proc_table.BinaryProcTable(fixture.binary, fixture.binary.with_suffix(".pdb"))
        image = gl_proc_table.PeImage(bytes(data))
        slot = image.offset_of(table.publics(image)["glCltRGBAProcTable"] + 8 + 8 * 30, 8)
        data[slot:slot + 8] = (int.from_bytes(data[slot:slot + 8], "little") + 1).to_bytes(8, "little")
        fixture.binary.write_bytes(bytes(data))
        result = self.generate(fixture)
        self.assertEqual(result.returncode, 1)
        self.assertIn("entry 30 points at RVA", result.stderr)

    def folded(self, stubs):
        """Slots 216 and 217 name two functions identical code folding made one."""
        values = list(range(ENTRIES))
        values[217] = values[216]
        return Fixture(self.directory, values=values, stubs=stubs)

    def test_folded_functions_are_named_by_the_stub_that_dispatches_through_each_slot(self):
        self.assertEqual(self.generate(self.folded({"glF216": 216, "glF217": 217})).returncode, 0)
        lines = self.out.read_text().splitlines()
        self.assertEqual(lines[217], "216 glF216")
        self.assertEqual(lines[218], "217 glF217")

    def test_folded_functions_follow_the_stubs_not_the_names(self):
        self.assertEqual(self.generate(self.folded({"glF216": 217, "glF217": 216})).returncode, 0)
        lines = self.out.read_text().splitlines()
        self.assertEqual(lines[217], "216 glF217")
        self.assertEqual(lines[218], "217 glF216")

    def test_folded_functions_without_a_stub_saying_which_are_refused(self):
        result = self.generate(self.folded({"glF216": 5}))
        self.assertEqual(result.returncode, 1)
        self.assertIn("entry 216 is one of glcltF216, glcltF217, and no exported stub", result.stderr)
        self.assertFalse(self.out.exists())

    def test_a_missing_workspace_is_skipped_by_check_and_refused_otherwise(self):
        absent = self.directory / "absent" / "opengl32.dll"
        result = self.check(absent)
        self.assertEqual(result.returncode, 0)
        self.assertIn("skipped", result.stdout)
        result = run(TOOL, "--binary", absent, "--out", self.out)
        self.assertEqual(result.returncode, 1)
        self.assertIn("absent", result.stderr)

    def test_a_binary_without_its_table_or_a_pe_is_refused(self):
        fixture = Fixture(self.directory)
        self.assertEqual(self.generate(fixture).returncode, 0)
        committed = self.out.read_text()
        not_pe = self.directory / "junk" / "opengl32.dll"
        not_pe.parent.mkdir()
        not_pe.write_bytes(b"not a PE image")
        (not_pe.parent / "opengl32.pdb").write_bytes((self.directory / "opengl32.pdb").read_bytes())
        result = self.check(not_pe)
        self.assertEqual(result.returncode, 1)
        self.assertIn("no MZ header", result.stderr)
        empty_pdb = self.directory / "opengl32.pdb"
        empty_pdb.write_bytes(b"not a PDB")
        result = self.check(fixture.binary)
        self.assertEqual(result.returncode, 1)
        self.assertIn("llvm-pdbutil refused it", result.stderr)
        self.assertEqual(self.out.read_text(), committed)


@unittest.skipUnless(WORKSPACE_BINARY.is_file(), "the reverse-engineering workspace is absent")
class CommittedTest(unittest.TestCase):
    def test_the_committed_order_is_the_pinned_binarys(self):
        result = run(TOOL, "--check")
        self.assertEqual(result.returncode, 0, result.stderr)
        table = gl_proc_table.ProcTableFile(COMMITTED.read_text()).table()
        self.assertEqual(table.names[0], "glNewList")
        self.assertEqual(table.names[136], "glVertex3f")
        self.assertEqual(table.names[215], "glEnable")


if __name__ == "__main__":
    unittest.main()
