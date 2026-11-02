import pathlib
import shutil
import tempfile

from tool_run import ROOT, run

KIT = ROOT / "third_party" / "windows-kit"
TOOLCHAIN = ROOT / "cmake" / "windows_kernel_toolchain.cmake"


class KernelProbeProject:
    def __init__(self, probes, sources=(), include_directories=()):
        self.directory = pathlib.Path(tempfile.mkdtemp())
        self.build = self.directory / "build"
        lines = ["cmake_minimum_required(VERSION 3.24)", "project(probes CXX)",
                 "set(CMAKE_CXX_STANDARD 23)"]
        shared = " ".join(str(ROOT / source) for source in sources)
        includes = " ".join(str(ROOT / directory) for directory in include_directories)
        for name, source in probes.items():
            (self.directory / f"{name}.cpp").write_text(source)
            lines.append(f"add_executable({name} {name}.cpp {shared})")
            lines.append(f"set_target_properties({name} PROPERTIES SUFFIX .sys)")
            if includes:
                lines.append(f"target_include_directories({name} PRIVATE {includes})")
        (self.directory / "CMakeLists.txt").write_text("\n".join(lines) + "\n")

    def configure(self):
        return run("cmake", "-S", self.directory, "-B", self.build, "-G", "Ninja",
                   f"-DCMAKE_TOOLCHAIN_FILE={TOOLCHAIN}",
                   f"-DAV_WINDOWS_KIT_DIR={KIT}")

    def build_target(self, name):
        result = run("cmake", "--build", self.build, "--target", name)
        return result.returncode, result.stdout + result.stderr

    def image(self, name):
        return self.build / f"{name}.sys"

    def remove(self):
        shutil.rmtree(self.directory)


DRIVER_ENTRY = """#include <ntddk.h>
#include <dispmprt.h>
extern "C" DRIVER_INITIALIZE DriverEntry;
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT, PUNICODE_STRING)
{
    return DXGKDDI_INTERFACE_VERSION_WDDM3_2 ? STATUS_NOT_SUPPORTED : STATUS_SUCCESS;
}
"""
