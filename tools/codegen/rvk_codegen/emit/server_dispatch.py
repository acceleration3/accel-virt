"""Generate ``vk_dispatch_server.{h,cpp}``: the abstract ``ServerDispatch``
interface (one ``on_<vk>`` per command) and the ``dispatch_command``
function that decodes a wire frame and routes it.

User-implemented backends inherit ``ServerDispatch``; concrete generated
backends (``DefaultServerDispatch``, ``RealVulkanDispatch``) live in
``server_backends.py``.
"""

from __future__ import annotations

from pathlib import Path

from ..ir import Command, Registry
from . import classify
from .common import GENERATED_BANNER, header_guard, write


def _interface_signature(cmd: Command) -> str:
    args = ", ".join(f"{classify.param_cpp(p.type)} {p.name}" for p in cmd.params)
    return f"virtual {cmd.return_type.cpp()} on_{cmd.name}({args}) = 0;"


def _emit_case(reg: Registry, cmd: Command) -> list[str]:  # noqa: PLR0915
    is_void = cmd.return_type.base == "void"
    lines: list[str] = [f"case ::av::vkproto::kCmd_{cmd.name}: {{"]

    decl_lines: list[str] = ["    ::av::WireArena arena;", "    (void)arena;"]
    decode_lines: list[str] = []
    call_args: list[str] = []
    out_after_call: list[str] = []

    i = 0
    while i < len(cmd.params):
        p = cmd.params[i]
        if classify.is_count_array_pair(cmd.params, i):
            count = cmd.params[i]
            arr = cmd.params[i + 1]
            decl_lines.append("    std::uint8_t array_present = 0;")
            decl_lines.append(f"    std::uint32_t {count.name}_local = 0;")
            decl_lines.append(f"    std::vector<{arr.type.base}> {arr.name}_storage;")
            decode_lines.append("    if (!r.read_pod(array_present)) return false;")
            decode_lines.append("    if (array_present) {")
            decode_lines.append(f"        if (!r.read_pod({count.name}_local)) return false;")
            decode_lines.append(f"        {arr.name}_storage.resize({count.name}_local);")
            stype_value = classify.stype_value_for_struct(reg, arr.type.base)
            if stype_value is not None:
                decode_lines.append(
                    f"        for (auto& __e : {arr.name}_storage) {{ "
                    f"__e.sType = {stype_value}; __e.pNext = nullptr; }}"
                )
            decode_lines.append("    }")
            call_args.append(f"&{count.name}_local")
            call_args.append(f"array_present ? {arr.name}_storage.data() : nullptr")
            out_after_call.append(f"    reply.write_pod({count.name}_local);")
            out_after_call.append("    if (array_present) {")
            out_after_call.append(f"        for (std::uint32_t k = 0; k < {count.name}_local; ++k) {{")
            if arr.type.base in reg.structs or arr.type.base in reg.handles:
                out_after_call.append(
                    f"            serialize_{arr.type.base}(reply, {arr.name}_storage[k]);"
                )
            else:
                out_after_call.append(
                    f"            reply.write_pod({arr.name}_storage[k]);"
                )
            out_after_call.append("        }")
            out_after_call.append("    }")
            i += 2
            continue

        kind = classify.classify(reg, p, cmd.params)
        if kind == "in_null":
            call_args.append("nullptr")
        elif kind == "in_handle":
            decl_lines.append(f"    {p.type.base} {p.name} = VK_NULL_HANDLE;")
            decode_lines.append(f"    if (!deserialize_{p.type.base}(r, {p.name})) return false;")
            call_args.append(p.name)
        elif kind == "in_pod":
            decl_lines.append(f"    {p.type.cpp()} {p.name}{{}};")
            if classify.is_size_t(p.type):
                # Read the wire's 64 bits, then narrow to this machine's
                # size_t. See the same case on the client side.
                decode_lines.append("    {")
                decode_lines.append("        std::uint64_t __wide = 0;")
                decode_lines.append("        if (!r.read_pod(__wide)) return false;")
                decode_lines.append(
                    f"        {p.name} = static_cast<{p.type.cpp()}>(__wide);")
                decode_lines.append("    }")
            else:
                decode_lines.append(f"    if (!r.read_pod({p.name})) return false;")
            call_args.append(p.name)
        elif kind == "in_string":
            decl_lines.append(f"    const char* {p.name} = nullptr;")
            decode_lines.append("    {")
            decode_lines.append("        std::uint64_t n = 0;")
            decode_lines.append("        if (!r.read_varint(n)) return false;")
            decode_lines.append("        if (n > 0) {")
            decode_lines.append(
                "            char* tmp_str = arena.allocate_chars(static_cast<std::size_t>(n));"
            )
            decode_lines.append(
                "            if (!r.read(tmp_str, static_cast<std::size_t>(n))) return false;"
            )
            decode_lines.append(f"            {p.name} = tmp_str;")
            decode_lines.append("        }")
            decode_lines.append("    }")
            call_args.append(p.name)
        elif kind == "in_struct_ptr":
            decl_lines.append(f"    {p.type.base} {p.name}_local{{}};")
            decode_lines.append(
                f"    if (!deserialize_{p.type.base}(r, &{p.name}_local, arena)) return false;"
            )
            call_args.append(f"&{p.name}_local")
        elif kind in ("in_handle_array", "in_struct_array", "in_pod_array"):
            elem = p.type.base
            srv_len = classify.server_len_expr(cmd.params, p.len_expr)
            # Mirror the client's presence byte for a nullable array: read
            # it, decode only when present, and pass the driver null when
            # not — an empty vector's data() is not guaranteed null.
            nullable = bool(p.optional_levels) and p.optional_levels[0]
            decl_lines.append(f"    std::vector<{elem}> {p.name}_storage;")
            pad = ""
            if nullable:
                decl_lines.append(f"    std::uint8_t {p.name}_present = 0;")
                decode_lines.append(
                    f"    if (!r.read_pod({p.name}_present)) return false;"
                )
                decode_lines.append(f"    if ({p.name}_present) {{")
                pad = "    "
            decode_lines.append(
                f"{pad}    {p.name}_storage.resize(static_cast<std::size_t>({srv_len}));"
            )
            decode_lines.append(
                f"{pad}    for (std::uint32_t __i = 0; "
                f"__i < static_cast<std::uint32_t>({srv_len}); ++__i) {{"
            )
            if kind == "in_handle_array":
                decode_lines.append(
                    f"{pad}        if (!deserialize_{elem}(r, {p.name}_storage[__i])) return false;"
                )
            elif kind == "in_struct_array":
                decode_lines.append(
                    f"{pad}        if (!deserialize_{elem}(r, &{p.name}_storage[__i], arena)) return false;"
                )
            else:  # in_pod_array
                decode_lines.append(
                    f"{pad}        if (!r.read_pod({p.name}_storage[__i])) return false;"
                )
            decode_lines.append(f"{pad}    }}")
            if nullable:
                decode_lines.append("    }")
                call_args.append(
                    f"{p.name}_present ? {p.name}_storage.data() : nullptr"
                )
            else:
                call_args.append(f"{p.name}_storage.data()")
        elif kind in ("in_struct_array2", "in_pod_array2"):
            # Rebuild the pointer-to-pointer shape in the arena: an outer
            # array of pointers, each to its own inner array.
            sib, _, member = p.inner_len.partition(".")
            srv_outer = classify.server_len_expr(cmd.params, p.len_expr)
            elem = p.type.base
            decl_lines.append(f"    const {elem}** {p.name}_outer = nullptr;")
            decode_lines.append(f"    {{")
            decode_lines.append(
                f"        const std::uint32_t __n_outer = static_cast<std::uint32_t>({srv_outer});")
            decode_lines.append(
                f"        auto** __ptrs = arena.allocate<const {elem}*>(__n_outer);")
            decode_lines.append("        for (std::uint32_t __i = 0; __i < __n_outer; ++__i) {")
            decode_lines.append(
                f"            const std::uint32_t __n = static_cast<std::uint32_t>({sib}_storage[__i].{member});")
            decode_lines.append(f"            auto* __inner = arena.allocate<{elem}>(__n);")
            decode_lines.append("            for (std::uint32_t __j = 0; __j < __n; ++__j) {")
            if kind == "in_struct_array2":
                decode_lines.append(
                    f"                if (!deserialize_{elem}(r, &__inner[__j], arena)) return false;")
            else:
                decode_lines.append("                if (!r.read_pod(__inner[__j])) return false;")
            decode_lines.append("            }")
            decode_lines.append("            __ptrs[__i] = __inner;")
            decode_lines.append("        }")
            decode_lines.append(f"        {p.name}_outer = __ptrs;")
            decode_lines.append("    }")
            call_args.append(f"{p.name}_outer")
        elif kind == "in_byte_array":
            srv_len = classify.server_len_expr(cmd.params, p.len_expr)
            decl_lines.append(f"    std::vector<std::uint8_t> {p.name}_storage;")
            decode_lines.append(f"    if ({srv_len} > 0) {{")
            decode_lines.append(
                f"        {p.name}_storage.resize(static_cast<std::size_t>({srv_len}));"
            )
            decode_lines.append(
                f"        if (!r.read({p.name}_storage.data(), "
                f"static_cast<std::size_t>({srv_len}))) return false;"
            )
            decode_lines.append("    }")
            call_args.append(f"{p.name}_storage.data()")
        elif kind == "in_fixed_array":
            decl_lines.append(
                f"    {p.type.base} {p.name}_local[{p.type.fixed_dim}]{{}};"
            )
            decode_lines.append(
                f"    for (std::uint32_t __i = 0; __i < {p.type.fixed_dim}u; ++__i) {{"
            )
            decode_lines.append(
                f"        if (!r.read_pod({p.name}_local[__i])) return false;"
            )
            decode_lines.append("    }")
            call_args.append(f"{p.name}_local")
        elif kind == "out_handle":
            decl_lines.append(f"    {p.type.base} {p.name}_out = VK_NULL_HANDLE;")
            call_args.append(f"&{p.name}_out")
            out_after_call.append(f"    serialize_{p.type.base}(reply, {p.name}_out);")
        elif kind == "out_struct":
            decl_lines.append(f"    {p.type.base} {p.name}_out{{}};")
            if reg.structs[p.type.base].has_stype:
                decode_lines.append(f"    if (!r.read_pod({p.name}_out.sType)) return false;")
                # Rebuild the client's chain out of the arena before the
                # driver call: those nodes are what the driver fills in,
                # and serializing the struct afterwards walks the same
                # chain back to the client.
                decode_lines.append(
                    f"    if (!deserialize_pnext_chain(r, "
                    f"reinterpret_cast<VkBaseOutStructure*>(&{p.name}_out), arena)) "
                    f"return false;"
                )
            call_args.append(f"&{p.name}_out")
            out_after_call.append(f"    serialize_{p.type.base}(reply, {p.name}_out);")
        elif kind == "out_pod":
            decl_lines.append(f"    {p.type.base} {p.name}_out{{}};")
            call_args.append(f"&{p.name}_out")
            out_after_call.append(f"    reply.write_pod({p.name}_out);")
        elif kind == "out_byte_array":
            srv_len = classify.server_len_expr(cmd.params, p.len_expr)
            decl_lines.append(f"    std::vector<std::uint8_t> {p.name}_storage;")
            decode_lines.append(
                f"    {p.name}_storage.resize(static_cast<std::size_t>({srv_len}));"
            )
            call_args.append(f"{p.name}_storage.data()")
            out_after_call.append(f"    if (!{p.name}_storage.empty()) {{")
            out_after_call.append(
                f"        reply.write({p.name}_storage.data(), {p.name}_storage.size());"
            )
            out_after_call.append("    }")
        elif kind in ("out_handle_array", "out_struct_array", "out_pod_array"):
            elem = p.type.base
            srv_len = classify.server_len_expr(cmd.params, p.len_expr)
            decl_lines.append(f"    std::vector<{elem}> {p.name}_storage;")
            decode_lines.append(
                f"    {p.name}_storage.resize(static_cast<std::size_t>({srv_len}));"
            )
            call_args.append(f"{p.name}_storage.data()")
            out_after_call.append(
                f"    for (std::uint32_t __k = 0; __k < {p.name}_storage.size(); ++__k) {{"
            )
            if kind in ("out_handle_array", "out_struct_array"):
                out_after_call.append(
                    f"        serialize_{elem}(reply, {p.name}_storage[__k]);"
                )
            else:
                out_after_call.append(
                    f"        reply.write_pod({p.name}_storage[__k]);"
                )
            out_after_call.append("    }")
        i += 1

    lines.extend(decl_lines)
    lines.extend(decode_lines)

    fmt, fmt_args = classify.trace_format_and_args(cmd, reg)
    fmt_args_str = ("," + ", ".join(fmt_args)) if fmt_args else ""
    lines.append(
        f'    RVK_TRACE("server {cmd.name}{fmt}"{fmt_args_str});'
    )

    if is_void:
        lines.append(f"    s.on_{cmd.name}({', '.join(call_args)});")
        if cmd.sync_mode == "sync":
            lines.append("    reply.write_pod<std::int32_t>(0);")
            lines.extend(out_after_call)
        lines.append(f'    RVK_TRACE("server {cmd.name} -> (void)");')
    else:
        lines.append(
            f"    {cmd.return_type.cpp()} result = s.on_{cmd.name}({', '.join(call_args)});"
        )
        # Encode the return in its own width. Most commands return
        # VkResult and fit in 32 bits, but the device-address and
        # opaque-capture-address queries return 64. Forcing those through
        # an int32 silently truncates a GPU pointer, and the shader that
        # dereferences the result faults the device — reported as
        # VK_ERROR_DEVICE_LOST, nowhere near the actual cause.
        if classify.returns_wide(cmd):
            lines.append(
                "    reply.write_pod<std::uint64_t>(static_cast<std::uint64_t>(result));"
            )
        else:
            lines.append(
                "    reply.write_pod<std::int32_t>(static_cast<std::int32_t>(result));"
            )
        lines.extend(out_after_call)
        lines.append(
            f'    RVK_TRACE("server {cmd.name} -> {{}}", static_cast<long long>(result));'
        )

    lines.append("    return true;")
    lines.append("}")
    return lines


def emit(reg: Registry, out_dir: Path) -> None:
    cmds = sorted(reg.commands.values(), key=lambda c: c.cmd_id)

    # ---- Header ----
    header = [
        GENERATED_BANNER,
        "",
        "#include <cstdint>",
        "",
        "#include <vulkan/vulkan_core.h>",
        "",
        # dispatch_command's signature names both, so both must be
        # complete here — a forward declaration of BufferReader is not
        # enough for the declaration to compile.
        '#include "proto/wire/wire_writer.hpp"',
        '#include "proto/wire/wire_reader.hpp"',
        "",
        "namespace av::vkproto {",
        "",
        "// User implements this; the dispatch table calls into it for every command.",
        "class ServerDispatch {",
        "public:",
        "    virtual ~ServerDispatch() = default;",
        "",
    ]
    for cmd in cmds:
        header.append("    " + _interface_signature(cmd))
    header.append("};")
    header.append("")
    header.append(
        "// Decode and dispatch a single command. Returns false on protocol error."
    )
    header.append(
        "bool dispatch_command(ServerDispatch& s, std::uint32_t cmd_id, "
        "::av::WireReader& r, ::av::WireWriter& reply);"
    )
    header.append("")
    header.append("}  // namespace av::vkproto")
    header.append("")
    write(out_dir / "vk_dispatch_server.h", header_guard("\n".join(header[1:])))

    # ---- Source ----
    source = [
        GENERATED_BANNER,
        "",
        '#include "vk_dispatch_server.h"',
        "",
        "#include <vector>",
        "",
        # rvk/instr/log.hpp, not rvk/log.hpp — see client_dispatch.py.
        '#include "rvk/instr/log.hpp"',
        '#include "vk_protocol_ids.h"',
        '#include "vk_serialize.h"',
        "",
        "namespace av::vkproto {",
        "",
        "bool dispatch_command(ServerDispatch& s, std::uint32_t cmd_id, "
        "::av::WireReader& r, ::av::WireWriter& reply) {",
        "    switch (cmd_id) {",
    ]
    for cmd in cmds:
        for line in _emit_case(reg, cmd):
            source.append("    " + line)
    source.append("    default:")
    source.append("        return false;")
    source.append("    }")
    source.append("}")
    source.append("")
    source.append("}  // namespace av::vkproto")
    source.append("")
    write(out_dir / "vk_dispatch_server.cpp", "\n".join(source))
