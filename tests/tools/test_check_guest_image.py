import importlib.util
import pathlib
import shutil
import struct
import tempfile
import unittest

from tool_run import ROOT, run

CHECK = ROOT / "tools" / "check_guest_image.py"

spec = importlib.util.spec_from_file_location("check_guest_image", CHECK)
check_guest_image = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check_guest_image)

PROGRAM = """extern "C" __declspec(dllimport) void ExitProcess(unsigned code);
extern "C" __declspec(dllimport) long D3DKMTEnumAdapters2(void *request);
extern "C" void entry() { D3DKMTEnumAdapters2(nullptr); ExitProcess(0); }
"""

LIBRARY = """extern "C" __declspec(dllimport) long D3DKMTEnumAdapters2(void *request);
extern "C" int entry(void *, unsigned long, void *) { return D3DKMTEnumAdapters2(nullptr) == 0; }
extern "C" long vk_icdNegotiateLoaderICDInterfaceVersion(unsigned *version) { return *version; }
extern "C" void *vk_icdGetInstanceProcAddr(void *instance, const char *name) { return instance; }
"""

ICD_EXPORTS = ("vk_icdNegotiateLoaderICDInterfaceVersion", "vk_icdGetInstanceProcAddr")
X86_BASE = 0x10000000


# A table of pointers to its own code, which gives an image base relocations.
POINTERS = """
extern "C" long vk_icdNegotiateLoaderICDInterfaceVersion(unsigned *version);
extern "C" void *const own_entries[] = {reinterpret_cast<void *>(&vk_icdNegotiateLoaderICDInterfaceVersion)};
"""

# What each architecture is built as: the target, llvm-dlltool's machine, and
# what lld-link needs to resolve an x86 __stdcall import against the
# undecorated import library src/um/CMakeLists.txt makes.
ARCHITECTURES = {"amd64": ("x86_64-pc-windows-msvc", "i386:x86-64", []),
                 "i386": ("i686-pc-windows-msvc", "i386", ["-stdcall-fixup"])}


class Image:
    """A small image importing `modules`, linked for `subsystem`; a DLL
    exporting `exports` when it names any; x64 unless `machine` says i386."""

    def __init__(self, directory, name, subsystem, modules, exports=(), machine="amd64", base=None):
        target, dlltool_machine, fixup = ARCHITECTURES[machine]
        self.path = directory / (f"{name}.dll" if exports else f"{name}.exe")
        source = directory / f"{name}.cpp"
        source.write_text(LIBRARY + POINTERS if exports else PROGRAM)
        libraries = []
        for module, symbols in modules.items():
            definition = directory / f"{name}-{module}.def"
            definition.write_text(f"LIBRARY {module}\nEXPORTS\n" + "\n".join(symbols) + "\n")
            library = directory / f"{name}-{module}.lib"
            self.step("llvm-dlltool", "-m", dlltool_machine, "-d", definition, "-l", library)
            libraries.append(library)
        link = []
        if exports:
            definition = directory / f"{name}.def"
            definition.write_text(f"LIBRARY {name}\nEXPORTS\n" + "\n".join(exports) + "\n")
            link = ["/DLL", f"/DEF:{definition}"]
        if base is not None:
            link.append(f"/BASE:0x{base:X}")
        self.step("clang-cl", f"--target={target}", "/nologo", "/c", "/GS-",
                  f"/Fo{directory / (name + '.obj')}", source)
        self.step("lld-link", "/nologo", f"/SUBSYSTEM:{subsystem}", "/ENTRY:entry", "/NODEFAULTLIB", *fixup,
                  *link, f"/OUT:{self.path}", directory / f"{name}.obj", *libraries)

    @staticmethod
    def step(*command):
        result = run(*command)
        if result.returncode != 0:
            raise AssertionError(result.stdout + result.stderr)


SECTION_RVA = 0x1000
FILE_ALIGNMENT = 0x200
OPTIONAL_AT = 0x58
OPTIONAL_SIZE = 240


IMAGE_BASE = 0x180000000
RELOCATED_DIR64 = 10


def synthetic_pe(machine=0x8664, dll=True, subsystem=2, imports=None, delay_imports=None,
                 exports=(), unnamed_exports=0, relocated=(), relocation_block=None):
    """A PE32+ image laid out by hand: one section holding the import, delay
    import and export tables. A symbol given as an int is imported by ordinal.
    `relocated` is one DIR64-relocated pointer per value, each holding that
    value; `relocation_block` replaces the whole table with raw bytes."""
    section = bytearray()

    def place(raw):
        rva = SECTION_RVA + len(section)
        section.extend(raw)
        return rva

    def string(text):
        return place(text.encode() + b"\0")

    def thunks(symbols):
        return place(b"".join(struct.pack("<Q", (1 << 63) | symbol if isinstance(symbol, int)
                                          else place(b"\0\0" + symbol.encode() + b"\0"))
                              for symbol in symbols) + bytes(8))

    imports = {"kernel32.dll": ["ExitProcess"]} if imports is None else imports
    descriptors = b""
    for module, symbols in imports.items():
        table = thunks(symbols)
        descriptors += struct.pack("<IIIII", table, 0, 0, string(module), table)
    import_directory = (place(descriptors + bytes(20)), len(descriptors) + 20)

    delay_directory = (0, 0)
    if delay_imports:
        descriptors = b""
        for module, symbols in delay_imports.items():
            table = thunks(symbols)
            descriptors += struct.pack("<8I", 1, string(module), 0, table, table, 0, 0, 0)
        delay_directory = (place(descriptors + bytes(32)), len(descriptors) + 32)

    export_directory = (0, 0)
    if exports or unnamed_exports:
        ordered = sorted(exports)
        names = place(b"".join(struct.pack("<I", string(name)) for name in ordered))
        ordinals = place(b"".join(struct.pack("<H", index) for index in range(len(ordered))))
        functions = place(struct.pack("<I", SECTION_RVA) * (len(ordered) + unnamed_exports))
        export_directory = (place(struct.pack("<IIHHIIIIIII", 0, 0, 0, 0, string("synthetic.dll"), 1,
                                              len(ordered) + unnamed_exports, len(ordered),
                                              functions, names, ordinals)), 40)

    relocation_directory = (0, 0)
    if relocated or relocation_block is not None:
        pointers = [place(struct.pack("<Q", value)) for value in relocated]
        if relocation_block is None:
            entries = b"".join(struct.pack("<H", (RELOCATED_DIR64 << 12) | (rva & 0xFFF))
                               for rva in pointers)
            page = (pointers[0] & ~0xFFF) if pointers else SECTION_RVA
            relocation_block = struct.pack("<II", page, 8 + len(entries)) + entries
        relocation_directory = (place(relocation_block), len(relocation_block))

    raw_size = -(-len(section) // FILE_ALIGNMENT) * FILE_ALIGNMENT
    headers = bytearray(FILE_ALIGNMENT)
    headers[0:2] = b"MZ"
    struct.pack_into("<I", headers, 0x3C, 0x40)
    headers[0x40:0x44] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", headers, 0x44, machine, 1, 0, 0, 0, OPTIONAL_SIZE,
                     0x0022 | (0x2000 if dll else 0))
    struct.pack_into("<H", headers, OPTIONAL_AT, 0x20B)
    struct.pack_into("<H", headers, OPTIONAL_AT + 68, subsystem)
    struct.pack_into("<I", headers, OPTIONAL_AT + 108, 16)
    struct.pack_into("<Q", headers, OPTIONAL_AT + 24, IMAGE_BASE)
    for index, entry in ((0, export_directory), (1, import_directory), (5, relocation_directory),
                         (13, delay_directory)):
        struct.pack_into("<II", headers, OPTIONAL_AT + 112 + 8 * index, *entry)
    struct.pack_into("<8sIIII", headers, OPTIONAL_AT + OPTIONAL_SIZE, b".rdata", len(section),
                     SECTION_RVA, raw_size, FILE_ALIGNMENT)
    return bytes(headers) + bytes(section) + bytes(raw_size - len(section))


GUEST_IMPORTS = {"gdi32.dll": ["D3DKMTEnumAdapters2"], "kernel32.dll": ["ExitProcess"],
                 "ntdll.dll": ["RtlZeroMemory"]}


class GuestImageCheck(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scratch = pathlib.Path(tempfile.mkdtemp())
        guest = {"gdi32.dll": ["D3DKMTEnumAdapters2"], "kernel32.dll": ["ExitProcess"]}
        cls.good = Image(cls.scratch, "good", "CONSOLE", guest)
        cls.window = Image(cls.scratch, "window", "WINDOWS", guest)
        cls.foreign = Image(cls.scratch, "foreign", "CONSOLE",
                            {"d3d12.dll": ["D3DKMTEnumAdapters2"], "kernel32.dll": ["ExitProcess"]})
        cls.icd = Image(cls.scratch, "icd", "WINDOWS", {"gdi32.dll": ["D3DKMTEnumAdapters2"]},
                        exports=ICD_EXPORTS)
        cls.icd32 = Image(cls.scratch, "icd32", "WINDOWS", {"gdi32.dll": ["D3DKMTEnumAdapters2"]},
                          exports=ICD_EXPORTS, machine="i386", base=X86_BASE)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.scratch)

    def test_accepts_a_console_image_importing_only_guest_modules(self):
        result = run(CHECK, self.good.path, "--require-import", "D3DKMTEnumAdapters2")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("guest_image: ok", result.stdout)

    def test_rejects_another_subsystem(self):
        result = run(CHECK, self.window.path)
        self.assertEqual(result.returncode, 1)
        self.assertIn("subsystem IMAGE_SUBSYSTEM_WINDOWS_GUI, want IMAGE_SUBSYSTEM_WINDOWS_CUI", result.stderr)

    def test_rejects_a_module_a_guest_may_not_have(self):
        result = run(CHECK, self.foreign.path)
        self.assertEqual(result.returncode, 1)
        self.assertIn("imports d3d12.dll, not a module every guest has", result.stderr)

    def test_a_required_import_must_be_present(self):
        result = run(CHECK, self.good.path, "--require-import", "D3DKMTSubmitCommand")
        self.assertEqual(result.returncode, 1)
        self.assertIn("does not import D3DKMTSubmitCommand", result.stderr)

    def test_rejects_a_missing_or_unreadable_image_naming_it(self):
        missing = self.scratch / "absent.exe"
        result = run(CHECK, missing)
        self.assertEqual(result.returncode, 1)
        self.assertIn(str(missing), result.stderr)
        garbage = self.scratch / "garbage.exe"
        garbage.write_bytes(b"not a PE image")
        result = run(CHECK, garbage)
        self.assertEqual(result.returncode, 1)
        self.assertIn("not a PE image: no MZ header", result.stderr)

    def test_accepts_a_linked_dll_exporting_exactly_the_icd_entry_points(self):
        """A real linker's export table, not only the one these tests lay out."""
        arguments = ["--dll", "--only-exports", "--require-import", "D3DKMTEnumAdapters2"]
        for export in ICD_EXPORTS:
            arguments += ["--require-export", export]
        result = run(CHECK, self.icd.path, *arguments)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("guest_image: ok", result.stdout)

    def test_a_32_bit_dll_passes_as_i386_and_is_refused_as_the_default_amd64(self):
        """The x86 drivers a WoW64 process loads, checked as what they are."""
        arguments = ["--dll", "--only-exports", "--require-import", "D3DKMTEnumAdapters2"]
        for export in ICD_EXPORTS:
            arguments += ["--require-export", export]
        result = run(CHECK, self.icd32.path, "--machine", "i386", *arguments)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("guest_image: ok", result.stdout)
        result = run(CHECK, self.icd32.path, *arguments)
        self.assertEqual(result.returncode, 1)
        self.assertIn("machine IMAGE_FILE_MACHINE_I386, want IMAGE_FILE_MACHINE_AMD64", result.stderr)
        result = run(CHECK, self.icd.path, "--machine", "i386", *arguments)
        self.assertEqual(result.returncode, 1)
        self.assertIn("machine IMAGE_FILE_MACHINE_AMD64, want IMAGE_FILE_MACHINE_I386", result.stderr)

    def test_a_32_bit_images_base_and_32_bit_relocations_are_read(self):
        """PE32's ImageBase is four bytes past where PE32+'s begins, and its
        pointers are HIGHLOW relocations: each one read holds an address inside
        the image, above its base."""
        image = check_guest_image.PeImage(self.icd32.path.read_bytes())
        self.assertEqual(image.image_base, X86_BASE)
        pointers = image.relocated_pointers()
        self.assertTrue(pointers)
        for _, value in pointers:
            self.assertGreater(value, X86_BASE)
            self.assertLess(value, X86_BASE + 0x100000)

    def test_a_linked_dll_is_refused_as_an_executable_and_for_an_export_it_lacks(self):
        result = run(CHECK, self.icd.path, "--require-export", "vk_icdGetPhysicalDeviceProcAddr")
        self.assertEqual(result.returncode, 1)
        self.assertIn("subsystem IMAGE_SUBSYSTEM_WINDOWS_GUI, want IMAGE_SUBSYSTEM_WINDOWS_CUI", result.stderr)
        self.assertIn("does not export vk_icdGetPhysicalDeviceProcAddr", result.stderr)


class SyntheticDllCheck(unittest.TestCase):
    def setUp(self):
        self.scratch = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)

    def check(self, image, *arguments):
        path = self.scratch / "image.dll"
        path.write_bytes(image)
        return run(CHECK, path, *arguments)

    def refusals(self, result):
        self.assertEqual(result.returncode, 1, result.stdout)
        return [line.split(": ", 1)[1] for line in result.stderr.splitlines()]

    def icd(self, **overrides):
        fields = {"imports": GUEST_IMPORTS, "exports": ICD_EXPORTS, **overrides}
        return synthetic_pe(**fields)

    def exactly_the_icd(self):
        arguments = ["--dll", "--only-exports"]
        for export in ICD_EXPORTS:
            arguments += ["--require-export", export]
        return arguments

    def test_a_dll_of_either_windows_subsystem_passes(self):
        for subsystem in (2, 3):
            result = self.check(self.icd(subsystem=subsystem), *self.exactly_the_icd())
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("guest_image: ok", result.stdout)

    def test_a_dll_of_another_subsystem_is_refused_naming_both_it_accepts(self):
        result = self.check(self.icd(subsystem=1), *self.exactly_the_icd())
        self.assertEqual(self.refusals(result), [
            "subsystem IMAGE_SUBSYSTEM_NATIVE, want IMAGE_SUBSYSTEM_WINDOWS_GUI or IMAGE_SUBSYSTEM_WINDOWS_CUI"])

    def test_an_image_without_the_dll_characteristic_is_refused(self):
        result = self.check(self.icd(dll=False), *self.exactly_the_icd())
        self.assertEqual(self.refusals(result), ["not a DLL: IMAGE_FILE_DLL is clear"])

    def test_another_machine_is_refused(self):
        result = self.check(self.icd(machine=0x14C), *self.exactly_the_icd())
        self.assertEqual(self.refusals(result),
                         ["machine IMAGE_FILE_MACHINE_I386, want IMAGE_FILE_MACHINE_AMD64"])

    def test_a_fourth_module_is_refused_and_the_three_guest_modules_are_not(self):
        imports = {**GUEST_IMPORTS, "user32.dll": ["MessageBoxA"]}
        result = self.check(self.icd(imports=imports), *self.exactly_the_icd())
        self.assertEqual(self.refusals(result), ["imports user32.dll, not a module every guest has"])

    def test_an_allowed_module_is_accepted_whatever_its_case_and_only_when_allowed(self):
        image = self.icd(imports={**GUEST_IMPORTS, "Vulkan-1.dll": ["vkCreateInstance"]})
        result = self.check(image, *self.exactly_the_icd(), "--allow-import", "vulkan-1.dll",
                            "--require-import", "vkCreateInstance")
        self.assertEqual(result.returncode, 0, result.stderr)
        result = self.check(image, *self.exactly_the_icd(), "--allow-import", "vulkan-2.dll")
        self.assertEqual(self.refusals(result), ["imports Vulkan-1.dll, not a module every guest has"])

    def test_the_opengl_icds_user32_and_opengl32_imports_are_accepted_only_when_allowed(self):
        # avgl.dll presents through user32 (050 research.md §6); its probe imports opengl32.
        image = self.icd(imports={**GUEST_IMPORTS, "user32.dll": ["WindowFromDC", "GetClientRect"],
                                  "opengl32.dll": ["wglCreateContext"]})
        result = self.check(image, *self.exactly_the_icd(), "--require-import", "WindowFromDC")
        self.assertEqual(self.refusals(result), ["imports user32.dll, not a module every guest has",
                                                 "imports opengl32.dll, not a module every guest has"])
        result = self.check(image, *self.exactly_the_icd(), "--allow-import", "user32.dll")
        self.assertEqual(self.refusals(result), ["imports opengl32.dll, not a module every guest has"])
        result = self.check(image, *self.exactly_the_icd(), "--allow-import", "user32.dll",
                            "--allow-import", "opengl32.dll", "--require-import", "WindowFromDC",
                            "--require-import", "wglCreateContext")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_a_delay_loaded_module_is_still_a_module_the_guest_must_have(self):
        result = self.check(self.icd(delay_imports={"d3d12.dll": ["D3D12CreateDevice"]}),
                            *self.exactly_the_icd(), "--require-import", "D3D12CreateDevice")
        self.assertEqual(self.refusals(result), ["imports d3d12.dll, not a module every guest has"])

    def test_an_import_by_ordinal_is_read_and_can_be_required(self):
        image = self.icd(imports={"kernel32.dll": [0x1234, "ExitProcess"]})
        result = self.check(image, *self.exactly_the_icd(), "--require-import", "#4660",
                            "--require-import", "ExitProcess")
        self.assertEqual(result.returncode, 0, result.stderr)
        result = self.check(image, *self.exactly_the_icd(), "--require-import", "#52")
        self.assertEqual(self.refusals(result), ["does not import #52"])

    def test_a_missing_export_is_refused(self):
        result = self.check(self.icd(exports=ICD_EXPORTS[:1]), *self.exactly_the_icd())
        self.assertEqual(self.refusals(result), [f"does not export {ICD_EXPORTS[1]}"])

    def test_an_extra_export_is_refused_only_under_only_exports(self):
        image = self.icd(exports=ICD_EXPORTS + ("vk_icdGetPhysicalDeviceProcAddr",))
        result = self.check(image, *self.exactly_the_icd())
        self.assertEqual(self.refusals(result),
                         ["exports vk_icdGetPhysicalDeviceProcAddr, which is not a required export"])
        permissive = [argument for argument in self.exactly_the_icd() if argument != "--only-exports"]
        self.assertEqual(self.check(image, *permissive).returncode, 0)

    def test_an_export_with_no_name_is_refused_under_only_exports(self):
        result = self.check(self.icd(unnamed_exports=1), *self.exactly_the_icd())
        self.assertEqual(self.refusals(result), ["exports ordinal 3 with no name"])

    def test_a_relocated_pointer_to_the_image_base_is_refused_naming_where_it_is(self):
        """A pointer the linker filled with nothing: RVA 0 is the DOS header,
        never a symbol, and calling one jumps to the image base. This is how a
        guest DLL built against a __declspec(dllimport) declaration with no
        import library to satisfy it fails, and it fails at run time."""
        result = self.check(self.icd(relocated=(IMAGE_BASE,)), *self.exactly_the_icd())
        refusals = self.refusals(result)
        self.assertEqual(len(refusals), 1, refusals)
        self.assertRegex(refusals[0], r"^the relocated pointer at RVA 0x[0-9A-F]+ holds the image base, "
                                      r"so it points at nothing$")

    def test_relocated_pointers_that_point_somewhere_are_accepted(self):
        image = self.icd(relocated=(IMAGE_BASE + SECTION_RVA, IMAGE_BASE + SECTION_RVA + 8))
        result = self.check(image, *self.exactly_the_icd())
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_every_null_pointer_is_named_not_only_the_first(self):
        image = self.icd(relocated=(IMAGE_BASE, IMAGE_BASE + SECTION_RVA, IMAGE_BASE))
        self.assertEqual(len(self.refusals(self.check(image, *self.exactly_the_icd()))), 2)

    def test_an_image_with_no_relocation_table_is_accepted(self):
        result = self.check(self.icd(), *self.exactly_the_icd())
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_a_padding_entry_relocates_nothing_and_is_not_read(self):
        """Type 0 is IMAGE_REL_BASED_ABSOLUTE, which every linker pads blocks
        with; reading a pointer at its offset would judge an unrelated word."""
        block = struct.pack("<IIHH", SECTION_RVA, 12, 0, 0)
        result = self.check(self.icd(relocation_block=block), *self.exactly_the_icd())
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_a_relocation_block_shorter_than_its_header_is_refused(self):
        block = struct.pack("<II", SECTION_RVA, 6)
        refusals = self.refusals(self.check(self.icd(relocation_block=block), *self.exactly_the_icd()))
        self.assertEqual(refusals, ["not a PE image: a relocation block of 6 bytes has no entries"])

    def test_a_relocation_block_overrunning_the_table_is_refused(self):
        block = struct.pack("<IIH", SECTION_RVA, 0x1000, (RELOCATED_DIR64 << 12))
        refusals = self.refusals(self.check(self.icd(relocation_block=block), *self.exactly_the_icd()))
        self.assertEqual(refusals, ["not a PE image: a relocation block of 4096 bytes overruns "
                                    "the 10 bytes of the table"])

    def test_a_relocation_naming_an_rva_in_no_section_is_refused(self):
        block = struct.pack("<IIH", 0x900000, 10, (RELOCATED_DIR64 << 12) | 0x20)
        refusals = self.refusals(self.check(self.icd(relocation_block=block), *self.exactly_the_icd()))
        self.assertEqual(refusals, ["not a PE image: RVA 0x900020 is in no section's file data"])

    def test_a_truncated_image_is_refused_as_not_a_pe_image(self):
        image = self.icd()
        result = self.check(image[:0x180], *self.exactly_the_icd())
        refusals = self.refusals(result)
        self.assertEqual(len(refusals), 1)
        self.assertRegex(refusals[0], r"^not a PE image: \d+ bytes at 0x[0-9A-F]+ are past the end of the file$")

    def test_an_export_naming_a_function_past_the_table_is_refused(self):
        image = bytearray(self.icd())
        directory = struct.unpack_from("<I", image, OPTIONAL_AT + 112)[0]
        functions_at = FILE_ALIGNMENT + directory - SECTION_RVA + 20
        struct.pack_into("<I", image, functions_at, 1)
        result = self.check(bytes(image), *self.exactly_the_icd())
        self.assertEqual(len(self.refusals(result)), 1)
        self.assertRegex(result.stderr, r"not a PE image: export vk_icd\w+ names function 1 of 1")


class HostileImages(unittest.TestCase):
    """The parse in-process, so every prefix and every flipped byte is judged
    quickly: an image is either read or refused as Malformed, never anything
    else."""

    def parse(self, data):
        image = check_guest_image.PeImage(data)
        return image.imports(), image.exports()

    def test_the_full_image_reads_back_what_was_laid_out(self):
        imports, (named, unnamed) = self.parse(synthetic_pe(
            imports=GUEST_IMPORTS, delay_imports={"d3d12.dll": ["D3D12CreateDevice"]},
            exports=ICD_EXPORTS, unnamed_exports=2))
        self.assertEqual(imports, [(module, symbols) for module, symbols in GUEST_IMPORTS.items()]
                         + [("d3d12.dll", ["D3D12CreateDevice"])])
        self.assertEqual(sorted(named), sorted(ICD_EXPORTS))
        self.assertEqual(unnamed, [3, 4])

    def test_every_prefix_that_cuts_a_table_is_refused(self):
        image = synthetic_pe(imports=GUEST_IMPORTS, delay_imports={"d3d12.dll": ["D3D12CreateDevice"]},
                             exports=ICD_EXPORTS, unnamed_exports=1)
        used = len(image.rstrip(b"\0"))
        for length in range(used):
            with self.subTest(length=length), self.assertRaises(check_guest_image.Malformed):
                self.parse(image[:length])

    def test_every_flipped_byte_is_read_or_refused(self):
        image = synthetic_pe(imports=GUEST_IMPORTS, delay_imports={"d3d12.dll": [5]},
                             exports=ICD_EXPORTS, unnamed_exports=1)
        refused = 0
        for offset in range(len(image.rstrip(b"\0"))):
            for mask in (0x01, 0x80, 0xFF):
                flipped = bytearray(image)
                flipped[offset] ^= mask
                try:
                    self.parse(bytes(flipped))
                except check_guest_image.Malformed:
                    refused += 1
        self.assertGreater(refused, 0)

    def test_optional_header_too_small_for_its_directories_is_refused(self):
        image = bytearray(synthetic_pe(exports=ICD_EXPORTS))
        struct.pack_into("<H", image, 0x44 + 16, OPTIONAL_SIZE - 8)
        with self.assertRaisesRegex(check_guest_image.Malformed, "overrun the optional header"):
            self.parse(bytes(image))

    def test_a_table_straddling_the_end_of_its_section_is_refused(self):
        """The file holds padding past the section's virtual size; a read that
        starts inside the section and ends in that padding is still refused."""
        image = bytearray(synthetic_pe(exports=ICD_EXPORTS))
        directory = struct.unpack_from("<I", image, OPTIONAL_AT + 112)[0]
        struct.pack_into("<I", image, OPTIONAL_AT + OPTIONAL_SIZE + 8, directory + 38 - SECTION_RVA)
        with self.assertRaisesRegex(check_guest_image.Malformed, r"RVA 0x\w+ is in no section's file data"):
            self.parse(bytes(image))

    def test_a_name_with_no_terminator_is_refused(self):
        """Section data that runs on past the longest name a linker writes."""
        image = bytearray(synthetic_pe(imports={}))
        header = OPTIONAL_AT + OPTIONAL_SIZE
        unterminated = SECTION_RVA + len(image) - FILE_ALIGNMENT
        image += b"x" * (check_guest_image.LONGEST_NAME + 1)
        struct.pack_into("<II", image, header + 8, len(image) - FILE_ALIGNMENT, SECTION_RVA)
        struct.pack_into("<I", image, header + 16, len(image) - FILE_ALIGNMENT)
        parsed = check_guest_image.PeImage(bytes(image))
        with self.assertRaisesRegex(check_guest_image.Malformed, "has no terminator"):
            parsed.string_at(unterminated)


if __name__ == "__main__":
    unittest.main()
