from __future__ import annotations

from pathlib import Path

from ..ir import Command, Registry
from . import classify, serialize
from .common import GENERATED_BANNER, header_guard, write


def _out_handle_is_raw(reg: Registry, p, params) -> bool:
    if p.type.base not in classify.DISPATCHABLE_HANDLES:
        return False
    if classify.classify(reg, p, params) in ("out_handle", "out_handle_array"):
        return True
    index = params.index(p)
    return index > 0 and classify.is_count_array_pair(params, index - 1)


def _signature(reg: Registry, cmd: Command, translates: bool = True) -> str:
    args = ["::av::ClientTransport& t", "HandleTranslation& handles" if translates else "HandleTranslation&"]
    for p in cmd.params:
        if _out_handle_is_raw(reg, p, cmd.params):
            args.append(f"uint64_t* {p.name}")
        else:
            args.append(f"{classify.param_cpp(p.type)} {p.name}")
    return f"{cmd.return_type.cpp()} av_{cmd.name}({', '.join(args)})"


def _emit_body(reg: Registry, cmd: Command) -> list[str]:  # noqa: PLR0915
    is_void = cmd.return_type.base == "void"
    is_async = cmd.sync_mode == "async"
    if is_void:
        on_fail = "    return;"
    elif cmd.return_type.base == "VkResult":
        on_fail = "    return VK_ERROR_DEVICE_LOST;"
    else:
        on_fail = f"    return static_cast<{cmd.return_type.cpp()}>(0);"

    lines: list[str] = ["::av::WireWriter payload = t.request_writer();"]

    i = 0
    while i < len(cmd.params):
        p = cmd.params[i]
        if classify.is_count_array_pair(cmd.params, i):
            count = cmd.params[i]
            arr = cmd.params[i + 1]
            lines.append(
                f"const uint8_t array_present = {arr.name} != nullptr ? 1 : 0;"
            )
            lines.append("payload.write_pod(array_present);")
            lines.append("if (array_present) {")
            if classify.is_size_blob(count, arr):
                lines.append(f"    payload.write_pod<uint64_t>(*{count.name});")
            else:
                lines.append(f"    payload.write_pod<uint32_t>(*{count.name});")
            lines.append("}")
            i += 2
            continue

        kind = classify.classify(reg, p, cmd.params)
        if kind == "in_null":
            lines.append(f"(void){p.name};  // {p.type.base} not crossed over the wire")
        elif kind == "in_handle":
            lines.append(f"serialize_{p.type.base}(payload, handles, {p.name});")
        elif kind == "in_pod":
            if classify.is_size_t(p.type):
                lines.append(f"payload.write_pod<uint64_t>({p.name});")
            else:
                lines.append(f"payload.write_pod({p.name});")
        elif kind == "out_handle":
            pass
        elif kind == "in_string":
            lines.append("{")
            lines.append(f"    const char* s = {p.name};")
            lines.append("    if (s == nullptr) {")
            lines.append("        payload.write_varint(0);")
            lines.append("    } else {")
            lines.append("        const size_t n = __builtin_strlen(s) + 1u;")
            lines.append("        payload.write_varint(n);")
            lines.append("        payload.write(s, n);")
            lines.append("    }")
            lines.append("}")
        elif kind == "in_struct_ptr":
            lines.append(f"{serialize.serialize_call(reg, p.type.base, 'payload', f'*{p.name}')};")
        elif kind in ("in_handle_array", "in_struct_array", "in_pod_array"):
            elem_line = (
                f"    payload.write_pod({p.name}[av_i]);"
                if kind == "in_pod_array"
                else f"    {serialize.serialize_call(reg, p.type.base, 'payload', f'{p.name}[av_i]')};"
            )
            nullable = bool(p.optional_levels) and p.optional_levels[0]
            pad = ""
            if nullable:
                lines.append(
                    f"const uint8_t {p.name}_present = {p.name} != nullptr ? 1 : 0;"
                )
                lines.append(f"payload.write_pod({p.name}_present);")
                lines.append(f"if ({p.name}_present) {{")
                pad = "    "
            lines.append(
                f"{pad}for (uint32_t av_i = 0; av_i < static_cast<uint32_t>({p.len_expr}); ++av_i) {{"
            )
            lines.append(f"{pad}{elem_line}")
            lines.append(f"{pad}}}")
            if nullable:
                lines.append("}")
        elif kind in ("in_struct_array2", "in_pod_array2"):
            sib, _, member = p.inner_len.partition(".")
            lines.append(f"for (uint32_t av_i = 0; av_i < static_cast<uint32_t>({p.len_expr}); ++av_i) {{")
            lines.append(f"    const uint32_t av_n = static_cast<uint32_t>({sib}[av_i].{member});")
            lines.append(f"    for (uint32_t av_j = 0; av_j < av_n; ++av_j) {{")
            if kind == "in_struct_array2":
                lines.append(f"        {serialize.serialize_call(reg, p.type.base, 'payload', f'{p.name}[av_i][av_j]')};")
            else:
                lines.append(f"        payload.write_pod({p.name}[av_i][av_j]);")
            lines.append("    }")
            lines.append("}")
        elif kind == "in_template_blob":
            handle = classify.template_sibling(cmd.params)
            lines.append(
                f"const uint64_t av_blob = ({p.name} != nullptr)"
                f" ? handles.template_bytes({handle}) : 0;"
            )
            lines.append("payload.write_varint(av_blob);")
            lines.append(
                f"if (av_blob != 0) {{ payload.write({p.name},"
                " static_cast<size_t>(av_blob)); }"
            )
        elif kind == "in_byte_array":
            lines.append(f"if ({p.name} != nullptr && {p.len_expr} > 0) {{")
            lines.append(
                f"    payload.write({p.name}, static_cast<size_t>({p.len_expr}));"
            )
            lines.append("}")
        elif kind == "in_fixed_array":
            lines.append(
                f"for (uint32_t av_i = 0; av_i < {p.type.fixed_dim}u; ++av_i) {{"
            )
            lines.append(f"    payload.write_pod({p.name}[av_i]);")
            lines.append("}")
        elif kind == "out_struct":
            if reg.structs[p.type.base].has_stype:
                lines.append(f"payload.write_pod({p.name}->sType);")
                lines.append(f"serialize_pnext_chain(payload, handles, {p.name}->pNext);")
            for arr, count in serialize.caller_arrays(reg, reg.structs[p.type.base]):
                lines.append(f"payload.write_pod({p.name}->{count.name});")
                lines.append(f"payload.write_pod<uint8_t>({p.name}->{arr.name} != nullptr ? 1 : 0);")
        i += 1

    if is_async:
        lines.append(
            f"if (t.send_async(::av::vkproto::kCmd_{cmd.name}, payload) "
            "!= ::av::TransportResult::ok) {"
        )
        lines.append(on_fail)
        lines.append("}")
        if not is_void:
            lines.append("return VK_SUCCESS;")
        return lines

    lines.append("const ::av::SyncCallScope av_call{t};")
    lines.append("::av::WireReader r{nullptr, 0u};")
    lines.append(
        f"if (t.send_sync(::av::vkproto::kCmd_{cmd.name}, payload, r) "
        "!= ::av::TransportResult::ok) {"
    )
    lines.append(on_fail)
    lines.append("}")
    if classify.returns_wide(cmd):
        lines.append("uint64_t result_code = 0;")
    else:
        lines.append("int32_t result_code = 0;")
    lines.append("if (!r.read_pod(result_code)) {")
    lines.append(on_fail)
    lines.append("}")

    i = 0
    while i < len(cmd.params):
        p = cmd.params[i]
        if classify.is_count_array_pair(cmd.params, i):
            count = cmd.params[i]
            arr = cmd.params[i + 1]
            answered = f"{count.name}_answered"
            if classify.is_size_blob(count, arr):
                lines.append(f"uint64_t {answered} = 0;")
                lines.append(f"if (!r.read_pod({answered})) {{")
                lines.append(on_fail)
                lines.append("}")
                lines.append(f"if (array_present && ({answered} > *{count.name} ||")
                lines.append(f"    ({answered} != 0 && !r.read({arr.name}, static_cast<size_t>({answered}))))) {{")
                lines.append(on_fail)
                lines.append("}")
                lines.append(f"*{count.name} = static_cast<size_t>({answered});")
                i += 2
                continue
            lines.append(f"uint32_t {answered} = 0;")
            lines.append(f"if (!r.read_pod({answered})) {{")
            lines.append(on_fail)
            lines.append("}")
            element_is_struct = arr.type.base in reg.structs
            element_is_handle = arr.type.base in reg.handles
            lines.append("if (array_present) {")
            lines.append(f"    if ({answered} > *{count.name}) {{")
            lines.append("        " + on_fail.lstrip())
            lines.append("    }")
            lines.append(f"    for (uint32_t k = 0; k < {answered}; ++k) {{")
            if element_is_struct:
                lines.append(
                    f"        if (!{serialize.deserialize_call(reg, arr.type.base, 'r', f'&{arr.name}[k]', 't.decode_arena()')}) {{"
                )
            elif arr.type.base in classify.DISPATCHABLE_HANDLES:
                lines.append(f"        if (!r.read_pod({arr.name}[k])) {{")
            elif element_is_handle:
                lines.append(
                    f"        if (!deserialize_{arr.type.base}(r, handles, {arr.name}[k])) {{"
                )
            else:
                lines.append(f"        if (!r.read_pod({arr.name}[k])) {{")
            lines.append("        " + on_fail.lstrip())
            lines.append("        }")
            lines.append("    }")
            lines.append("}")
            lines.append(f"*{count.name} = {answered};")
            i += 2
            continue

        kind = classify.classify(reg, p, cmd.params)
        if kind == "out_handle":
            lines.append(f"if (!r.read_pod(*{p.name})) {{")
            lines.append(on_fail)
            lines.append("}")
        elif kind == "out_struct":
            lines.append(f"if (!{serialize.deserialize_call(reg, p.type.base, 'r', p.name, 't.decode_arena()')}) {{")
            lines.append(on_fail)
            lines.append("}")
        elif kind == "out_pod":
            lines.append(f"if (!r.read_pod(*{p.name})) {{")
            lines.append(on_fail)
            lines.append("}")
        elif kind == "out_byte_array":
            lines.append(f"if ({p.name} != nullptr && {p.len_expr} > 0) {{")
            lines.append(
                f"    if (!r.read({p.name}, static_cast<size_t>({p.len_expr}))) {{"
            )
            lines.append("    " + on_fail.lstrip())
            lines.append("    }")
            lines.append("}")
        elif kind in ("out_handle_array", "out_struct_array", "out_pod_array"):
            elem = p.type.base
            lines.append(
                f"for (uint32_t av_k = 0; av_k < static_cast<uint32_t>({p.len_expr}); ++av_k) {{"
            )
            if kind == "out_handle_array":
                lines.append(f"    if (!r.read_pod({p.name}[av_k])) {{")
                lines.append("    " + on_fail.lstrip())
                lines.append("    }")
            elif kind == "out_struct_array":
                lines.append(
                    f"    if (!{serialize.deserialize_call(reg, elem, 'r', f'&{p.name}[av_k]', 't.decode_arena()')}) {{"
                )
                lines.append("    " + on_fail.lstrip())
                lines.append("    }")
            else:
                lines.append(f"    if (!r.read_pod({p.name}[av_k])) {{")
                lines.append("    " + on_fail.lstrip())
                lines.append("    }")
            lines.append("}")
        i += 1

    if not is_void:
        lines.append(f"return static_cast<{cmd.return_type.cpp()}>(result_code);")
    return lines


def emit(reg: Registry, out_dir: Path) -> None:
    cmds = sorted(reg.commands.values(), key=lambda c: c.cmd_id)

    header = [
        GENERATED_BANNER,
        "",
        "#include <stdint.h>",
        "",
        "#include <vulkan/vulkan_core.h>",
        "",
        '#include "proto/client/client_transport.hpp"',
        "",
        "namespace av::vkproto {",
        "",
        "// What every handle a command names crosses the wire as",
        "// (vk_handle_translation.h).",
        "class HandleTranslation;",
        "",
    ]
    for cmd in cmds:
        header.append(_signature(reg, cmd) + ";")
    header.append("")
    header.append("}  // namespace av::vkproto")
    header.append("")
    write(out_dir / "vk_dispatch_client.h", header_guard("\n".join(header[1:])))

    source = [
        GENERATED_BANNER,
        "",
        '#include "vk_dispatch_client.h"',
        "",
        '#include "core/trace/function_scope.hpp"',
        '#include "proto/client/sync_call_scope.hpp"',
        '#include "proto/wire/wire_writer.hpp"',
        '#include "vk_protocol_ids.h"',
        '#include "vk_serialize.h"',
        "",
        "namespace av::vkproto {",
        "",
    ]
    for cmd in cmds:
        body = _emit_body(reg, cmd)
        source.append(_signature(reg, cmd, any("handles" in line for line in body)) + " {")
        source.append(f'    AV_SCOPE("av_{cmd.name}");')
        for line in body:
            source.append("    " + line)
        source.append("}")
        source.append("")
    source.append("}  // namespace av::vkproto")
    source.append("")
    write(out_dir / "vk_dispatch_client.cpp", "\n".join(source))
