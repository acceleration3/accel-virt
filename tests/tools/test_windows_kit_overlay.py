import json
import pathlib
import shutil
import tempfile
import unittest

from tool_run import ROOT, run

OVERLAY = ROOT / "tools" / "windows_kit_overlay.py"


class WindowsKitOverlay(unittest.TestCase):
    """The kit's headers were written for a case-insensitive filesystem."""

    def setUp(self):
        self.kit = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.kit)
        km = self.kit / "include" / "km" / "crt"
        km.mkdir(parents=True)
        shared = self.kit / "include" / "sdk-shared"
        shared.mkdir(parents=True)
        (self.kit / "include" / "km" / "Main.h").write_text('#include "driverSPECS.h"\n')
        (km / "stddef.h").write_text("#define KIT_CRT 1\n")
        (shared / "driverspecs.h").write_text("#define KIT_MARK 42\n")
        um = self.kit / "include" / "um"
        um.mkdir(parents=True)
        (um / "Windows.h").write_text("#define KIT_USER 1\n")

    def generate(self):
        result = run(OVERLAY, self.kit)
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads((self.kit / "overlay.yaml").read_text())

    def test_is_case_insensitive_relative_and_keeps_virtual_names(self):
        overlay = self.generate()
        self.assertEqual(overlay["case-sensitive"], "false")
        self.assertIs(overlay["overlay-relative"], True)
        self.assertIs(overlay["use-external-names"], False)

    def test_skips_a_missing_root_instead_of_failing(self):
        names = [root["name"] for root in self.generate()["roots"]]
        self.assertEqual(names, ["/windows-kit/km", "/windows-kit/sdk-shared", "/windows-kit/um"])

    def test_lists_every_file_explicitly_with_a_relative_path(self):
        files = []

        def walk(entries):
            for entry in entries:
                if entry["type"] == "directory":
                    walk(entry["contents"])
                else:
                    files.append(entry["external-contents"])

        walk(self.generate()["roots"])
        self.assertEqual(sorted(files), ["include/km/Main.h",
                                         "include/km/crt/stddef.h",
                                         "include/sdk-shared/driverspecs.h",
                                         "include/um/Windows.h"])

    def test_resolves_an_include_written_in_the_wrong_case(self):
        self.generate()
        probe = self.kit / "probe.cpp"
        probe.write_text('#include <Main.h>\nstatic int value = KIT_MARK;\n')
        result = run("clang++", "-fsyntax-only", "-Wno-unused-variable",
                     "-ivfsoverlay", self.kit / "overlay.yaml",
                     "-I/windows-kit/km", "-I/windows-kit/sdk-shared", probe)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
