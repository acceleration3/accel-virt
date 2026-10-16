#!/usr/bin/env python3
"""Gate for constitution Principle IV.5: no class or public method in
src/core, src/miniport, src/device, src/proto, src/guest or src/icd without a test that names
it.

Private methods are covered through the public surface; requiring a test to
name them would test the shape of the implementation instead of its behaviour.
The harness is not a test: a fixture calling a method proves nothing about it.

  check_symbol_tests.py [repository root]
"""
import os
import re
import sys

from header_scan import declared_classes, is_shim

# Headers that declare a *required* interface - what core calls and the
# platform provides - rather than a provided one. Testing these would test the
# mock. Every entry needs a reason; the list is short on purpose.
EXCLUDE = {
    "src/core/platform/platform.hpp": "the platform contract, implemented by src/km and "
                             "tests/harness and exercised through every core test",
    "src/core/platform/register_window.hpp": "a device register region, implemented by src/km over a "
                                    "mapped BAR and by the fake device in tests/harness, and "
                                    "exercised through every transport test",
    "src/guest/kmt/kmt.hpp": "the D3DKMT contract, implemented by src/um over gdi32 and by MockKmt "
                             "in tests/harness, and exercised through every guest suite test",
    "src/guest/probe/vulkan_entry.hpp": "the Vulkan the probe calls, implemented by LoaderVulkan and by "
                             "ScriptedVulkan in tests/harness, and exercised through every probe test",
    "src/guest/probe/gl_entry.hpp": "the WGL and GL the GL probe calls, implemented by the system opengl32 in "
                             "src/um, by Opengl32GlEntry over FakeOpengl32 and by ScriptedGl in tests/harness, "
                             "and exercised through every GL probe test",
    "src/guest/probe/d3d11_entry.hpp": "the Direct3D 11 the D3D probe calls, implemented by "
                             "WindowsD3d11Entry over DXVK's d3d11.dll in src/um and by ScriptedD3d11 in "
                             "tests/harness, and exercised through every D3d11Work test",
    "src/guest/probe/d3d12_entry.hpp": "the Direct3D 12 the D3D probe calls, implemented by "
                             "WindowsD3d12Entry over vkd3d-proton's d3d12.dll in src/um and by ScriptedD3d12 "
                             "in tests/harness, and exercised through every D3d12Work test",
    "src/guest/probe/d3d9_entry.hpp": "the Direct3D 9 the D3D probe calls, implemented by "
                             "WindowsD3d9Entry over DXVK's d3d9.dll in src/um and by ScriptedD3d9 in "
                             "tests/harness, and exercised through every D3d9Work test",
    "src/guest/probe/d3d11_deferred_entry.hpp": "the Direct3D 11 the D3D11 probe's deferred-map scenario "
                             "calls, implemented by WindowsD3d11DeferredEntry over the system's d3d11.dll in "
                             "src/um and by ScriptedD3d11Deferred in tests/harness, and exercised through "
                             "every DeferredMapWork test",
    "src/guest/probe/d3d11_shared_entry.hpp": "the Direct3D 11 the D3D11 probe's shared-clear scenario calls, "
                             "implemented by WindowsD3d11SharedEntry over the system's d3d11.dll in src/um and by "
                             "ScriptedD3d11Shared in tests/harness, and exercised through every SharedClearWork test",
    "src/guest/probe/d3d11_tir_entry.hpp": "the Direct3D 11 the D3D11 probe's target-independent rasterization "
                             "scenario calls, implemented by WindowsD3d11TirEntry over the system's d3d11.dll in "
                             "src/um and by ScriptedD3d11Tir in tests/harness, and exercised through every "
                             "TirCoverageWork test",
    "src/guest/probe/d3d9ex_entry.hpp": "the Direct3D 9Ex the D3D9 probe's WPF scenario calls, implemented "
                             "by WindowsD3d9ExEntry over the system's d3d9.dll in src/um and by ScriptedD3d9Ex "
                             "in tests/harness, and exercised through every WpfPresentWork test",
    "src/executor/session_backend.hpp": "what a session requires of a host API, implemented "
                             "by the Vulkan, OpenGL and OpenCL backends and by MockSessionBackend "
                             "in tests/harness, and exercised through every session test",
    "src/device/context/context_plane.hpp": "where a device's contexts run, implemented by LocalContextPlane, "
                             "RendererContextPlane and RecordingContextPlane in tests/harness, and exercised "
                             "through every plane and control-command test",
    "src/device/renderer/renderer_resources.hpp": "what the renderer's service requires of its process, "
                             "implemented by PosixRendererHost and by MockRendererResources in tests/harness, "
                             "and exercised through every renderer service test",
    "src/icd/opencl/d3d_adapter_source.hpp": "the DXGI the OpenCL ICD asks for an adapter's LUID, implemented "
                             "by WindowsD3dAdapterSource in src/um and by ScriptedD3dAdapterSource in "
                             "tests/harness, and exercised through every cl_d3d10_sharing test",
    "src/icd/opencl/gl_share_source.hpp": "the opengl32 and OpenGL ICD the OpenCL ICD asks for a GL context's "
                             "share name, implemented by WindowsGlShareSource in src/um and by "
                             "ScriptedGlShareSource in tests/harness, and exercised through every "
                             "cl_gl_sharing test",
    "src/icd/opencl/d3d11_bridge.hpp": "the Direct3D 11 the OpenCL ICD copies through, implemented by "
                             "WindowsD3d11Bridge in src/um and by ScriptedD3d11Bridge in tests/harness, and "
                             "exercised through every cl_d3d11_sharing test",
    "src/icd/opencl/d3d11_rows.hpp": "what is done with a mapped subresource, implemented by ClD3d11Upload and "
                             "ClD3d11Download, whose own tests call it",
}


def test_corpus(root):
    texts = []
    for base, _dirs, files in os.walk(os.path.join(root, "tests")):
        if os.path.join("tests", "harness") in os.path.relpath(base, root) + os.sep:
            continue
        for name in files:
            if name.endswith((".cpp", ".hpp")):
                with open(os.path.join(base, name)) as source:
                    texts.append(source.read())
    return "\n".join(texts)


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    corpus = test_corpus(root)
    violations = []
    for declared in declared_classes(root):
        if declared.header in EXCLUDE:
            continue
        # Constitution II: the shim is the non-portable edge - dlopen, an
        # instance, an enumeration - and a unit test on a machine that may have
        # no GPU cannot name it into existence. L3 and L5 prove it instead, and
        # the directory is the rule so moving a file out puts the gate back
        # (feature 042).
        if is_shim(declared.header):
            continue
        # A type private to its class is covered through that class's public
        # surface, as its private methods are.
        if declared.is_private_nested:
            continue
        if not re.search(r"\b" + re.escape(declared.name) + r"\b", corpus):
            violations.append(f"{declared.header}: class {declared.name} has no test naming it")
        for method in declared.public_methods:
            if not re.search(r"\b" + re.escape(method) + r"\b", corpus):
                violations.append(
                    f"{declared.header}: {declared.name}::{method} has no test naming it")
    for violation in violations:
        print(violation, file=sys.stderr)
    if violations:
        sys.exit(1)
    print("symbol_tests: ok")


if __name__ == "__main__":
    main()
