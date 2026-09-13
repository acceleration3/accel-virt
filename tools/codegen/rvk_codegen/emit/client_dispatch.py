"""Generate ``vk_dispatch_client.{h,cpp}``: ``rvk_vkXxx`` wrappers that
serialize their args, ship them over a ``Transport``, and decode the
reply.

Each wrapper takes a ``Transport&`` so the caller chooses the wire path
(real ICD connection, in-process loopback, recording transport, etc.).
"""

from __future__ import annotations

from pathlib import Path

from ..ir import Command, Registry
from . import classify
from .common import GENERATED_BANNER, header_guard, write


# Dispatchable handles the server sends back arrive as 64-bit wire
# values and stay that way.
#
# The obvious signature for an output handle is ``VkInstance*``, and it
# is wrong: a dispatchable handle is a pointer, so on a 32-bit client it
# is four bytes, and deserializing the server's 64-bit handle into one
# silently drops the top half. The truncated value then goes back on the
# wire as if it were the server's handle, and the server dereferences a
# number no object was ever at. A 32-bit guest — DXVK running an old
# Direct3D game — got exactly that far and then hung inside the driver.
#
# So an output dispatchable handle is a ``std::uint64_t*`` here: the
# wire's width, on both platforms. The ICD wraps that raw value into the
# handle it gives the application. On a 64-bit build this generates the
# same code it always did.
def _out_handle_is_raw(reg: Registry, p, params) -> bool:
    if p.type.base not in classify.DISPATCHABLE_HANDLES:
        return False
    if classify.classify(reg, p, params) in ("out_handle", "out_handle_array"):
        return True
    # The array half of a count/array pair — vkEnumeratePhysicalDevices'
    # pPhysicalDevices is this shape, and it is the first handle a client
    # ever receives.
    index = params.index(p)
    return index > 0 and classify.is_count_array_pair(params, index - 1)


def _signature(reg: Registry, cmd: Command) -> str:
    args = ["::rvk::protocol::Transport& t"]
    for p in cmd.params:
        if _out_handle_is_raw(reg, p, cmd.params):
            args.append(f"std::uint64_t* {p.name}")
        else:
            args.append(f"{classify.param_cpp(p.type)} {p.name}")
    return f"{cmd.return_type.cpp()} rvk_{cmd.name}({', '.join(args)})"


def _emit_body(reg: Registry, cmd: Command) -> list[str]:  # noqa: PLR0915
    is_void = cmd.return_type.base == "void"
    is_async = cmd.sync_mode == "async"
    on_fail = "    return;" if is_void else "    return VK_ERROR_DEVICE_LOST;"

    fmt, fmt_args = classify.trace_format_and_args(cmd, reg)
    fmt_args_str = ("," + ", ".join(fmt_args)) if fmt_args else ""
    lines: list[str] = [
        f'RVK_TRACE("client {cmd.name}{fmt}"{fmt_args_str});',
        "::av::WireWriter payload;",
    ]

    # ---- Encode inputs ----
    i = 0
    has_array_pair = False
    while i < len(cmd.params):
        p = cmd.params[i]
        if classify.is_count_array_pair(cmd.params, i):
            count = cmd.params[i]
            arr = cmd.params[i + 1]
            lines.append(
                f"const std::uint8_t array_present = {arr.name} != nullptr ? 1 : 0;"
            )
            lines.append("payload.write_pod(array_present);")
            lines.append("if (array_present) {")
            lines.append(f"    payload.write_pod<std::uint32_t>(*{count.name});")
            lines.append("}")
            has_array_pair = True
            i += 2
            continue

        kind = classify.classify(reg, p, cmd.params)
        if kind == "in_null":
            lines.append(f"(void){p.name};  // {p.type.base} not crossed over the wire")
        elif kind == "in_handle":
            lines.append(f"serialize_{p.type.base}(payload, {p.name});")
        elif kind == "in_pod":
            # size_t is the machine's width, and the wire's is 64 bits.
            #
            # This was fixed for struct members and missed for command
            # parameters, which is the same bug in the one place it was
            # still able to hide. vkGetDescriptorEXT takes a size_t
            # dataSize: a 32-bit client wrote four bytes where the server
            # read eight, so every one of those calls arrived four bytes
            # short and was rejected — 264,753 of them in a two minute
            # run, while the application was told each had succeeded and
            # drew with no descriptors bound.
            if classify.is_size_t(p.type):
                lines.append(f"payload.write_pod<std::uint64_t>({p.name});")
            else:
                lines.append(f"payload.write_pod({p.name});")
        elif kind == "out_handle":
            pass  # output, no payload encoding
        elif kind == "in_string":
            lines.append("{")
            lines.append(f"    const char* s = {p.name};")
            lines.append("    if (s == nullptr) {")
            lines.append("        payload.write_varint(0);")
            lines.append("    } else {")
            lines.append("        const std::size_t n = std::strlen(s) + 1u;")
            lines.append("        payload.write_varint(n);")
            lines.append("        payload.write(s, n);")
            lines.append("    }")
            lines.append("}")
        elif kind == "in_struct_ptr":
            lines.append(f"serialize_{p.type.base}(payload, *{p.name});")
        elif kind in ("in_handle_array", "in_struct_array", "in_pod_array"):
            # A nullable array (Vulkan optional="true" on the pointer, e.g.
            # vkCmdBindVertexBuffers2's pSizes/pStrides) may arrive null;
            # guard it with a presence byte so the loop never walks off a
            # null pointer, and the server can hand the driver null back.
            elem_line = (
                f"    payload.write_pod({p.name}[__i]);"
                if kind == "in_pod_array"
                else f"    serialize_{p.type.base}(payload, {p.name}[__i]);"
            )
            nullable = bool(p.optional_levels) and p.optional_levels[0]
            pad = ""
            if nullable:
                lines.append(
                    f"const std::uint8_t {p.name}_present = {p.name} != nullptr ? 1 : 0;"
                )
                lines.append(f"payload.write_pod({p.name}_present);")
                lines.append(f"if ({p.name}_present) {{")
                pad = "    "
            lines.append(
                f"{pad}for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>({p.len_expr}); ++__i) {{"
            )
            lines.append(f"{pad}{elem_line}")
            lines.append(f"{pad}}}")
            if nullable:
                lines.append("}")
        elif kind in ("in_struct_array2", "in_pod_array2"):
            # Outer array of pointers; each inner array's length comes
            # from a member of the matching element of a sibling array.
            sib, _, member = p.inner_len.partition(".")
            lines.append(f"for (std::uint32_t __i = 0; __i < static_cast<std::uint32_t>({p.len_expr}); ++__i) {{")
            lines.append(f"    const std::uint32_t __n = static_cast<std::uint32_t>({sib}[__i].{member});")
            lines.append(f"    for (std::uint32_t __j = 0; __j < __n; ++__j) {{")
            if kind == "in_struct_array2":
                lines.append(f"        serialize_{p.type.base}(payload, {p.name}[__i][__j]);")
            else:
                lines.append(f"        payload.write_pod({p.name}[__i][__j]);")
            lines.append("    }")
            lines.append("}")
        elif kind == "in_byte_array":
            lines.append(f"if ({p.name} != nullptr && {p.len_expr} > 0) {{")
            lines.append(
                f"    payload.write({p.name}, static_cast<std::size_t>({p.len_expr}));"
            )
            lines.append("}")
        elif kind == "in_fixed_array":
            lines.append(
                f"for (std::uint32_t __i = 0; __i < {p.type.fixed_dim}u; ++__i) {{"
            )
            lines.append(f"    payload.write_pod({p.name}[__i]);")
            lines.append("}")
        elif kind == "out_struct":
            # sType-bearing structs need their discriminator on the wire so
            # the server reconstructs the right variant; sType-less v1
            # structs send nothing.
            #
            # The chain goes out too, even though this is an output
            # parameter. The application chains empty structs onto an
            # output struct to say *which* extended properties it wants
            # filled in — the query is the chain. Sending an empty one
            # makes the driver answer about core Vulkan only, and every
            # extended feature reads back as unsupported.
            if reg.structs[p.type.base].has_stype:
                lines.append(f"payload.write_pod({p.name}->sType);")
                lines.append(f"serialize_pnext_chain(payload, {p.name}->pNext);")
        i += 1

    # ---- Send / receive ----
    if is_async:
        lines.append(
            f"(void)t.send_async(::av::vkproto::kCmd_{cmd.name}, payload.bytes());"
        )
        lines.append(
            f'RVK_TRACE("client {cmd.name} -> (async, no reply expected)");'
        )
        # Async-with-result (e.g. vkQueueSubmit when sync_override = "async")
        # is optimistic: errors come back out-of-band on the control stream.
        # Return VK_SUCCESS so the caller's success path runs.
        if not is_void:
            lines.append("return VK_SUCCESS;")
        return lines

    lines.append("::av::WireWriter reply;")
    lines.append(
        f"if (t.send_sync(::av::vkproto::kCmd_{cmd.name}, payload.bytes(), &reply) "
        "!= ::rvk::protocol::TransportResult::Ok) {"
    )
    lines.append(on_fail)
    lines.append("}")
    lines.append("::av::WireReader r{reply.bytes()};")
    # Must match the width the server wrote — see classify.returns_wide.
    if classify.returns_wide(cmd):
        lines.append("std::uint64_t result_code = 0;")
    else:
        lines.append("std::int32_t result_code = 0;")
    lines.append("if (!r.read_pod(result_code)) {")
    lines.append(on_fail)
    lines.append("}")

    # ---- Decode out-params ----
    i = 0
    while i < len(cmd.params):
        p = cmd.params[i]
        if classify.is_count_array_pair(cmd.params, i):
            count = cmd.params[i]
            arr = cmd.params[i + 1]
            lines.append(f"if (!r.read_pod(*{count.name})) {{")
            lines.append(on_fail)
            lines.append("}")
            element_is_struct = arr.type.base in reg.structs
            element_is_handle = arr.type.base in reg.handles
            if element_is_struct:
                lines.append("::av::WireArena arena_arr;")
            lines.append("if (array_present) {")
            lines.append(f"    for (std::uint32_t k = 0; k < *{count.name}; ++k) {{")
            if element_is_struct:
                lines.append(
                    f"        if (!deserialize_{arr.type.base}(r, &{arr.name}[k], arena_arr)) {{"
                )
            elif arr.type.base in classify.DISPATCHABLE_HANDLES:
                # Raw 64 bits: the caller's array is std::uint64_t[] so
                # nothing is narrowed on a 32-bit client.
                lines.append(f"        if (!r.read_pod({arr.name}[k])) {{")
            elif element_is_handle:
                lines.append(
                    f"        if (!deserialize_{arr.type.base}(r, {arr.name}[k])) {{"
                )
            else:  # POD / enum
                lines.append(f"        if (!r.read_pod({arr.name}[k])) {{")
            lines.append("        " + on_fail.lstrip())
            lines.append("        }")
            lines.append("    }")
            lines.append("}")
            i += 2
            continue

        kind = classify.classify(reg, p, cmd.params)
        if kind == "out_handle":
            # read_pod of the raw 64 bits, not deserialize_<H>, which
            # would narrow it to the local pointer width.
            lines.append(f"if (!r.read_pod(*{p.name})) {{")
            lines.append(on_fail)
            lines.append("}")
        elif kind == "out_struct":
            lines.append("::av::WireArena arena_out;")
            lines.append(f"if (!deserialize_{p.type.base}(r, {p.name}, arena_out)) {{")
            lines.append(on_fail)
            lines.append("}")
        elif kind == "out_pod":
            lines.append(f"if (!r.read_pod(*{p.name})) {{")
            lines.append(on_fail)
            lines.append("}")
        elif kind == "out_byte_array":
            # Raw bytes straight back into the caller's buffer — the
            # application already sized it and owns it.
            lines.append(f"if ({p.name} != nullptr && {p.len_expr} > 0) {{")
            lines.append(
                f"    if (!r.read({p.name}, static_cast<std::size_t>({p.len_expr}))) {{"
            )
            lines.append("    " + on_fail.lstrip())
            lines.append("    }")
            lines.append("}")
        elif kind in ("out_handle_array", "out_struct_array", "out_pod_array"):
            elem = p.type.base
            if kind == "out_struct_array":
                lines.append("::av::WireArena arena_out_arr;")
            lines.append(
                f"for (std::uint32_t __k = 0; __k < static_cast<std::uint32_t>({p.len_expr}); ++__k) {{"
            )
            if kind == "out_handle_array":
                lines.append(f"    if (!r.read_pod({p.name}[__k])) {{")
                lines.append("    " + on_fail.lstrip())
                lines.append("    }")
            elif kind == "out_struct_array":
                lines.append(
                    f"    if (!deserialize_{elem}(r, &{p.name}[__k], arena_out_arr)) {{"
                )
                lines.append("    " + on_fail.lstrip())
                lines.append("    }")
            else:  # out_pod_array
                lines.append(f"    if (!r.read_pod({p.name}[__k])) {{")
                lines.append("    " + on_fail.lstrip())
                lines.append("    }")
            lines.append("}")
        i += 1

    if has_array_pair:
        # Nothing — variable was consumed already above; suppresses the
        # "unused variable" pylint warning that this block used to need.
        pass

    lines.append(
        f'RVK_TRACE("client {cmd.name} -> result_code={{}}", result_code);'
    )
    if not is_void:
        lines.append(f"return static_cast<{cmd.return_type.cpp()}>(result_code);")
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
        '#include "rvk/protocol/transport.hpp"',
        "",
        "namespace av::vkproto {",
        "",
    ]
    for cmd in cmds:
        header.append(_signature(reg, cmd) + ";")
    header.append("")
    header.append("}  // namespace av::vkproto")
    header.append("")
    write(out_dir / "vk_dispatch_client.h", header_guard("\n".join(header[1:])))

    # ---- Source ----
    source = [
        GENERATED_BANNER,
        "",
        '#include "vk_dispatch_client.h"',
        "",
        # rvk/instr/log.hpp, not rvk/log.hpp — the latter has never
        # existed. The committed output had been corrected by hand, which
        # is why check-codegen could not pass and why regenerating broke
        # the build.
        '#include "rvk/instr/log.hpp"',
        '#include "proto/wire/wire_writer.hpp"',
        '#include "vk_protocol_ids.h"',
        '#include "vk_serialize.h"',
        "",
        "namespace av::vkproto {",
        "",
    ]
    for cmd in cmds:
        source.append(_signature(reg, cmd) + " {")
        for line in _emit_body(reg, cmd):
            source.append("    " + line)
        source.append("}")
        source.append("")
    source.append("}  // namespace av::vkproto")
    source.append("")
    write(out_dir / "vk_dispatch_client.cpp", "\n".join(source))
