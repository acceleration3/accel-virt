from __future__ import annotations

from pathlib import Path

from ..ir import Command, Param, Registry, StructMember
from . import classify
from .common import GENERATED_BANNER, header_guard, write

DISPATCHABLE_HANDLES = classify.DISPATCHABLE_HANDLES

_TRANSLATION = "::av::icd::wire_handles()"


def _is_recordable(cmd: Command, parent: Param) -> bool:
    if parent.type.base != "VkCommandBuffer":
        return False
    return cmd.name.startswith("vkCmd") or cmd.name in (
        "vkBeginCommandBuffer",
        "vkEndCommandBuffer",
    )


_DISPATCH_LEVELS = {
    "VkInstance": "instance",
    "VkPhysicalDevice": "physical_device",
    "VkDevice": "device",
    "VkQueue": "device",
    "VkCommandBuffer": "device",
}


def dispatch_level(cmd: Command) -> str:
    if not cmd.params:
        return "global"
    first = cmd.params[0].type
    if first.pointer_depth != 0 or first.fixed_dim is not None:
        return "global"
    return _DISPATCH_LEVELS.get(first.base, "global")


def _is_count_array_pair(params: list[Param], i: int) -> bool:
    if i + 1 >= len(params):
        return False
    a, b = params[i], params[i + 1]
    if a.type.base not in ("uint32_t", "size_t"):
        return False
    if a.type.pointer_depth != 1 or b.type.pointer_depth != 1:
        return False
    return b.len_expr == a.name


def _param_cpp(t) -> str:
    s = t.base
    eff_ptr = t.pointer_depth + (1 if t.fixed_dim is not None else 0)
    if eff_ptr:
        s += "*" * eff_ptr
    if t.is_const:
        s = "const " + s
    return s


def _signature(cmd: Command) -> str:
    args = ", ".join(f"{_param_cpp(p.type)} {p.name}" for p in cmd.params)
    return f"VKAPI_ATTR {cmd.return_type.cpp()} VKAPI_CALL Wrap_{cmd.name}({args})"


def _first_input_dispatchable(cmd: Command) -> Param | None:
    for p in cmd.params:
        if p.type.base in DISPATCHABLE_HANDLES and p.type.pointer_depth == 0:
            return p
    return None


def _emit_create_instance(cmd: Command) -> list[str]:
    out_inst = next(
        (p for p in cmd.params
         if p.type.base == "VkInstance" and p.type.pointer_depth == 1),
        None,
    )
    assert out_inst is not None, "vkCreateInstance must have VkInstance* out param"

    forwarded: list[str] = []
    for p in cmd.params:
        if p.name == out_inst.name:
            forwarded.append("&server_instance")
        else:
            forwarded.append(p.name)

    return [
        "    auto* conn = ::av::icd::acquire_default_connection();",
        "    if (conn == nullptr) {",
        "        return VK_ERROR_INITIALIZATION_FAILED;",
        "    }",
        "    // 64 bits wide because that is the wire's width. Holding the",
        "    // server's handle in a VkInstance would narrow it on a 32-bit",
        "    // client and send back a handle no object is at.",
        "    uint64_t server_instance = 0;",
        f"    const VkResult av_rc = ::av::vkproto::av_{cmd.name}("
        + f"conn->transport(), {_TRANSLATION}, "
        + ", ".join(forwarded)
        + ");",
        "    if (av_rc != VK_SUCCESS) {",
        "        ::av::icd::release_connection(conn);",
        "        return av_rc;",
        "    }",
        f"    *{out_inst.name} = ::av::icd::wrap_server_handle<{out_inst.type.base}>("
        f"server_instance, conn);",
        "    return av_rc;",
    ]


def _emit_destroy_instance(cmd: Command) -> list[str]:
    inst_param = next(
        (p for p in cmd.params
         if p.type.base == "VkInstance" and p.type.pointer_depth == 0),
        None,
    )
    assert inst_param is not None, "vkDestroyInstance must take VkInstance"

    forwarded: list[str] = []
    for p in cmd.params:
        forwarded.append(p.name)

    return [
        f"    if ({inst_param.name} == VK_NULL_HANDLE) {{",
        "        return;",
        "    }",
        f"    auto* conn = ::av::icd::connection_of({inst_param.name});",
        f"    ::av::vkproto::av_{cmd.name}(conn->transport(), {_TRANSLATION}, "
        + ", ".join(forwarded) + ");",
        f"    ::av::icd::destroy_wrapper({inst_param.name});",
        "    ::av::icd::release_connection(conn);",
    ]


def _staging_array(cmd: Command, name: str, count_expr: str) -> list[str]:
    if cmd.return_type.base == "void":
        raise ValueError(
            f"command {cmd.name} returns void but has a dispatchable handle "
            "array output; it has no way to refuse an oversized request"
        )
    return [
        f"    if (static_cast<size_t>({count_expr}) > kMaxStagedHandles) {{",
        "        return VK_ERROR_OUT_OF_HOST_MEMORY;",
        "    }",
        f"    uint64_t {name}[kMaxStagedHandles] = {{}};",
    ]


def _handle_array_member(reg: Registry, struct_name: str) -> StructMember | None:
    struct = reg.structs.get(struct_name)
    if struct is None:
        return None
    return next(
        (m for m in struct.members
         if m.type.base in DISPATCHABLE_HANDLES and m.type.fixed_dim is not None
         and m.type.pointer_depth == 0 and m.len_expr is not None),
        None,
    )


def _wrap_member_handles(array: str, count_expr: str, member: StructMember) -> list[str]:
    return [
        f"    for (uint32_t k = 0; k < {count_expr}; ++k) {{",
        f"        for (uint32_t j = 0; j < {array}[k].{member.len_expr}; ++j) {{",
        f"            {array}[k].{member.name}[j] = ::av::icd::wrap_server_handle<{member.type.base}>("
        f"reinterpret_cast<uint64_t>({array}[k].{member.name}[j]), conn);",
        f"            if ({array}[k].{member.name}[j] == VK_NULL_HANDLE) {{",
        "                return VK_ERROR_OUT_OF_HOST_MEMORY;",
        "            }",
        "        }",
        "    }",
    ]


def _emit_default_body(cmd: Command, reg: Registry) -> list[str]:
    is_void = cmd.return_type.base == "void"
    pre_call: list[str] = []
    post_call: list[str] = []
    call_args: list[str] = []
    has_handle_array_out = False

    parent = _first_input_dispatchable(cmd)
    if parent is None:
        raise ValueError(
            f"command {cmd.name} has no input dispatchable handle; "
            "either special-case it (like vkCreateInstance) or audit the profile"
        )

    if not _is_recordable(cmd, parent):
        pre_call.append(
            f"    auto* conn = ::av::icd::connection_of({parent.name});"
        )

    if _is_recordable(cmd, parent):
        transport = f"::av::icd::record_target({parent.name}), {_TRANSLATION}"
    else:
        transport = f"conn->transport(), {_TRANSLATION}"

    i = 0
    while i < len(cmd.params):
        p = cmd.params[i]

        if _is_count_array_pair(cmd.params, i):
            count = cmd.params[i]
            arr = cmd.params[i + 1]
            if arr.type.base in DISPATCHABLE_HANDLES:
                vec_name = f"server_{arr.name}"
                pre_call.append(f"    if ({arr.name} == nullptr) {{")
                forward_args: list[str] = []
                for q in cmd.params:
                    if q.name == arr.name:
                        forward_args.append("nullptr")
                    else:
                        forward_args.append(q.name)
                pre_call.append(
                    f"        return ::av::vkproto::av_{cmd.name}("
                    + transport + ", " + ", ".join(forward_args) + ");"
                )
                pre_call.append("    }")
                pre_call.extend(_staging_array(cmd, vec_name, f"*{count.name}"))
                call_args.append(count.name)
                call_args.append(vec_name)
                post_call.append(
                    f"    for (uint32_t k = 0; k < *{count.name}; ++k) {{"
                )
                post_call.append(
                    f"        {arr.name}[k] = "
                    f"::av::icd::wrap_server_handle<{arr.type.base}>({vec_name}[k], conn);"
                )
                post_call.append("    }")
                has_handle_array_out = True
            elif _handle_array_member(reg, arr.type.base) is not None:
                member = _handle_array_member(reg, arr.type.base)
                pre_call.append(f"    if ({arr.name} == nullptr) {{")
                pre_call.append(
                    f"        return ::av::vkproto::av_{cmd.name}("
                    + transport + ", "
                    + ", ".join("nullptr" if q.name == arr.name else q.name for q in cmd.params)
                    + ");"
                )
                pre_call.append("    }")
                call_args.append(count.name)
                call_args.append(arr.name)
                post_call.extend(_wrap_member_handles(arr.name, f"*{count.name}", member))
                has_handle_array_out = True
            else:
                call_args.append(count.name)
                call_args.append(arr.name)
            i += 2
            continue

        if p.type.base in DISPATCHABLE_HANDLES and p.type.pointer_depth == 0:
            call_args.append(p.name)
        elif (
            p.type.base in DISPATCHABLE_HANDLES
            and p.type.pointer_depth == 1
            and p.direction == "out"
            and p.len_expr is None
        ):
            local = f"server_{p.name}"
            pre_call.append(f"    uint64_t {local} = 0;")
            call_args.append(f"&{local}")
            post_call.append(
                f"    *{p.name} = ::av::icd::wrap_server_handle<{p.type.base}>({local}, conn);"
            )
        elif (
            p.type.base in DISPATCHABLE_HANDLES
            and p.type.pointer_depth == 1
            and p.direction == "out"
            and p.len_expr is not None
        ):
            vec = f"server_{p.name}"
            count_expr = p.len_expr
            pre_call.extend(_staging_array(cmd, vec, count_expr))
            call_args.append(vec)
            post_call.append(
                f"    for (uint32_t k = 0; k < static_cast<uint32_t>({count_expr}); ++k) {{"
            )
            post_call.append(
                f"        {p.name}[k] = ::av::icd::wrap_server_handle<{p.type.base}>({vec}[k], conn);"
            )
            post_call.append("    }")
            has_handle_array_out = True
        else:
            call_args.append(p.name)
        i += 1

    body: list[str] = []
    body.extend(pre_call)
    call_str = (
        f"::av::vkproto::av_{cmd.name}({transport}, "
        + ", ".join(call_args) + ")"
    )
    if is_void:
        body.append(f"    {call_str};")
        body.extend(post_call)
    elif post_call or has_handle_array_out:
        body.append(f"    const {cmd.return_type.cpp()} av_rc = {call_str};")
        body.append("    if (av_rc != VK_SUCCESS && av_rc != VK_INCOMPLETE) {")
        body.append("        return av_rc;")
        body.append("    }")
        body.extend(post_call)
        body.append("    return av_rc;")
    else:
        body.append(f"    return {call_str};")
    return body


def _emit_wrapper_body(cmd: Command, reg: Registry) -> list[str]:
    if cmd.name == "vkCreateInstance":
        return _emit_create_instance(cmd)
    if cmd.name == "vkDestroyInstance":
        return _emit_destroy_instance(cmd)
    return _emit_default_body(cmd, reg)


def emit(reg: Registry, out_dir: Path) -> None:
    cmds = sorted(reg.commands.values(), key=lambda c: c.cmd_id)

    h = [
        GENERATED_BANNER,
        "",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        "#include <vulkan/vulkan_core.h>",
        "",
        "namespace av::vkicd::generated {",
        "",
        "// Wrapper functions matching the application-facing Vulkan signatures.",
        "// Each one looks up the per-VkInstance Connection (or opens a fresh",
        "// one in vkCreateInstance), unwraps dispatchable handles, calls the",
        "// generated av_vk* dispatch over that Connection's transport, and",
        "// wraps any returned dispatchable handles back into the wrappers",
        "// the handle registry hands out, carrying the same Connection.",
        "",
    ]
    for cmd in cmds:
        h.append(_signature(cmd) + ";")
    h.append("")
    h.append("// The object the loader dispatches a command on, from its first")
    h.append("// parameter in vk.xml: a global command takes no dispatchable handle,")
    h.append("// and a queue or command buffer dispatches as its device.")
    h.append("enum class DispatchLevel : uint8_t { global, instance, physical_device, device };")
    h.append("")
    h.append("// Name -> function-pointer entries for vk_icdGetInstanceProcAddr.")
    h.append("struct Entry {")
    h.append("    const char* name;")
    h.append("    PFN_vkVoidFunction fn;")
    h.append("    DispatchLevel level;")
    h.append("};")
    h.append("")
    h.append("// The generated entries and how many there are. Not a std::span:")
    h.append("// this repository compiles freestanding, with no standard library,")
    h.append("// so the table carries its own length.")
    h.append("class EntryTable {")
    h.append("public:")
    h.append("    EntryTable(const Entry* first, size_t count) noexcept;")
    h.append("")
    h.append("    const Entry* begin() const noexcept;")
    h.append("    const Entry* end() const noexcept;")
    h.append("    size_t size() const noexcept;")
    h.append("")
    h.append("private:")
    h.append("    const Entry* first_;")
    h.append("    size_t count_;")
    h.append("};")
    h.append("")
    h.append("// The generated wrapper entries. The ICD layer prepends its own")
    h.append("// non-wire entries (loader negotiation, GetInstanceProcAddr, etc.).")
    h.append("EntryTable wrapper_entries() noexcept;")
    h.append("")
    h.append("}  // namespace av::vkicd::generated")
    h.append("")
    write(out_dir / "vk_icd_entries.h", header_guard("\n".join(h[1:])))

    s = [
        GENERATED_BANNER,
        "",
        '#include "vk_icd_entries.h"',
        "",
        '#include "core/trace/function_scope.hpp"',
        '#include "proto/client/icd_connection.hpp"',
        '#include "proto/client/icd_handle_registry.hpp"',
        '#include "vk_dispatch_client.h"',
        "",
        "namespace av::vkicd::generated {",
        "namespace {",
        "",
        "// How many raw server handles a wrapper stages on its stack while",
        "// wrapping an array the application asked for. A request for more is",
        "// refused with VK_ERROR_OUT_OF_HOST_MEMORY rather than allocated.",
        "constexpr size_t kMaxStagedHandles = 1024;",
        "",
        "}  // namespace",
        "",
    ]
    for cmd in cmds:
        s.append(_signature(cmd) + " {")
        s.append(f'    AV_SCOPE("{cmd.name}");')
        s.extend(_emit_wrapper_body(cmd, reg))
        s.append("}")
        s.append("")

    s.append("namespace {")
    s.append(f"const Entry kWrapperTable[{len(cmds)}] = {{")
    for cmd in cmds:
        s.append(
            f'    Entry{{ "{cmd.name}", '
            f"reinterpret_cast<PFN_vkVoidFunction>(&Wrap_{cmd.name}), "
            f"DispatchLevel::{dispatch_level(cmd)} }},"
        )
    s.append("};")
    s.append("}  // namespace")
    s.append("")
    s.append(
        "EntryTable::EntryTable(const Entry* first, size_t count) noexcept"
    )
    s.append("    : first_(first), count_(count) {}")
    s.append("")
    s.append("const Entry* EntryTable::begin() const noexcept { return first_; }")
    s.append(
        "const Entry* EntryTable::end() const noexcept { return first_ + count_; }"
    )
    s.append("size_t EntryTable::size() const noexcept { return count_; }")
    s.append("")
    s.append(
        f"EntryTable wrapper_entries() noexcept {{ "
        f"return EntryTable{{kWrapperTable, {len(cmds)}u}}; }}"
    )
    s.append("")
    s.append("}  // namespace av::vkicd::generated")
    s.append("")
    write(out_dir / "vk_icd_entries.cpp", "\n".join(s[1:]))
