import unittest

from kernel_probe import DRIVER_ENTRY, KIT, KernelProbeProject
from tool_run import ROOT, run

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


KERNEL_PLATFORM_ROUND_TRIP = """#include <ntddk.h>
#include "kernel_platform.hpp"
#include "mutex.hpp"
#include "mutex_guard.hpp"
#include "spin_lock.hpp"
#include "spin_lock_guard.hpp"

extern "C" DRIVER_INITIALIZE DriverEntry;

static NTSTATUS round_trip()
{
    av::KernelPlatform platform(0x6b767272ul);
    av::SpinLock lock(platform, av::LockRank{2});
    av::Mutex mutex(platform, av::LockRank{1});
    {
        av::MutexGuard outer(mutex);
        av::SpinLockGuard inner(lock);
    }
    void *block = platform.allocate(64u, av::Pool::non_paged);
    platform.zero(block, 64u);
    platform.release(block);
    platform.log("round trip");
    return platform.interrupt_time() != 0u ? STATUS_NOT_SUPPORTED : STATUS_UNSUCCESSFUL;
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT, PUNICODE_STRING)
{
    return round_trip();
}
"""


@unittest.skipUnless((KIT / "VERSION").exists(),
                     "no Windows kit: run tools/fetch_windows_kit.sh")
class KernelPlatformLinks(unittest.TestCase):
    def test_a_driver_using_the_kernel_platform_imports_only_the_kernel(self):
        project = KernelProbeProject(
            {"platform": KERNEL_PLATFORM_ROUND_TRIP},
            sources=("src/km/kernel_platform.cpp", "src/core/spin_lock.cpp",
                     "src/core/spin_lock_guard.cpp", "src/core/mutex.cpp",
                     "src/core/mutex_guard.cpp"),
            include_directories=("src/core", "src/km"))
        self.addCleanup(project.remove)
        configured = project.configure()
        self.assertEqual(configured.returncode, 0, configured.stdout + configured.stderr)
        code, output = project.build_target("platform")
        self.assertEqual(code, 0, output)
        image = run("python3", "-B", ROOT / "tools" / "check_driver_image.py", project.image("platform"))
        self.assertEqual(image.returncode, 0, image.stderr)
        imports = run("llvm-readobj", "--coff-imports", project.image("platform")).stdout
        for routine in ("ExAllocatePool2", "ExFreePoolWithTag", "KeAcquireSpinLockRaiseToDpc",
                        "KeReleaseSpinLock", "ExAcquireFastMutex", "ExReleaseFastMutex",
                        "DbgPrintEx"):
            self.assertIn(routine, imports)


if __name__ == "__main__":
    unittest.main()
