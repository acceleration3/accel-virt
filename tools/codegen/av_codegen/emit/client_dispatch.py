"""Generate ``vk_dispatch_client.{h,cpp}``: ``av_vkXxx`` wrappers that
serialize their args, ship them over a ``Transport``, and decode the
reply.

Each wrapper takes a ``Transport&`` so the caller chooses the wire path
(real ICD connection, in-process loopback, recording transport, etc.).
"""

from __future__ import annotations

from pathlib import Path

from ..ir import Command, Registry
from . import classify, serialize
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
# So an output dispatchable handle is a ``uint64_t*`` here: the
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


def _signature(reg: Registry, cmd: Command, translates: bool = True) -> str:
    # The translation is every command's, so that a caller never has to ask
    # which take one; a body that encodes no handle leaves it unnamed.
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
    # A failed call returns the return type's own error: VK_ERROR_DEVICE_LOST
    # for a VkResult, a zero value for a handle getter or a count - returning a
    # negative VkResult into an unsigned handle getter was the bug this fixes.
    if is_void:
        on_fail = "    return;"
    elif cmd.return_type.base == "VkResult":
        on_fail = "    return VK_ERROR_DEVICE_LOST;"
    else:
        on_fail = f"    return static_cast<{cmd.return_type.cpp()}>(0);"

    lines: list[str] = ["::av::WireWriter payload = t.request_writer();"]

    # ---- Encode inputs ----
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
                lines.append(f"payload.write_pod<uint64_t>({p.name});")
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
            lines.append("        const size_t n = __builtin_strlen(s) + 1u;")
            lines.append("        payload.write_varint(n);")
            lines.append("        payload.write(s, n);")
            lines.append("    }")
            lines.append("}")
        elif kind == "in_struct_ptr":
            lines.append(f"{serialize.serialize_call(reg, p.type.base, 'payload', f'*{p.name}')};")
        elif kind in ("in_handle_array", "in_struct_array", "in_pod_array"):
            # A nullable array (Vulkan optional="true" on the pointer, e.g.
            # vkCmdBindVertexBuffers2's pSizes/pStrides) may arrive null;
            # guard it with a presence byte so the loop never walks off a
            # null pointer, and the server can hand the driver null back.
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
            # Outer array of pointers; each inner array's length comes
            # from a member of the matching element of a sibling array.
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
            # The length is the template's, and only a side that remembers the
            # template can answer it. A null blob sends no bytes rather than a
            # length with nothing behind it.
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
                lines.append(f"serialize_pnext_chain(payload, handles, {p.name}->pNext);")
            # An array the caller hangs off the struct goes out as its room
            # and whether there is one, not its elements: they are what the
            # call writes, and uninitialised handles are numbers a host
            # refuses. Without the room the driver is handed a null array and
            # answers only how many there are.
            for arr, count in serialize.caller_arrays(reg, reg.structs[p.type.base]):
                lines.append(f"payload.write_pod({p.name}->{count.name});")
                lines.append(f"payload.write_pod<uint8_t>({p.name}->{arr.name} != nullptr ? 1 : 0);")
        i += 1

    # ---- Send / receive ----
    if is_async:
        # send_async refuses a writer that overflowed its ring slot rather than
        # send a short command; a dropped frame is a failed call.
        lines.append(
            f"if (t.send_async(::av::vkproto::kCmd_{cmd.name}, payload) "
            "!= ::av::TransportResult::ok) {"
        )
        lines.append(on_fail)
        lines.append("}")
        # Async-with-result (e.g. vkQueueSubmit when sync_override = "async")
        # is optimistic: errors come back out-of-band on the control stream.
        # Return VK_SUCCESS so the caller's success path runs.
        if not is_void:
            lines.append("return VK_SUCCESS;")
        return lines

    # send_sync points `r` at the answer in the reply slot on ok; it is valid
    # until the call ends. The scope ends it on every return path below, so a
    # transport shared between threads holds the slot through the decode.
    lines.append("const ::av::SyncCallScope av_call{t};")
    lines.append("::av::WireReader r{nullptr, 0u};")
    lines.append(
        f"if (t.send_sync(::av::vkproto::kCmd_{cmd.name}, payload, r) "
        "!= ::av::TransportResult::ok) {"
    )
    lines.append(on_fail)
    lines.append("}")
    # Must match the width the server wrote — see classify.returns_wide.
    if classify.returns_wide(cmd):
        lines.append("uint64_t result_code = 0;")
    else:
        lines.append("int32_t result_code = 0;")
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
            # The count is the host's word, and the caller's array holds only
            # what the caller said it does: a reply listing more than that is
            # a failed call, refused before a single element is written.
            answered = f"{count.name}_answered"
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
                # Raw 64 bits: the caller's array is uint64_t[] so
                # nothing is narrowed on a 32-bit client.
                lines.append(f"        if (!r.read_pod({arr.name}[k])) {{")
            elif element_is_handle:
                lines.append(
                    f"        if (!deserialize_{arr.type.base}(r, handles, {arr.name}[k])) {{"
                )
            else:  # POD / enum
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
            # read_pod of the raw 64 bits, not deserialize_<H>, which
            # would narrow it to the local pointer width.
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
            # Raw bytes straight back into the caller's buffer — the
            # application already sized it and owns it.
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
            else:  # out_pod_array
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

    # ---- Header ----
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

    # ---- Source ----
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
        # One trace scope per command sent (docs/trace.md); the serializers
        # it calls run per member and are not marked.
        source.append(f'    AV_SCOPE("av_{cmd.name}");')
        for line in body:
            source.append("    " + line)
        source.append("}")
        source.append("")
    source.append("}  // namespace av::vkproto")
    source.append("")
    write(out_dir / "vk_dispatch_client.cpp", "\n".join(source))
