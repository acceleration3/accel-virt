from __future__ import annotations

from pathlib import Path

from ..ir import Registry
from .common import GENERATED_BANNER, header_guard, write


def emit(reg: Registry, out_dir: Path) -> None:
    lines = [
        GENERATED_BANNER,
        "",
        "#include <stdint.h>",
        "",
        "namespace av::vkproto {",
        "",
        "// Stable wire-protocol command IDs. See codegen/protocol_ids.toml.",
        "enum CmdId : uint32_t {",
    ]
    for cmd in sorted(reg.commands.values(), key=lambda c: c.cmd_id):
        lines.append(f"    kCmd_{cmd.name} = 0x{cmd.cmd_id:08X}u,")
    lines.append("};")
    lines.append("")
    lines.append("}  // namespace av::vkproto")
    lines.append("")

    write(out_dir / "vk_protocol_ids.h", header_guard("\n".join(lines[1:])))
