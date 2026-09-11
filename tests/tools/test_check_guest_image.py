import pathlib
import shutil
import tempfile
import unittest

from tool_run import ROOT, run

CHECK = ROOT / "tools" / "check_guest_image.py"

PROGRAM = """extern "C" __declspec(dllimport) void ExitProcess(unsigned code);
extern "C" __declspec(dllimport) long D3DKMTEnumAdapters2(void *request);
extern "C" void entry() { D3DKMTEnumAdapters2(nullptr); ExitProcess(0); }
"""


class Image:
    """A small x64 image importing `modules`, linked for `subsystem`."""

    def __init__(self, directory, name, subsystem, modules):
        self.path = directory / f"{name}.exe"
        (directory / "program.cpp").write_text(PROGRAM)
        libraries = []
        for module, symbols in modules.items():
            definition = directory / f"{name}-{module}.def"
            definition.write_text(f"LIBRARY {module}\nEXPORTS\n" + "\n".join(symbols) + "\n")
            library = directory / f"{name}-{module}.lib"
            self.step("llvm-dlltool", "-m", "i386:x86-64", "-d", definition, "-l", library)
            libraries.append(library)
        self.step("clang-cl", "--target=x86_64-pc-windows-msvc", "/nologo", "/c", "/GS-",
                  f"/Fo{directory / 'program.obj'}", directory / "program.cpp")
        self.step("lld-link", "/nologo", f"/SUBSYSTEM:{subsystem}", "/ENTRY:entry", "/NODEFAULTLIB",
                  f"/OUT:{self.path}", directory / "program.obj", *libraries)

    @staticmethod
    def step(*command):
        result = run(*command)
        if result.returncode != 0:
            raise AssertionError(result.stdout + result.stderr)


class GuestImageCheck(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scratch = pathlib.Path(tempfile.mkdtemp())
        guest = {"gdi32.dll": ["D3DKMTEnumAdapters2"], "kernel32.dll": ["ExitProcess"]}
        cls.good = Image(cls.scratch, "good", "CONSOLE", guest)
        cls.window = Image(cls.scratch, "window", "WINDOWS", guest)
        cls.foreign = Image(cls.scratch, "foreign", "CONSOLE",
                            {"d3d12.dll": ["D3DKMTEnumAdapters2"], "kernel32.dll": ["ExitProcess"]})

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
        self.assertIn("llvm-readobj failed", result.stderr)


if __name__ == "__main__":
    unittest.main()
