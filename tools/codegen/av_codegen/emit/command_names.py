from __future__ import annotations

from pathlib import Path

from ..ir import Registry
from .common import GENERATED_BANNER, header_guard, write


AV_INTERNAL_CMDS: tuple[tuple[int, str], ...] = (
    (0x00000001, "kCmdHello"),
    (0x00000002, "kCmdHelloAck"),
    (0x00000003, "kCmdPing"),
    (0x00000004, "kCmdPong"),
    (0x00000005, "kCmdAsyncError"),
    (0x00000010, "kCmdCreateXcbSurface"),
    (0x00000011, "kCmdDestroySurface"),
    (0x00000020, "kCmdMemoryWrite"),
    (0x00000021, "kCmdMemoryRead"),
    (0xFFFFFFFF, "kCmdBye"),
)


def emit(reg: Registry, out_dir: Path) -> None:
    cmds = sorted(reg.commands.values(), key=lambda c: c.cmd_id)

    header = [
        GENERATED_BANNER,
        "",
        "#include <stdint.h>",
        "",
        "namespace av::vkproto {",
        "",
        "// Returns a static C-string name for a wire command id. Returns",
        "// ``\"unknown\"`` for unknown ids — never null. Cheap (one switch).",
        "[[nodiscard]] const char* cmd_id_to_string(uint32_t cmd_id) noexcept;",
        "// Whether the profile carries `cmd_id`: what a session decodes at all.",
        "[[nodiscard]] bool knows_command(uint32_t cmd_id) noexcept;",
        "",
        "}  // namespace av::vkproto",
        "",
    ]
    write(out_dir / "vk_command_names.h", header_guard("\n".join(header[1:])))

    source = [
        GENERATED_BANNER,
        "",
        '#include "vk_command_names.h"',
        "",
        "namespace av::vkproto {",
        "",
        "const char* cmd_id_to_string(uint32_t cmd_id) noexcept {",
        "    switch (cmd_id) {",
    ]
    for cid, name in AV_INTERNAL_CMDS:
        source.append(f'        case 0x{cid:08X}u: return "{name}";')
    for cmd in cmds:
        source.append(
            f'        case 0x{cmd.cmd_id:08X}u: return "{cmd.name}";'
        )
    source.append('        default: return "unknown";')
    source.append("    }")
    source.append("}")
    source.append("")
    source.append("bool knows_command(uint32_t cmd_id) noexcept {")
    source.append("    switch (cmd_id) {")
    for cmd in cmds:
        source.append(f"        case 0x{cmd.cmd_id:08X}u:")
    source.append("            return true;")
    source.append("        default: return false;")
    source.append("    }")
    source.append("}")
    source.append("")
    source.append("}  // namespace av::vkproto")
    source.append("")
    write(out_dir / "vk_command_names.cpp", "\n".join(source))
