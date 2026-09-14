"""Generate ``vk_supported_extensions.h``: what this build can actually do.

The ICD answers ``vkEnumerateDeviceExtensionProperties`` by forwarding
the question to the server, which asks the real driver — so the list an
application sees is every device extension that GPU supports. The ICD
implements the four dozen in ``profile.toml``.

Nothing catches the difference. An application enables an extension it
was offered, asks for one of its entry points, gets null from
``vkGetDeviceProcAddr`` because no such command was ever generated, and
calls it. That is a jump to address zero with no Vulkan error raised
anywhere along the way — the failure mode that took a D3D8 title down
inside DXVK, four frames deep with an empty backtrace.

Advertising only what exists is the fix, and the list has to be
generated: maintained by hand next to the generator it would be wrong
the first time an extension was added or removed.
"""

from __future__ import annotations

from pathlib import Path

from ..ir import Registry
from .common import GENERATED_BANNER, header_guard, write


def emit(reg: Registry, out_dir: Path) -> None:
    device = sorted(n for n, kind in reg.implemented_extensions.items() if kind == "device")
    instance = sorted(n for n, kind in reg.implemented_extensions.items() if kind == "instance")

    out = [
        GENERATED_BANNER,
        "",
        "#include <stddef.h>",
        "",
        "namespace av::vkproto {",
        "",
        "// Device extensions this build generated entry points for.",
        "//",
        "// The ICD filters the driver's list against this one before",
        "// handing it to the application. An extension missing here is not",
        "// necessarily unsupported by the GPU — it is unsupported by THIS",
        "// proxy, which is the only claim the ICD is entitled to make.",
        "inline constexpr const char* kImplementedDeviceExtensions[] = {",
    ]
    for name in device:
        out.append(f'    "{name}",')
    out += [
        "};",
        "",
        "// Instance extensions, for the same reason. The ICD answers",
        "// instance enumeration from a hand-written table rather than the",
        "// driver, so this is a cross-check rather than a filter.",
        "inline constexpr const char* kImplementedInstanceExtensions[] = {",
    ]
    for name in instance:
        # The platform surface-creation extension is chosen at compile
        # time: the profile names the Xlib one because that is what the
        # host build presents through, but the same source cross-compiled
        # for Windows presents through Win32 and must advertise that
        # name instead. Emitting both behind an #if keeps one generated
        # header honest for both targets.
        if name == "VK_KHR_xlib_surface":
            out += [
                "    // The platform surface-creation extension is the one the"
                " presenter",
                "    // is built for: Xlib presents with XPutImage, Win32 with",
                "    // StretchDIBits. Advertising the other platform's name"
                " would offer a",
                "    // surface nothing on this build can present through.",
                "#if defined(_WIN32)",
                '    "VK_KHR_win32_surface",',
                "#else",
                '    "VK_KHR_xlib_surface",',
                "#endif",
            ]
        else:
            out.append(f'    "{name}",')
    out += [
        "};",
        "",
        "// Entry points the ICD must provide itself.",
        "//",
        "// These are ``[commands].exclude`` in the profile: commands that",
        "// cannot be generated because they carry something no wire format",
        "// holds — a window, a mapped pointer, a surface naming a window in",
        "// this process. The lists above count them as implemented, which",
        "// is what keeps VK_KHR_surface claimable at all.",
        "//",
        "// That makes this list a promise, and the ICD checks it at",
        "// startup: a name here that its own table does not answer is an",
        "// extension advertised over a null pointer, which is how a D3D8",
        "// title came to exit with status 1 and no message.",
        "inline constexpr const char* kManualEntryPoints[] = {",
    ]
    for name in sorted(reg.manual_commands):
        # The platform surface commands follow the extension: the build
        # that advertises VK_KHR_win32_surface must promise its two
        # commands, and the Xlib build its own. Promising the absent
        # platform's commands would make the startup self-check report a
        # gap that is correct by design.
        if name == "vkCreateXlibSurfaceKHR":
            out += [
                "#if defined(_WIN32)",
                '    "vkCreateWin32SurfaceKHR",',
                "#else",
                '    "vkCreateXlibSurfaceKHR",',
                "#endif",
            ]
        elif name == "vkGetPhysicalDeviceXlibPresentationSupportKHR":
            out += [
                "#if defined(_WIN32)",
                '    "vkGetPhysicalDeviceWin32PresentationSupportKHR",',
                "#else",
                '    "vkGetPhysicalDeviceXlibPresentationSupportKHR",',
                "#endif",
            ]
        else:
            out.append(f'    "{name}",')
    out += [
        "};",
        "",
        "}  // namespace av::vkproto",
        "",
    ]
    write(out_dir / "vk_supported_extensions.h",
          header_guard("\n".join(out[1:])))
