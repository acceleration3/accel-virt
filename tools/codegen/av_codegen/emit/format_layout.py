from __future__ import annotations

from pathlib import Path

from ..ir import Registry
from .common import GENERATED_BANNER, header_guard, write


def _depth_bytes(bits: int) -> int:
    return 2 if bits == 16 else 4


def emit(reg: Registry, out_dir: Path) -> None:
    out = [
        GENERATED_BANNER,
        "",
        "#include <stdint.h>",
        "",
        "#include <vulkan/vulkan_core.h>",
        "",
        "namespace av::vkproto {",
        "",
        "struct FormatLayout {",
        "    uint32_t block_bytes;",
        "    uint32_t block_width;",
        "    uint32_t block_height;",
        "};",
        "",
        "inline bool format_layout(VkFormat format, VkImageAspectFlagBits aspect, FormatLayout& out) {",
        "    switch (format) {",
    ]
    for f in reg.formats:
        name = f["name"]
        out.append(f"    case {name}:")
        if f["depth_bits"] or f["stencil_bits"]:
            if f["depth_bits"]:
                out.append(
                    "        if (aspect == VK_IMAGE_ASPECT_DEPTH_BIT) { "
                    f"out = {{{_depth_bytes(f['depth_bits'])}u, 1u, 1u}}; return true; }}")
            if f["stencil_bits"]:
                out.append(
                    "        if (aspect == VK_IMAGE_ASPECT_STENCIL_BIT) { "
                    "out = {1u, 1u, 1u}; return true; }")
            out.append("        return false;")
        else:
            out.append(
                "        if (aspect == VK_IMAGE_ASPECT_COLOR_BIT) { "
                f"out = {{{f['block_bytes']}u, {f['block_width']}u, {f['block_height']}u}}; return true; }}")
            out.append("        return false;")
    out += [
        "    default:",
        "        return false;",
        "    }",
        "}",
        "",
        "}",
        "",
    ]
    write(out_dir / "vk_format_layout.h", header_guard("\n".join(out[1:])))
