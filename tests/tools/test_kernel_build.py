import unittest

from kernel_probe import DRIVER_ENTRY, KIT, KernelProbeProject

PROBES = {
    "hosted_header": "#include <vector>\n" + DRIVER_ENTRY,
    "crt_function": DRIVER_ENTRY + """
extern "C" void *fopen(const char *, const char *);
void *open_file() { return fopen("x", "r"); }
""",
    "heap_operator": DRIVER_ENTRY + "int *grab() { return new int(1); }\n",
    "exception": DRIVER_ENTRY + "void fail() { throw 1; }\n",
    "runtime_type": DRIVER_ENTRY + """
struct Shape { virtual ~Shape(); };
const char *name_of(Shape &shape) { return typeid(shape).name(); }
""",
    "driver": DRIVER_ENTRY,
}


@unittest.skipUnless((KIT / "VERSION").exists(),
                     "no Windows kit: run tools/fetch_windows_kit.sh")
class KernelBuildRejectsWhatTheKernelLacks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.project = KernelProbeProject(PROBES)
        configured = cls.project.configure()
        if configured.returncode != 0:
            cls.project.remove()
            raise AssertionError(configured.stdout + configured.stderr)

    @classmethod
    def tearDownClass(cls):
        cls.project.remove()

    def assert_rejected(self, target, needle):
        code, output = self.project.build_target(target)
        self.assertNotEqual(code, 0, output)
        self.assertIn(needle, output)

    def test_a_hosted_standard_header_is_not_found(self):
        self.assert_rejected("hosted_header", "'vector' file not found")

    def test_a_c_runtime_function_is_unresolved(self):
        self.assert_rejected("crt_function", "undefined symbol: fopen")

    def test_the_heap_operator_is_unresolved(self):
        self.assert_rejected("heap_operator", "operator new")

    def test_throw_is_rejected(self):
        self.assert_rejected("exception", "exceptions disabled")

    def test_runtime_type_information_is_rejected(self):
        self.assert_rejected("runtime_type", "typeid")

    def test_a_display_miniport_entry_point_builds(self):
        code, output = self.project.build_target("driver")
        self.assertEqual(code, 0, output)
        self.assertTrue(self.project.image("driver").exists())


if __name__ == "__main__":
    unittest.main()
