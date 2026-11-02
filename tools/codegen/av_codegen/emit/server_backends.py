from __future__ import annotations

import re

from pathlib import Path

from ..ir import Command, Param, Registry
from .common import GENERATED_BANNER, header_guard, write


def _param_cpp(t) -> str:
    s = t.base
    eff_ptr = t.pointer_depth + (1 if t.fixed_dim is not None else 0)
    if eff_ptr:
        s += "*" * eff_ptr
    if t.is_const:
        s = "const " + s
    return s


def _signature(cmd: Command, body: str = "") -> str:
    parts = []
    for p in cmd.params:
        used = body and re.search(r"\b" + re.escape(p.name) + r"\b", body) is not None
        parts.append(f"{_param_cpp(p.type)} {p.name}" if used else f"{_param_cpp(p.type)}")
    return f"{cmd.return_type.cpp()} on_{cmd.name}({', '.join(parts)})"


def _is_count_param(cmd: Command, idx: int) -> bool:
    if idx + 1 >= len(cmd.params):
        return False
    a, b = cmd.params[idx], cmd.params[idx + 1]
    if a.type.base not in ("uint32_t", "size_t"):
        return False
    if a.type.pointer_depth != 1 or b.type.pointer_depth != 1:
        return False
    return b.len_expr == a.name


def _default_body(cmd: Command) -> list[str]:
    body: list[str] = []
    for i, p in enumerate(cmd.params):
        if _is_count_param(cmd, i):
            body.append(
                f"        if ({p.name} != nullptr) {{ *{p.name} = 0; }}"
            )
    if cmd.return_type.base != "void":
        body.append("        return VK_SUCCESS;")
    return body


def _emit_default(reg: Registry) -> str:
    lines: list[str] = [
        GENERATED_BANNER,
        "",
        "#include <stdint.h>",
        "",
        "#include <vulkan/vulkan_core.h>",
        "",
        '#include "vk_dispatch_server.h"',
        "",
        "namespace av::vkproto {",
        "",
        "// Concrete ServerDispatch with a no-op default for every command.",
        "// Subclasses override only the methods they care about; new commands",
        "// added to the codegen profile inherit a sane default automatically.",
        "class DefaultServerDispatch : public ServerDispatch {",
        "public:",
        "    // Every number is a handle and nothing is remembered, as the",
        "    // commands do nothing: there is no driver behind them to protect.",
        "    bool handle_for(HandleKind, uint64_t wire, uint64_t& handle) const override {",
        "        handle = wire;",
        "        return true;",
        "    }",
        "    uint64_t template_bytes(VkDescriptorUpdateTemplate) const override { return 0; }",
        "    void made(HandleKind, uint64_t) override {}",
        "    bool room_for(uint64_t) override { return true; }",
        "    void made_through(uint64_t) override {}",
        "    void ended(uint64_t) override {}",
        "    void children_ended(uint64_t) override {}",
        "",
    ]
    for cmd in sorted(reg.commands.values(), key=lambda c: c.cmd_id):
        body = _default_body(cmd)
        if not body:
            lines.append(f"    {_signature(cmd)} override {{}}")
        else:
            lines.append(f"    {_signature(cmd, chr(10).join(body))} override {{")
            lines.extend(body)
            lines.append("    }")
    lines.append("};")
    lines.append("")
    lines.append("}  // namespace av::vkproto")
    lines.append("")
    return "\n".join(lines[1:])


def _real_call_args(cmd: Command) -> list[str]:
    out: list[str] = []
    for p in cmd.params:
        if p.type.base == "VkAllocationCallbacks":
            out.append("nullptr")
        else:
            out.append(p.name)
    return out


def _emit_real(reg: Registry) -> str:
    lines: list[str] = [
        GENERATED_BANNER,
        "",
        "#include <stdint.h>",
        "",
        "#include <vulkan/vulkan_core.h>",
        "",
        '#include "vk_dispatch_server.h"',
        "",
        "namespace av::vkproto {",
        "",
        "// Concrete ServerDispatch that forwards every command into the host",
        "// Vulkan loader. Use this when the server should drive a real GPU.",
        "// VkAllocationCallbacks parameters are always replaced with nullptr",
        "// — the AV protocol does not forward the client's allocator.",
        "class RealVulkanDispatch : public ServerDispatch {",
        "public:",
    ]
    members: list[str] = []
    for cmd in sorted(reg.commands.values(), key=lambda c: c.cmd_id):
        args = ", ".join(_real_call_args(cmd))
        sig = _signature(cmd, args)

        if cmd.name == "vkCreateInstance":
            lines.extend([
                f"    {sig} override {{",
                f"        const VkResult rc = ::vkCreateInstance({args});",
                "        if (rc == VK_SUCCESS && pInstance != nullptr) "
                "{ instance_ = *pInstance; }",
                "        return rc;",
                "    }",
            ])
            continue

        if not cmd.extension_only:
            call = f"::{cmd.alias_of or cmd.name}({args})"
            if cmd.return_type.base == "void":
                lines.extend([f"    {sig} override {{", f"        {call};", "    }"])
            else:
                lines.extend([f"    {sig} override {{", f"        return {call};", "    }"])
            continue

        pfn = f"pfn_{cmd.name}_"
        members.append(f"    PFN_{cmd.name} {pfn} = nullptr;")
        body = [
            f"    {sig} override {{",
            f"        if ({pfn} == nullptr) {{",
            f"            {pfn} = reinterpret_cast<PFN_{cmd.name}>("
            f'::vkGetInstanceProcAddr(instance_, "{cmd.name}"));',
            "        }",
            f"        if ({pfn} == nullptr) {{",
        ]
        if cmd.return_type.base == "void":
            body.append("            return;")
            body.append("        }")
            body.append(f"        {pfn}({args});")
        elif cmd.return_type.base == "VkResult":
            body.append("            return VK_ERROR_EXTENSION_NOT_PRESENT;")
            body.append("        }")
            body.append(f"        return {pfn}({args});")
        else:
            body.append(f"            return {cmd.return_type.cpp()}{{}};")
            body.append("        }")
            body.append(f"        return {pfn}({args});")
        body.append("    }")
        lines.extend(body)

    lines.append("")
    lines.append("private:")
    lines.append("    VkInstance instance_ = VK_NULL_HANDLE;")
    lines.extend(members)
    lines.append("};")
    lines.append("")
    lines.append("}  // namespace av::vkproto")
    lines.append("")
    return "\n".join(lines[1:])


def emit(reg: Registry, out_dir: Path) -> None:
    write(out_dir / "vk_default_dispatch.h", header_guard(_emit_default(reg)))
    write(out_dir / "vk_real_dispatch.h", header_guard(_emit_real(reg)))
