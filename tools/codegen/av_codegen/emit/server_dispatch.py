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
from . import classify, serialize
from .common import GENERATED_BANNER, header_guard, write


def _interface_signature(cmd: Command) -> str:
    args = ", ".join(f"{classify.param_cpp(p.type)} {p.name}" for p in cmd.params)
    return f"virtual {cmd.return_type.cpp()} on_{cmd.name}({args}) = 0;"


def _storage_decls(name: str, elem: str) -> list[str]:
    return [
        f"    {elem}* {name} = nullptr;",
        f"    size_t {name}_count = 0;",
    ]


def _storage_alloc(
    pad: str, name: str, elem: str, count_expr: str, *, zero_fill: bool = True
) -> list[str]:
    """Room for an array whose length the frame gives.

    The length is the sender's to choose, so the storage is the caller's
    arena block rather than a container sized by whoever sent the frame: a
    length the block cannot hold answers null and the command is refused.
    Zeroed unless every byte is about to be overwritten, because the driver
    may fill fewer elements than were asked for and the reply serializes
    what is there.
    """
    lines = [
        f"{pad}{name}_count = static_cast<size_t>({count_expr});",
        f"{pad}if ({name}_count != 0) {{",
        f"{pad}    {name} = arena.allocate<{elem}>({name}_count);",
        f"{pad}    if ({name} == nullptr) return false;",
    ]
    if zero_fill:
        lines.append(
            f"{pad}    for (size_t av_z = 0; av_z < {name}_count; ++av_z) "
            f"{{ {name}[av_z] = {{}}; }}"
        )
    lines.append(f"{pad}}}")
    return lines


def _failed(cmd: Command) -> str | None:
    """Whether the call failed, after it, or None for one that cannot.

    A VkResult below zero is an error; VK_INCOMPLETE and the other positive
    codes are successes whose outputs mean what they say."""
    if cmd.return_type.base == "VkResult" and cmd.return_type.pointer_depth == 0:
        return "static_cast<int32_t>(result) < 0"
    return None


def _cleared_storage(name: str) -> str:
    return (f"        for (size_t av_z = 0; av_z < {name}_storage_count; ++av_z) "
            f"{{ {name}_storage[av_z] = {{}}; }}")


# Commands that end objects without ending the one they name. vk.xml says
# which handle a command destroys only by the command's name, and says nothing
# of a reset that frees every set a pool gave out - so this is the one place
# that knowledge is written down.
_ENDS_CHILDREN = {"vkResetDescriptorPool": "descriptorPool"}


def _handles_in_struct(reg: Registry, name: str, seen: set[str]) -> set[str]:
    if name in seen or name not in reg.structs:
        return set()
    seen.add(name)
    found: set[str] = set()
    for m in reg.structs[name].members:
        if m.type.base in reg.handles:
            found.add(m.type.base)
        else:
            found |= _handles_in_struct(reg, m.type.base, seen)
    return found


def _made_through(reg: Registry, cmd: Command) -> str | None:
    """The expression naming the object a command makes its handles through.

    A host that forgets what a destroy ends has to forget what was made through
    it too - a device's buffers, a pool's command buffers - or the guest could
    still name them after the driver freed them. vk.xml gives every handle type
    its parent type; the object is the command's argument of that type, either
    a parameter or a member of the create info a parameter points to.
    """
    made: set[str] = set()
    for p in cmd.params:
        if p.direction != "out":
            continue
        if p.type.base in reg.handles:
            made.add(p.type.base)
        else:
            made |= _handles_in_struct(reg, p.type.base, set())
    parents = {reg.handles[h].parent for h in made} - {None}
    if not parents:
        return None
    if len(parents) != 1:
        raise ValueError(f"{cmd.name} makes handles through {sorted(parents)}; want one parent")
    parent = parents.pop()
    for p in cmd.params:
        if p.direction == "in" and p.type.base == parent and p.type.pointer_depth == 0:
            return f"as_number({p.name})"
    for p in cmd.params:
        if classify.classify(reg, p, cmd.params) != "in_struct_ptr":
            continue
        for m in reg.structs[p.type.base].members:
            if m.type.base == parent and m.type.pointer_depth == 0:
                return f"as_number({p.name}_local.{m.name})"
    raise ValueError(f"{cmd.name} makes a handle through {parent} and names none")


def _handle_slots(reg: Registry, name: str, counted: frozenset[str] = frozenset()) -> int:
    """How many handles one of `name` carries out in its own members.

    The host must have room to remember every handle a reply carries before
    the driver makes them (handles_room). Handles behind a pointer are counted
    by the caller only where the structure is the command's own output and
    names the room it gave (`counted`, its caller arrays); anywhere else a
    count no member gives is refused here rather than left uncounted."""
    if name not in reg.structs:
        return 0
    slots = 0
    for m in reg.structs[name].members:
        if m.name == "pNext" or m.name in counted:
            continue
        dim = m.type.fixed_dim or 1
        if m.type.base in reg.handles:
            if m.type.pointer_depth:
                raise ValueError(f"{name}::{m.name} answers handles through a pointer; "
                                 "no room for them could be counted")
            slots += dim
        elif m.type.pointer_depth == 0:
            slots += dim * _handle_slots(reg, m.type.base)
    return slots


def _ending_lines(reg: Registry, cmd: Command) -> list[str]:
    """What a destroy, free or reset tells the host has ended."""
    if cmd.name in _ENDS_CHILDREN:
        return [f"    handle_children_ended(as_number({_ENDS_CHILDREN[cmd.name]}));"]
    if not cmd.name.startswith(("vkDestroy", "vkFree")):
        return []
    ended = [p for p in cmd.params if p.direction == "in" and p.type.base in reg.handles]
    if not ended:
        raise ValueError(f"{cmd.name} destroys and names no handle")
    last = ended[-1]
    if classify.classify(reg, last, cmd.params) == "in_handle":
        return [f"    handle_ended(as_number({last.name}));"]
    return [
        f"    for (size_t av_e = 0; av_e < {last.name}_storage_count; ++av_e) {{",
        f"        handle_ended(as_number({last.name}_storage[av_e]));",
        "    }",
    ]


def _emit_case(reg: Registry, cmd: Command) -> list[str]:  # noqa: PLR0915
    is_void = cmd.return_type.base == "void"
    # One trace scope per decoded command, inside the case's own block
    # (docs/trace.md).
    lines: list[str] = [f"case ::av::vkproto::kCmd_{cmd.name}: {{", f'    AV_SCOPE("server:{cmd.name}");']

    decl_lines: list[str] = []
    decode_lines: list[str] = []
    call_args: list[str] = []
    out_after_call: list[str] = []
    # A failed call's outputs are whatever the driver left in them, which may
    # be a handle it never made - and serializing a handle is what makes the
    # host accept it back. So each output is put back to nothing first.
    failed = _failed(cmd)
    cleared_on_failure: list[str] = []
    # The handles the reply may carry, each of which the host remembers as it
    # is serialized: room for them is asked for before the driver makes any.
    room_terms: list[str] = []

    i = 0
    while i < len(cmd.params):
        p = cmd.params[i]
        if classify.is_count_array_pair(cmd.params, i):
            count = cmd.params[i]
            arr = cmd.params[i + 1]
            decl_lines.append("    uint8_t array_present = 0;")
            decl_lines.append(f"    uint32_t {count.name}_local = 0;")
            decl_lines.extend(_storage_decls(f"{arr.name}_storage", arr.type.base))
            decode_lines.append("    if (!r.read_pod(array_present)) return false;")
            decode_lines.append("    if (array_present) {")
            decode_lines.append(f"        if (!r.read_pod({count.name}_local)) return false;")
            decode_lines.extend(
                _storage_alloc(
                    "        ", f"{arr.name}_storage", arr.type.base, f"{count.name}_local"
                )
            )
            stype_value = classify.stype_value_for_struct(reg, arr.type.base)
            if stype_value is not None:
                decode_lines.append(
                    f"        for (size_t av_e = 0; "
                    f"av_e < {arr.name}_storage_count; ++av_e) {{ "
                    f"{arr.name}_storage[av_e].sType = {stype_value}; "
                    f"{arr.name}_storage[av_e].pNext = nullptr; }}"
                )
            decode_lines.append("    }")
            call_args.append(f"&{count.name}_local")
            call_args.append(f"array_present ? {arr.name}_storage : nullptr")
            if arr.type.base in reg.handles:
                room_terms.append(f"{arr.name}_storage_count")
            elif _handle_slots(reg, arr.type.base):
                room_terms.append(f"{arr.name}_storage_count * {_handle_slots(reg, arr.type.base)}u")
            # What the driver says it listed is sent only for a call that
            # worked, and never past the room the guest gave: a count the
            # driver raised would serialize arena memory beyond the array.
            sent = f"{count.name}_sent"
            listed = f"{count.name}_local" if failed is None else f"{failed} ? 0u : {count.name}_local"
            out_after_call.append(f"    uint32_t {sent} = {listed};")
            out_after_call.append(f"    if (array_present && {sent} > {arr.name}_storage_count) {{")
            out_after_call.append(f"        {sent} = static_cast<uint32_t>({arr.name}_storage_count);")
            out_after_call.append("    }")
            out_after_call.append(f"    reply.write_pod({sent});")
            out_after_call.append("    if (array_present) {")
            out_after_call.append(f"        for (uint32_t k = 0; k < {sent}; ++k) {{")
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
            if p.null_refused and not reg.handles[p.type.base].dispatchable:
                decode_lines.append(f"    if ({p.name} == VK_NULL_HANDLE) return false;")
            call_args.append(p.name)
        elif kind == "in_pod":
            decl_lines.append(f"    {p.type.cpp()} {p.name}{{}};")
            if classify.is_size_t(p.type):
                # Read the wire's 64 bits, then narrow to this machine's
                # size_t. See the same case on the client side.
                decode_lines.append("    {")
                decode_lines.append("        uint64_t av_wide = 0;")
                decode_lines.append("        if (!r.read_pod(av_wide)) return false;")
                decode_lines.append(
                    f"        {p.name} = static_cast<{p.type.cpp()}>(av_wide);")
                decode_lines.append("    }")
            else:
                decode_lines.append(f"    if (!r.read_pod({p.name})) return false;")
            call_args.append(p.name)
        elif kind == "in_string":
            decl_lines.append(f"    const char* {p.name} = nullptr;")
            decode_lines.append("    {")
            decode_lines.append("        uint64_t n = 0;")
            decode_lines.append("        if (!r.read_varint(n)) return false;")
            decode_lines.append("        if (n > 0) {")
            decode_lines.append(
                "            char* tmp_str = arena.allocate_chars(static_cast<size_t>(n));"
            )
            decode_lines.append(
                "            if (!r.read(tmp_str, static_cast<size_t>(n))) return false;"
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
            # not.
            nullable = bool(p.optional_levels) and p.optional_levels[0]
            decl_lines.extend(_storage_decls(f"{p.name}_storage", elem))
            pad = ""
            if nullable:
                decl_lines.append(f"    uint8_t {p.name}_present = 0;")
                decode_lines.append(
                    f"    if (!r.read_pod({p.name}_present)) return false;"
                )
                decode_lines.append(f"    if ({p.name}_present) {{")
                pad = "    "
            decode_lines.extend(
                _storage_alloc(f"{pad}    ", f"{p.name}_storage", elem, srv_len)
            )
            decode_lines.append(
                f"{pad}    for (size_t av_i = 0; "
                f"av_i < {p.name}_storage_count; ++av_i) {{"
            )
            if kind == "in_handle_array":
                decode_lines.append(
                    f"{pad}        if (!deserialize_{elem}(r, {p.name}_storage[av_i])) return false;"
                )
            elif kind == "in_struct_array":
                decode_lines.append(
                    f"{pad}        if (!deserialize_{elem}(r, &{p.name}_storage[av_i], arena)) return false;"
                )
            else:  # in_pod_array
                decode_lines.append(
                    f"{pad}        if (!r.read_pod({p.name}_storage[av_i])) return false;"
                )
            decode_lines.append(f"{pad}    }}")
            if nullable:
                decode_lines.append("    }")
                call_args.append(
                    f"{p.name}_present ? {p.name}_storage : nullptr"
                )
            else:
                call_args.append(f"{p.name}_storage")
        elif kind in ("in_struct_array2", "in_pod_array2"):
            # Rebuild the pointer-to-pointer shape in the arena: an outer
            # array of pointers, each to its own inner array.
            sib, _, member = p.inner_len.partition(".")
            srv_outer = classify.server_len_expr(cmd.params, p.len_expr)
            elem = p.type.base
            decl_lines.append(f"    const {elem}** {p.name}_outer = nullptr;")
            decode_lines.append(f"    {{")
            decode_lines.append(
                f"        const uint32_t av_n_outer = static_cast<uint32_t>({srv_outer});")
            decode_lines.append(
                f"        auto** av_ptrs = arena.allocate<const {elem}*>(av_n_outer);")
            decode_lines.append("        for (uint32_t av_i = 0; av_i < av_n_outer; ++av_i) {")
            decode_lines.append(
                f"            const uint32_t av_n = static_cast<uint32_t>({sib}_storage[av_i].{member});")
            decode_lines.append(f"            auto* av_inner = arena.allocate<{elem}>(av_n);")
            decode_lines.append("            for (uint32_t av_j = 0; av_j < av_n; ++av_j) {")
            if kind == "in_struct_array2":
                decode_lines.append(
                    f"                if (!deserialize_{elem}(r, &av_inner[av_j], arena)) return false;")
            else:
                decode_lines.append("                if (!r.read_pod(av_inner[av_j])) return false;")
            decode_lines.append("            }")
            decode_lines.append("            av_ptrs[av_i] = av_inner;")
            decode_lines.append("        }")
            decode_lines.append(f"        {p.name}_outer = av_ptrs;")
            decode_lines.append("    }")
            call_args.append(f"{p.name}_outer")
        elif kind == "in_template_blob":
            handle = classify.template_sibling(cmd.params)
            decl_lines.extend(_storage_decls(f"{p.name}_storage", "uint8_t"))
            decl_lines.append("    uint64_t av_blob = 0;")
            decode_lines.append("    if (!r.read_varint(av_blob)) return false;")
            # Constitution V: the guest's length is a claim. The template this
            # side decoded says what the length must be.
            decode_lines.append(
                f"    if (av_blob != descriptor_template_bytes({handle})) return false;"
            )
            alloc = _storage_alloc(
                "    ", f"{p.name}_storage", "uint8_t", "av_blob", zero_fill=False
            )
            alloc.insert(
                len(alloc) - 1,
                f"        if (!r.read({p.name}_storage, {p.name}_storage_count)) "
                f"return false;",
            )
            decode_lines.extend(alloc)
            call_args.append(f"{p.name}_storage")
        elif kind == "in_byte_array":
            srv_len = classify.server_len_expr(cmd.params, p.len_expr)
            decl_lines.extend(_storage_decls(f"{p.name}_storage", "uint8_t"))
            # The read fills every byte, so it goes inside the allocation's
            # own guard and the block is not zeroed first.
            alloc = _storage_alloc(
                "    ", f"{p.name}_storage", "uint8_t", srv_len, zero_fill=False
            )
            alloc.insert(
                len(alloc) - 1,
                f"        if (!r.read({p.name}_storage, {p.name}_storage_count)) "
                f"return false;",
            )
            decode_lines.extend(alloc)
            call_args.append(f"{p.name}_storage")
        elif kind == "in_fixed_array":
            decl_lines.append(
                f"    {p.type.base} {p.name}_local[{p.type.fixed_dim}]{{}};"
            )
            decode_lines.append(
                f"    for (uint32_t av_i = 0; av_i < {p.type.fixed_dim}u; ++av_i) {{"
            )
            decode_lines.append(
                f"        if (!r.read_pod({p.name}_local[av_i])) return false;"
            )
            decode_lines.append("    }")
            call_args.append(f"{p.name}_local")
        elif kind == "out_handle":
            room_terms.append("1u")
            decl_lines.append(f"    {p.type.base} {p.name}_out = VK_NULL_HANDLE;")
            cleared_on_failure.append(f"        {p.name}_out = VK_NULL_HANDLE;")
            call_args.append(f"&{p.name}_out")
            out_after_call.append(f"    serialize_{p.type.base}(reply, {p.name}_out);")
        elif kind == "out_struct":
            arrays = serialize.caller_arrays(reg, reg.structs[p.type.base])
            counted = frozenset(arr.name for arr, _ in arrays)
            if _handle_slots(reg, p.type.base, counted):
                room_terms.append(f"{_handle_slots(reg, p.type.base, counted)}u")
            for arr, count in arrays:
                if arr.type.base in reg.handles:
                    room_terms.append(f"{p.name}_out.{count.name}")
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
            # The room the guest's array has, and an array of it for the
            # driver to fill; see client_dispatch for why no elements come.
            for arr, count in serialize.caller_arrays(reg, reg.structs[p.type.base]):
                if serialize._classify_member(reg, arr) == "struct_array":
                    raise ValueError(f"{p.type.base}::{arr.name}: a caller's structure array "
                                     "would need each element's sType and chain sent")
                room = f"{p.name}_out.{count.name}"
                present = f"{p.name}_{arr.name}_present"
                storage = f"{p.name}_{arr.name}_storage"
                decode_lines.append(f"    if (!r.read_pod({room})) return false;")
                decode_lines.append(f"    uint8_t {present} = 0;")
                decode_lines.append(f"    if (!r.read_pod({present})) return false;")
                decode_lines.append(f"    if ({present} != 0 && {room} != 0) {{")
                decode_lines.append(
                    f"        auto* {storage} = arena.allocate<{arr.type.base}>({room});")
                decode_lines.append(f"        if ({storage} == nullptr) return false;")
                decode_lines.append(f"        {p.name}_out.{arr.name} = {storage};")
                decode_lines.append("    }")
            cleared_on_failure.append(f"        {p.type.base} {p.name}_blank{{}};")
            if reg.structs[p.type.base].has_stype:
                cleared_on_failure.append(f"        {p.name}_blank.sType = {p.name}_out.sType;")
            cleared_on_failure.append(f"        {p.name}_out = {p.name}_blank;")
            call_args.append(f"&{p.name}_out")
            out_after_call.append(f"    serialize_{p.type.base}(reply, {p.name}_out);")
        elif kind == "out_pod":
            decl_lines.append(f"    {p.type.base} {p.name}_out{{}};")
            cleared_on_failure.append(f"        {p.name}_out = {{}};")
            call_args.append(f"&{p.name}_out")
            out_after_call.append(f"    reply.write_pod({p.name}_out);")
        elif kind == "out_byte_array":
            srv_len = classify.server_len_expr(cmd.params, p.len_expr)
            decl_lines.extend(_storage_decls(f"{p.name}_storage", "uint8_t"))
            decode_lines.extend(
                _storage_alloc("    ", f"{p.name}_storage", "uint8_t", srv_len)
            )
            call_args.append(f"{p.name}_storage")
            cleared_on_failure.append(_cleared_storage(p.name))
            out_after_call.append(f"    if ({p.name}_storage_count != 0) {{")
            out_after_call.append(
                f"        reply.write({p.name}_storage, {p.name}_storage_count);"
            )
            out_after_call.append("    }")
        elif kind in ("out_handle_array", "out_struct_array", "out_pod_array"):
            elem = p.type.base
            if kind == "out_handle_array":
                room_terms.append(f"{p.name}_storage_count")
            elif kind == "out_struct_array" and _handle_slots(reg, elem):
                room_terms.append(f"{p.name}_storage_count * {_handle_slots(reg, elem)}u")
            srv_len = classify.server_len_expr(cmd.params, p.len_expr)
            decl_lines.extend(_storage_decls(f"{p.name}_storage", elem))
            decode_lines.extend(
                _storage_alloc("    ", f"{p.name}_storage", elem, srv_len)
            )
            call_args.append(f"{p.name}_storage")
            cleared_on_failure.append(_cleared_storage(p.name))
            out_after_call.append(
                f"    for (size_t av_k = 0; av_k < {p.name}_storage_count; ++av_k) {{"
            )
            if kind in ("out_handle_array", "out_struct_array"):
                out_after_call.append(
                    f"        serialize_{elem}(reply, {p.name}_storage[av_k]);"
                )
            else:
                out_after_call.append(
                    f"        reply.write_pod({p.name}_storage[av_k]);"
                )
            out_after_call.append("    }")
        i += 1

    lines.extend(decl_lines)
    lines.extend(decode_lines)

    # Told after the driver call, whatever it answered. Every destroy and free
    # ends its objects - the spec gives them no failure that keeps one - and
    # were one kept anyway, forgetting it costs a leak the device's own
    # destruction reclaims, where remembering a freed one is a pointer the
    # guest could hand the driver again.
    after_call = _ending_lines(reg, cmd)
    parent = _made_through(reg, cmd)
    if parent is not None:
        after_call.append(f"    handles_made_through({parent});")

    room = None
    if room_terms:
        room = "handles_room(" + " + ".join(f"static_cast<uint64_t>({t})" for t in room_terms) + ")"
        if not is_void and failed is None:
            raise ValueError(f"{cmd.name} answers handles and no VkResult to refuse them with")

    if is_void:
        call = f"s.on_{cmd.name}({', '.join(call_args)});"
        lines.append(f"    if ({room}) {call}" if room else f"    {call}")
        lines.extend(after_call)
        if cmd.sync_mode == "sync":
            lines.append("    reply.write_pod<int32_t>(0);")
            lines.extend(out_after_call)
    else:
        call = f"s.on_{cmd.name}({', '.join(call_args)})"
        if room:
            # Refused before the driver makes anything the host could not
            # then remember: the guest is told the host is out of memory.
            call = f"{room} ? {call} : VK_ERROR_OUT_OF_HOST_MEMORY"
        lines.append(f"    {cmd.return_type.cpp()} result = {call};")
        lines.extend(after_call)
        # Encode the return in its own width. Most commands return
        # VkResult and fit in 32 bits, but the device-address and
        # opaque-capture-address queries return 64. Forcing those through
        # an int32 silently truncates a GPU pointer, and the shader that
        # dereferences the result faults the device — reported as
        # VK_ERROR_DEVICE_LOST, nowhere near the actual cause.
        if classify.returns_wide(cmd):
            lines.append(
                "    reply.write_pod<uint64_t>(static_cast<uint64_t>(result));"
            )
        else:
            lines.append(
                "    reply.write_pod<int32_t>(static_cast<int32_t>(result));"
            )
        if failed is not None and cleared_on_failure:
            lines.append(f"    if ({failed}) {{")
            lines.extend(cleared_on_failure)
            lines.append("    }")
        lines.extend(out_after_call)

    lines.append("    return true;")
    lines.append("}")
    return lines


def emit(reg: Registry, out_dir: Path) -> None:
    cmds = sorted(reg.commands.values(), key=lambda c: c.cmd_id)

    # ---- Header ----
    header = [
        GENERATED_BANNER,
        "",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        "#include <vulkan/vulkan_core.h>",
        "",
        # dispatch_command's signature names all three, so all three must
        # be complete here — a forward declaration of BufferReader is not
        # enough for the declaration to compile.
        '#include "proto/wire/wire_arena.hpp"',
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
        "// `arena` is where everything a decoded command points at lives, for the"
    )
    header.append(
        "// length of the call; it is the caller's block, because it has to outlive"
    )
    header.append("// the decode that filled it.")
    header.append(
        "[[nodiscard]] bool dispatch_command(ServerDispatch& s, uint32_t cmd_id, "
        "::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena);"
    )
    header.append("")
    header.extend([
        "// What a decoded command did to the handles the host answers for. A",
        "// handle leaves through handle_to_wire and comes back through",
        "// wire_to_handle; these say when one stops being an object at all, so",
        "// a host that checks the way back in stops accepting it. Weakly",
        "// defined to nothing, which is right wherever nothing is checked; see",
        "// vk_handle_lifetime_default.cpp.",
        "//",
        "// The handles the reply is about to carry were made through `parent`,",
        "// and end when it does.",
        "void handles_made_through(uint64_t parent) noexcept;",
        "// Whether `handles` more can be remembered, asked before the driver",
        "// makes any: false refuses the command as out of host memory.",
        "bool handles_room(uint64_t handles) noexcept;",
        "// `handle` was destroyed or freed, and with it what was made through it.",
        "void handle_ended(uint64_t handle) noexcept;",
        "// What was made through `handle` was freed, and `handle` was not.",
        "void handle_children_ended(uint64_t handle) noexcept;",
        "",
    ])
    header.append("}  // namespace av::vkproto")
    header.append("")
    write(out_dir / "vk_dispatch_server.h", header_guard("\n".join(header[1:])))

    # The defaults in a file of their own, for the reason the handle hooks
    # are two files (emit/serialize.py): on PE an archive member is displaced
    # whole, so a host defining these must not also be defining anything the
    # file beside them holds.
    lifetime_default = [
        GENERATED_BANNER,
        "",
        '#include "vk_dispatch_server.h"',
        "",
        "#include <stdint.h>",
        "",
        "#if defined(_WIN32)",
        "#  define AV_HOOK_DEFAULT",
        "#else",
        "#  define AV_HOOK_DEFAULT [[gnu::weak]]",
        "#endif",
        "",
        "namespace av::vkproto {",
        "",
        "AV_HOOK_DEFAULT void handles_made_through(uint64_t) noexcept {}",
        "AV_HOOK_DEFAULT bool handles_room(uint64_t) noexcept { return true; }",
        "AV_HOOK_DEFAULT void handle_ended(uint64_t) noexcept {}",
        "AV_HOOK_DEFAULT void handle_children_ended(uint64_t) noexcept {}",
        "",
        "}  // namespace av::vkproto",
        "",
    ]
    write(out_dir / "vk_handle_lifetime_default.cpp", "\n".join(lifetime_default))

    # ---- Source ----
    source = [
        GENERATED_BANNER,
        "",
        '#include "vk_dispatch_server.h"',
        "",
        '#include "core/trace/function_scope.hpp"',
        '#include "vk_protocol_ids.h"',
        '#include "vk_serialize.h"',
        "",
        "namespace av::vkproto {",
        "",
        "bool dispatch_command(ServerDispatch& s, uint32_t cmd_id, "
        "::av::WireReader& r, ::av::WireWriter& reply, ::av::WireArena& arena) {",
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
