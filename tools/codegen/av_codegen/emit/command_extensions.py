from __future__ import annotations

from pathlib import Path

from ..ir import Registry
from .common import GENERATED_BANNER, header_guard, write

DEVICE_LEVEL_HANDLES = frozenset({"VkDevice", "VkQueue", "VkCommandBuffer"})
INSTANCE_LEVEL_HANDLES = frozenset({"VkInstance", "VkPhysicalDevice"})


def _requirement(reg: Registry, name: str) -> tuple[str, list[str]]:
    cmd = reg.commands[name]
    if not cmd.extension_only or not cmd.params:
        return "open", []
    listed = reg.command_extensions.get(name, [])
    first = cmd.params[0].type.base
    if first in DEVICE_LEVEL_HANDLES:
        wanted, owner = "device", "device"
    elif first in INSTANCE_LEVEL_HANDLES:
        wanted, owner = "instance", "instance"
    else:
        return "open", []
    if not listed or any(kind != wanted for _ext, kind in listed):
        return "open", []
    return owner, [ext for ext, _kind in listed]


def _slots(reg: Registry, kind: str) -> list[str]:
    names = {ext for exts in reg.command_extensions.values() for ext, k in exts if k == kind}
    return sorted(names)


def emit(reg: Registry, out_dir: Path) -> None:
    device = _slots(reg, "device")
    instance = _slots(reg, "instance")
    most_slots = 64
    for kind, names in (("device", device), ("instance", instance)):
        if len(names) > most_slots:
            raise SystemExit(f"{len(names)} {kind} extensions need a slot each, and the enabled mask is {most_slots} bits")
    slot_of = {("device", n): i for i, n in enumerate(device)}
    slot_of.update({("instance", n): i for i, n in enumerate(instance)})

    rows: list[tuple[int, str, int, int]] = []
    flat: list[int] = []
    for cmd in sorted(reg.commands.values(), key=lambda c: c.cmd_id):
        owner, exts = _requirement(reg, cmd.name)
        if owner == "open":
            continue
        rows.append((cmd.cmd_id, owner, len(flat), len(exts)))
        flat.extend(slot_of[(owner, e)] for e in exts)

    header = [
        GENERATED_BANNER,
        "",
        "#include <stdint.h>",
        "",
        "namespace av::vkproto {",
        "",
        "// Whose enabled extensions a command needs one of: the instance's, the",
        "// device's, or nobody's (core commands, and extension commands a physical",
        "// device answers without the extension being enabled).",
        "enum class CommandOwner : uint8_t { open, instance, device };",
        "",
        f"inline constexpr uint32_t kDeviceExtensionSlots = {len(device)}u;",
        f"inline constexpr uint32_t kInstanceExtensionSlots = {len(instance)}u;",
        "inline constexpr uint16_t kNoExtensionSlot = 0xFFFFu;",
        "static_assert(kDeviceExtensionSlots <= 64u && kInstanceExtensionSlots <= 64u, \"the enabled mask is 64 bits\");",
        "",
        "struct CommandExtensions {",
        "    CommandOwner owner;",
        "    uint16_t count;",
        "    const uint16_t* slots;",
        "};",
        "",
        "// The extensions of which a command needs at least one enabled.",
        "[[nodiscard]] CommandExtensions command_extensions(uint32_t cmd_id) noexcept;",
        "// The slot an extension name holds among the device or instance",
        "// extensions some command needs; kNoExtensionSlot for any other name.",
        "[[nodiscard]] uint16_t device_extension_slot(const char* name) noexcept;",
        "[[nodiscard]] uint16_t instance_extension_slot(const char* name) noexcept;",
        "",
        "}  // namespace av::vkproto",
        "",
    ]
    write(out_dir / "vk_command_extensions.h", header_guard("\n".join(header[1:])))

    src = [
        GENERATED_BANNER,
        "",
        '#include "vk_command_extensions.h"',
        "",
        "namespace av::vkproto {",
        "",
        "namespace {",
        "",
        "struct Row {",
        "    uint32_t id;",
        "    CommandOwner owner;",
        "    uint16_t first;",
        "    uint16_t count;",
        "};",
        "",
        "constexpr Row kRows[] = {",
    ]
    for cid, owner, first, count in rows:
        src.append(f"    {{0x{cid:08X}u, CommandOwner::{owner}, {first}u, {count}u}},")
    src += ["};", "", "constexpr uint16_t kSlots[] = {"]
    for i in range(0, len(flat), 16):
        src.append("    " + ", ".join(f"{s}u" for s in flat[i:i + 16]) + ",")
    if not flat:
        src.append("    0u,")
    src += ["};", ""]
    for scope, names in (("device", device), ("instance", instance)):
        src.append(f"constexpr const char* k{scope.capitalize()}Names[] = {{")
        src.extend(f'    "{n}",' for n in names)
        if not names:
            src.append('    "",')
        src += ["};", ""]
    src += [
        "uint16_t slot_of(const char* const* names, uint32_t count, const char* name) noexcept {",
        "    for (uint32_t i = 0; i < count; ++i) {",
        "        if (__builtin_strcmp(names[i], name) == 0) { return static_cast<uint16_t>(i); }",
        "    }",
        "    return kNoExtensionSlot;",
        "}",
        "",
        "}  // namespace",
        "",
        "CommandExtensions command_extensions(uint32_t cmd_id) noexcept {",
        "    uint32_t low = 0;",
        "    uint32_t high = static_cast<uint32_t>(sizeof(kRows) / sizeof(kRows[0]));",
        "    while (low < high) {",
        "        const uint32_t mid = low + (high - low) / 2u;",
        "        if (kRows[mid].id == cmd_id) {",
        "            return CommandExtensions{kRows[mid].owner, kRows[mid].count, kSlots + kRows[mid].first};",
        "        }",
        "        if (kRows[mid].id < cmd_id) { low = mid + 1u; } else { high = mid; }",
        "    }",
        "    return CommandExtensions{CommandOwner::open, 0u, nullptr};",
        "}",
        "",
        "uint16_t device_extension_slot(const char* name) noexcept {",
        "    return slot_of(kDeviceNames, kDeviceExtensionSlots, name);",
        "}",
        "",
        "uint16_t instance_extension_slot(const char* name) noexcept {",
        "    return slot_of(kInstanceNames, kInstanceExtensionSlots, name);",
        "}",
        "",
        "}  // namespace av::vkproto",
        "",
    ]
    write(out_dir / "vk_command_extensions.cpp", "\n".join(src))
