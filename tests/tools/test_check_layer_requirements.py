"""Every extension a pinned layer requires is one the ICD reports, or the gate
names the one that is not (tools/check_layer_requirements.py, feature 047
FR-001).

Driven over a repository this test builds: a policy table and a layer source,
so what is judged is the comparison rather than today's real tables.
"""
import pathlib
import tempfile
import textwrap
import unittest

from tool_run import ROOT, run

CHECK = ROOT / "tools" / "check_layer_requirements.py"

TABLE = textwrap.dedent('''
    constexpr ExtensionPolicy::Row reported_extensions[] = {
        {"VK_EXT_robustness2", 1u, "some.test"},
        {"VK_KHR_push_descriptor", 2u, "some.other"},
    };
    constexpr InstanceExtension reported_instance_extensions[] = {
        {"VK_KHR_get_physical_device_properties2", 2u},
    };
''')


class CheckLayerRequirements(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.directory.name)
        self.addCleanup(self.directory.cleanup)
        table = self.root / "src" / "icd" / "vulkan"
        table.mkdir(parents=True)
        (table / "icd_root.cpp").write_text(TABLE)
        tools = self.root / "tools"
        tools.mkdir()
        # The gate runs the real extractor, and the real registry header.
        (tools / "layer_requirements.py").write_text(
            (ROOT / "tools" / "layer_requirements.py").read_text().replace(
                'ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))',
                f'ROOT = {str(ROOT)!r}'))

    def layer_source(self, layer, body):
        source = self.root / "third_party" / layer / "source"
        source.mkdir(parents=True)
        (source / "device.c").write_text(body)
        return source

    def check(self):
        return run(CHECK, self.root)

    def test_a_layer_that_is_not_fetched_is_skipped_and_said_so(self):
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("dxvk: skipped", result.stdout)
        self.assertIn("vkd3d-proton: skipped", result.stdout)
        self.assertIn("ok", result.stdout)

    def test_a_requirement_the_icd_reports_passes(self):
        self.layer_source("dxvk", textwrap.dedent("""
            DxvkExt extRobustness2 = { VK_EXT_ROBUSTNESS_2_EXTENSION_NAME, DxvkExtMode::Required };
        """))
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("dxvk: 1 required, 0 not reported", result.stdout)

    def test_a_requirement_the_icd_does_not_report_is_named(self):
        self.layer_source("dxvk", textwrap.dedent("""
            DxvkExt extRobustness2 = { VK_EXT_ROBUSTNESS_2_EXTENSION_NAME, DxvkExtMode::Required };
            DxvkExt extMeshShader = { VK_EXT_MESH_SHADER_EXTENSION_NAME, DxvkExtMode::Required };
        """))
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("dxvk requires VK_EXT_mesh_shader, which this ICD does not report", result.stderr)

    def test_what_the_loader_answers_is_not_the_driver_s_to_report(self):
        self.layer_source("vkd3d-proton", textwrap.dedent("""
            static const char * const required_device_extensions[] = {
                VK_KHR_SURFACE_EXTENSION_NAME,
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            };
        """))
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("vkd3d-proton: 2 required, 0 not reported", result.stdout)

    def test_a_fetched_layer_that_requires_nothing_is_a_failure(self):
        self.layer_source("dxvk", "/* no extension names at all */\n")
        result = self.check()
        self.assertEqual(result.returncode, 1)
        self.assertIn("names no required extension at all", result.stderr)

    def test_a_repository_with_no_policy_table_is_an_error(self):
        (self.root / "src" / "icd" / "vulkan" / "icd_root.cpp").write_text("/* nothing */\n")
        result = self.check()
        self.assertEqual(result.returncode, 2)
        self.assertIn("no policy table", result.stderr)


if __name__ == "__main__":
    unittest.main()
