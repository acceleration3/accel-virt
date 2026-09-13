"""The protocol ids in the documentation and in the generator's id files agree.

docs/protocol.md says which API owns which range of command ids; the id files
are what the generator actually emits. FR-007: the two say the same thing, or
the gate does.
"""
import pathlib
import shutil
import tempfile
import textwrap
import unittest

from tool_run import ROOT, run

CHECK = ROOT / "tools" / "check_protocol_ids.py"

DOC = """# A protocol

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | id | top 16 bits: the API - 0 transport, 1 Vulkan, 2 OpenCL, 3 OpenGL |
"""


class CheckProtocolIds(unittest.TestCase):
    def setUp(self):
        self.scratch = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.scratch)
        self.doc = self.scratch / "protocol.md"
        self.doc.write_text(DOC)
        self.profiles = self.scratch / "profiles"
        self.profiles.mkdir()
        self.write_ids("vulkan", {"vkCreateInstance": 0x10001})
        self.write_ids("opencl", {"clGetPlatformIDs": 0x20000})
        self.write_ids("opengl", {"glClear": 0x30000})

    def write_ids(self, api, commands):
        body = "\n".join(f'"{name}" = {value}' for name, value in commands.items())
        (self.profiles / f"{api}_ids.toml").write_text(f"[commands]\n{body}\n")

    def check(self):
        return run(CHECK, "--doc", self.doc, "--profiles", self.profiles)

    def test_ids_inside_their_api_s_range_pass(self):
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("vulkan", result.stdout)

    def test_an_id_in_another_api_s_range_fails_naming_the_command(self):
        self.write_ids("vulkan", {"vkCreateInstance": 0x10001, "vkCreateDevice": 0x20002})
        result = self.check()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("vkCreateDevice", result.stdout + result.stderr)

    def test_two_commands_sharing_an_id_fail(self):
        self.write_ids("vulkan", {"vkCreateInstance": 0x10001, "vkCreateDevice": 0x10001})
        result = self.check()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("0x10001", result.stdout + result.stderr)

    def test_an_api_the_document_names_with_no_id_file_fails(self):
        (self.profiles / "opengl_ids.toml").unlink()
        result = self.check()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("opengl", result.stdout + result.stderr)

    def test_an_id_file_the_document_does_not_name_fails(self):
        self.write_ids("direct3d", {"d3dNothing": 0x40000})
        result = self.check()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("direct3d", result.stdout + result.stderr)

    def test_a_document_with_no_id_table_fails_rather_than_passing_silently(self):
        self.doc.write_text("# A protocol\n\nNothing about ids at all.\n")
        result = self.check()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("protocol.md", result.stdout + result.stderr)

    def test_the_repository_s_own_documentation_and_id_files_agree(self):
        """What ships."""
        result = run(CHECK)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
