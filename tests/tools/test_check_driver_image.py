import pathlib
import shutil
import tempfile
import unittest

from kernel_probe import DRIVER_ENTRY, KIT, KernelProbeProject
from tool_run import ROOT, run

CHECK = ROOT / "tools" / "check_driver_image.py"

USER_MODE = """extern "C" __declspec(dllimport) void ExitProcess(unsigned code);
int main() { ExitProcess(0); return 0; }
"""


class UserModeImage:
    """A console executable importing KERNEL32.dll: everything a driver must not be."""

    def __init__(self, directory):
        self.directory = directory
        self.path = directory / "user.exe"

    def build(self):
        d = self.directory
        (d / "user.cpp").write_text(USER_MODE)
        (d / "kernel32.def").write_text("LIBRARY KERNEL32.dll\nEXPORTS\nExitProcess\n")
        steps = [
            ("llvm-dlltool", "-m", "i386:x86-64", "-D", "KERNEL32.dll",
             "-d", d / "kernel32.def", "-l", d / "kernel32.lib"),
            ("clang-cl", "--target=x86_64-pc-windows-msvc", "/nologo", "/c", "/GS-",
             f"/Fo{d / 'user.obj'}", d / "user.cpp"),
            ("lld-link", "/nologo", "/SUBSYSTEM:CONSOLE", "/ENTRY:main", "/NODEFAULTLIB",
             f"/OUT:{self.path}", d / "user.obj", d / "kernel32.lib"),
        ]
        for step in steps:
            result = run(*step)
            if result.returncode != 0:
                raise AssertionError(result.stdout + result.stderr)


@unittest.skipUnless((KIT / "VERSION").exists(),
                     "no Windows kit: run tools/fetch_windows_kit.sh")
class DriverImageCheck(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scratch = pathlib.Path(tempfile.mkdtemp())
        cls.project = KernelProbeProject({"driver": DRIVER_ENTRY})
        configured = cls.project.configure()
        code, output = cls.project.build_target("driver")
        if configured.returncode != 0 or code != 0:
            cls.tearDownClass()
            raise AssertionError(configured.stderr + output)
        cls.user = UserModeImage(cls.scratch)
        cls.user.build()

    @classmethod
    def tearDownClass(cls):
        cls.project.remove()
        shutil.rmtree(cls.scratch)

    def test_accepts_a_native_image_importing_only_the_kernel(self):
        result = run(CHECK, self.project.image("driver"))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("driver_image: ok", result.stdout)

    def test_rejects_a_user_mode_subsystem(self):
        result = run(CHECK, self.user.path)
        self.assertEqual(result.returncode, 1)
        self.assertIn("subsystem IMAGE_SUBSYSTEM_WINDOWS_CUI, want IMAGE_SUBSYSTEM_NATIVE",
                      result.stderr)

    def test_rejects_a_non_kernel_import(self):
        result = run(CHECK, self.user.path)
        self.assertEqual(result.returncode, 1)
        self.assertIn("imports KERNEL32.dll, not a kernel module", result.stderr)

    def test_rejects_a_missing_image_naming_it(self):
        missing = self.scratch / "absent.sys"
        result = run(CHECK, missing)
        self.assertEqual(result.returncode, 1)
        self.assertIn(str(missing), result.stderr)


if __name__ == "__main__":
    unittest.main()
