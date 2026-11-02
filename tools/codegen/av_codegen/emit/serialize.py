from __future__ import annotations

import re
from pathlib import Path

from ..ir import Registry, Struct, StructMember
from . import classify
from .common import GENERATED_BANNER, header_guard, write


def _referenced_handle_names(reg: Registry) -> list[str]:
    seen: set[str] = set()
    for cmd in reg.commands.values():
        if cmd.return_type.base in reg.handles:
            seen.add(cmd.return_type.base)
        for p in cmd.params:
            if p.type.base in reg.handles:
                seen.add(p.type.base)
    for s in reg.structs.values():
        for m in s.members:
            if m.type.base in reg.handles:
                seen.add(m.type.base)
    return sorted(seen)


def handle_kind(name: str) -> str:
    return re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", name[2:]).lower()


def _emit_handle_helpers(name: str) -> list[str]:
    kind = handle_kind(name)
    return [
        f"inline void serialize_{name}(::av::WireWriter& buf, HandleTranslation& handles, {name} v) noexcept {{",
        f"    buf.write_pod(handles.wire_for(HandleKind::{kind}, as_number(v)));",
        "}",
        "",
        "[[nodiscard]] inline bool "
        f"deserialize_{name}(::av::WireReader& r, const HandleTranslation& handles, {name}& out) noexcept {{",
        "    uint64_t raw = 0;",
        "    if (!r.read_pod(raw)) {",
        "        return false;",
        "    }",
        "    uint64_t named = 0;",
        f"    if (!handles.handle_for(HandleKind::{kind}, raw, named)) {{",
        "        return false;",
        "    }",
        f"    out = as_handle<{name}>(named);",
        "    return true;",
        "}",
        "",
    ]


_CARRIERS: dict[int, frozenset[str]] = {}


def carriers(reg: Registry) -> frozenset[str]:
    key = id(reg)
    if key in _CARRIERS:
        return _CARRIERS[key]
    direct = ("pnext", "nested_handle", "handle_array", "template_blob")
    found = {name for name in BESPOKE_STRUCT_BODIES if name in reg.structs}
    for s in reg.structs.values():
        if s.is_union:
            continue
        if any(_classify_member(reg, m) in direct for m in s.members):
            found.add(s.name)
    nested = ("nested_struct", "struct_array", "optional_struct_ptr")
    grew = True
    while grew:
        grew = False
        for s in reg.structs.values():
            if s.name in found or s.is_union:
                continue
            if any(m.type.base in found and _classify_member(reg, m) in nested for m in s.members):
                found.add(s.name)
                grew = True
    _CARRIERS[key] = frozenset(found)
    return _CARRIERS[key]


def serialize_call(reg: Registry, type_name: str, buf: str, value: str, via: str = "handles") -> str:
    if type_name in reg.handles or type_name in carriers(reg):
        return f"serialize_{type_name}({buf}, {via}, {value})"
    return f"serialize_{type_name}({buf}, {value})"


def deserialize_call(reg: Registry, type_name: str, r: str, out: str, arena: str,
                     via: str = "handles") -> str:
    if type_name in reg.handles:
        return f"deserialize_{type_name}({r}, {via}, {out})"
    if type_name in carriers(reg):
        return f"deserialize_{type_name}({r}, {via}, {out}, {arena})"
    return f"deserialize_{type_name}({r}, {out}, {arena})"


def _len_components(m: StructMember) -> list[str]:
    if not m.len_expr:
        return []
    return [s.strip() for s in m.len_expr.split(",") if s.strip()]


def _expr_to_cpp(expr: str, prefix: str) -> str:
    import re

    def sub(m: "re.Match[str]") -> str:
        name = m.group(0)
        if name.startswith("VK_"):
            return name
        return f"as_number({prefix}{name})"

    return re.sub(r"\b([a-zA-Z_][a-zA-Z_0-9]*)\b", sub, expr)


_TEMPLATE_SIBLINGS: dict[int, str | None] = {}


def _template_sibling(reg: Registry, m: StructMember) -> str | None:
    key = id(m)
    if key not in _TEMPLATE_SIBLINGS:
        found = None
        for owner in reg.structs.values():
            if any(member is m for member in owner.members):
                found = next((o.name for o in owner.members
                              if o.type.base == classify.TEMPLATE_HANDLE and o.type.pointer_depth == 0), None)
                break
        _TEMPLATE_SIBLINGS[key] = found
    return _TEMPLATE_SIBLINGS[key]


def _classify_member(reg: Registry, m: StructMember) -> str:
    if m.name == "pNext":
        return "pnext"
    if m.type.is_fixed_array:
        return "fixed_array"

    if m.type.pointer_depth == 0:
        if m.type.base in reg.structs:
            return "nested_struct"
        if m.type.base in reg.handles:
            return "nested_handle"
        return "pod"

    parts = _len_components(m)

    if m.type.pointer_depth == 1:
        if m.type.base == "char":
            if parts == ["null-terminated"]:
                return "string"
            return "unknown_ptr"
        if m.type.base == "void":
            if parts:
                return "void_blob"
            if _template_sibling(reg, m) is not None:
                return "template_blob"
            return "unknown_ptr"
        if m.type.base in reg.structs:
            if not parts:
                return "optional_struct_ptr"
            return "struct_array"
        if m.type.base in reg.handles and parts:
            return "handle_array"
        if parts:
            return "pod_array"
        return "unknown_ptr"

    if m.type.pointer_depth == 2:
        if m.type.base == "char" and parts and parts[-1] == "null-terminated":
            return "string_array"
        return "unknown_ptr"

    return "unknown_ptr"


CALLER_ARRAY_KINDS = ("handle_array", "pod_array", "struct_array")


def caller_arrays(reg: Registry, s: Struct) -> list[tuple[StructMember, StructMember]]:
    members = {m.name: m for m in s.members}
    pairs: list[tuple[StructMember, StructMember]] = []
    for m in s.members:
        if m.type.is_const or _classify_member(reg, m) not in CALLER_ARRAY_KINDS:
            continue
        parts = _len_components(m)
        count = members.get(parts[0]) if len(parts) == 1 else None
        if count is None or count.type.base != "uint32_t" or count.type.pointer_depth != 0:
            continue
        if sum(1 for other in s.members if _len_components(other)[:1] == [count.name]) != 1:
            continue
        pairs.append((m, count))
    return pairs


def _output_structs(reg: Registry) -> set[str]:
    found: set[str] = set()
    for cmd in reg.commands.values():
        for index, p in enumerate(cmd.params):
            kind = classify.classify(reg, p, cmd.params)
            paired = index > 0 and classify.is_count_array_pair(cmd.params, index - 1)
            if kind in ("out_struct", "out_struct_array") or (paired and p.type.base in reg.structs):
                found.add(p.type.base)
    chain = _chain_nodes(reg)
    grew = True
    while grew:
        grew = False
        reached = {m.type.base for name in found for m in reg.structs[name].members
                   if m.type.base in reg.structs and m.name != "pNext"}
        reached |= {s.name for s in chain if found.intersection(s.extends)}
        if not reached <= found:
            found |= reached
            grew = True
    return found


def _fixed_array_bounds(s: Struct) -> dict[str, str]:
    members = {m.name: m for m in s.members}
    bounds: dict[str, str] = {}
    for m in s.members:
        count = members.get(m.len_expr or "")
        if not m.type.is_fixed_array or count is None or count.type.base != "uint32_t":
            continue
        bounds[count.name] = m.type.api_constant or f"{m.type.fixed_dim}u"
    return bounds


def _deserialize_bounded_count(count: StructMember, bound: str) -> list[str]:
    answered = f"{count.name}_answered"
    return [
        "    {",
        f"        decltype(out->{count.name}) {answered}{{}};",
        f"        if (!r.read_pod({answered})) {{ return false; }}",
        f"        if ({answered} > {bound}) {{ return false; }}",
        f"        out->{count.name} = {answered};",
        "    }",
    ]


def _deserialize_caller_array(reg: Registry, m: StructMember, count: StructMember) -> list[str]:
    kind = _classify_member(reg, m)
    answered = f"{count.name}_answered"
    if kind in ("struct_array", "handle_array"):
        def read(array: str) -> str:
            element = f"&{array}[i]" if kind == "struct_array" else f"{array}[i]"
            return deserialize_call(reg, m.type.base, "r", element, "arena")
    else:
        def read(array: str) -> str:
            return f"r.read_pod({array}[i])"
    return [
        "    {",
        "        uint8_t present = 0;",
        "        if (!r.read_pod(present)) { return false; }",
        f"        if (out->{m.name} != nullptr) {{",
        f"            if ({answered} > out->{count.name}) {{ return false; }}",
        f"            if (present == 0 && {answered} != 0) {{ return false; }}",
        f"            for (uint32_t i = 0; i < {answered}; ++i) {{",
        f"                if (!{read('out->' + m.name)}) {{ return false; }}",
        "            }",
        f"            out->{count.name} = {answered};",
        "        } else {",
        f"            out->{count.name} = {answered};",
        f"            if (present != 0 && {answered} != 0) {{",
        f"                auto* arr = arena.allocate<{m.type.base}>(static_cast<size_t>({answered}));",
        f"                for (uint32_t i = 0; i < {answered}; ++i) {{",
        f"                    if (!{read('arr')}) {{ return false; }}",
        "                }",
        f"                out->{m.name} = arr;",
        "            }",
        "        }",
        "    }",
    ]


def _is_size_t(m) -> bool:
    return classify.is_machine_width(m.type)


def _serialize_member(reg: Registry, m: StructMember) -> list[str]:
    kind = _classify_member(reg, m)
    if kind == "pnext":
        return ["    serialize_pnext_chain(buf, handles, v.pNext);"]
    if kind == "fixed_array":
        return [f"    buf.write(&v.{m.name}[0], sizeof(v.{m.name}));"]
    if kind in ("nested_struct", "nested_handle"):
        return [f"    {serialize_call(reg, m.type.base, 'buf', f'v.{m.name}')};"]
    if kind == "pod":
        if classify.is_machine_width(m.type):
            return [f"    buf.write_pod<uint64_t>(v.{m.name});"]
        return [f"    buf.write_pod(v.{m.name});"]
    if kind == "string":
        return [
            "    {",
            f"        const char* s = v.{m.name};",
            "        if (s == nullptr) {",
            "            buf.write_varint(0);",
            "        } else {",
            "            const size_t n = __builtin_strlen(s) + 1u;",
            "            buf.write_varint(n);",
            "            buf.write(s, n);",
            "        }",
            "    }",
        ]
    if kind == "optional_struct_ptr":
        return [
            "    {",
            f"        const auto* p = v.{m.name};",
            "        if (p == nullptr) {",
            "            buf.write_pod<uint8_t>(0);",
            "        } else {",
            "            buf.write_pod<uint8_t>(1);",
            f"            {serialize_call(reg, m.type.base, 'buf', '*p')};",
            "        }",
            "    }",
        ]
    if kind == "struct_array":
        count = _len_components(m)[0]
        return [
            "    {",
            f"        const uint8_t present = v.{m.name} != nullptr ? 1 : 0;",
            "        buf.write_pod(present);",
            "        if (present) {",
            f"            for (uint32_t i = 0; i < static_cast<uint32_t>({_expr_to_cpp(count, "v.")}); ++i) {{",
            f"                {serialize_call(reg, m.type.base, 'buf', f'v.{m.name}[i]')};",
            "            }",
            "        }",
            "    }",
        ]
    if kind == "pod_array":
        count = _len_components(m)[0]
        return [
            "    {",
            f"        const uint8_t present = v.{m.name} != nullptr ? 1 : 0;",
            "        buf.write_pod(present);",
            "        if (present) {",
            f"            for (uint32_t i = 0; i < static_cast<uint32_t>({_expr_to_cpp(count, "v.")}); ++i) {{",
            (f"                buf.write_pod<uint64_t>(v.{m.name}[i]);"
             if _is_size_t(m) else f"                buf.write_pod(v.{m.name}[i]);"),
            "            }",
            "        }",
            "    }",
        ]
    if kind == "template_blob":
        template = _template_sibling(reg, m)
        return [
            "    {",
            f"        const uint64_t av_blob = v.{m.name} != nullptr ? handles.template_bytes(v.{template}) : 0;",
            "        buf.write_varint(av_blob);",
            f"        if (av_blob != 0) {{ buf.write(v.{m.name}, static_cast<size_t>(av_blob)); }}",
            "    }",
        ]
    if kind == "void_blob":
        size = _len_components(m)[0]
        return [
            "    {",
            f"        const uint8_t present = v.{m.name} != nullptr ? 1 : 0;",
            "        buf.write_pod(present);",
            f"        if (present && {_expr_to_cpp(size, "v.")} != 0) {{",
            f"            buf.write(v.{m.name}, static_cast<size_t>({_expr_to_cpp(size, "v.")}));",
            "        }",
            "    }",
        ]
    if kind == "handle_array":
        count = _len_components(m)[0]
        return [
            "    {",
            f"        const uint8_t present = v.{m.name} != nullptr ? 1 : 0;",
            "        buf.write_pod(present);",
            "        if (present) {",
            f"            for (uint32_t i = 0; i < static_cast<uint32_t>({_expr_to_cpp(count, "v.")}); ++i) {{",
            f"                {serialize_call(reg, m.type.base, 'buf', f'v.{m.name}[i]')};",
            "            }",
            "        }",
            "    }",
        ]
    if kind == "string_array":
        count = _len_components(m)[0]
        return [
            "    {",
            f"        const uint8_t present = v.{m.name} != nullptr ? 1 : 0;",
            "        buf.write_pod(present);",
            "        if (present) {",
            f"            for (uint32_t i = 0; i < static_cast<uint32_t>({_expr_to_cpp(count, "v.")}); ++i) {{",
            f"                const char* s = v.{m.name}[i];",
            "                if (s == nullptr) {",
            "                    buf.write_varint(0);",
            "                } else {",
            "                    const size_t n = __builtin_strlen(s) + 1u;",
            "                    buf.write_varint(n);",
            "                    buf.write(s, n);",
            "                }",
            "            }",
            "        }",
            "    }",
        ]
    return [
        f"    // TODO: unhandled member shape {kind!r} for {m.name!r} ({m.type.cpp()})",
        f"    (void)v.{m.name};",
    ]


def _refuse_absent(m: StructMember, length: str) -> list[str]:
    if not m.required:
        return []
    return [f"        if (!present && {_expr_to_cpp(length, "out->")} != 0) {{ return false; }}"]


def _zero_absent(m: StructMember, length: str, element: str) -> list[str]:
    if not m.conditional:
        return []
    count = _expr_to_cpp(length, "out->")
    return [
        f"        if (out->{m.name} == nullptr && {count} != 0) {{",
        f"            auto* zeroes = arena.allocate<{element}>(static_cast<size_t>({count}));",
        f"            for (uint32_t i = 0; i < static_cast<uint32_t>({count}); ++i) {{ zeroes[i] = {element}{{}}; }}",
        f"            out->{m.name} = zeroes;",
        "        }",
    ]


def _absent_struct(m: StructMember) -> list[str]:
    if m.required:
        return ["            return false;"]
    if m.conditional:
        return [
            f"            auto* zeroes = arena.allocate<{m.type.base}>();",
            f"            *zeroes = {m.type.base}{{}};",
            f"            out->{m.name} = zeroes;",
        ]
    return [f"            out->{m.name} = nullptr;"]


def _deserialize_member(reg: Registry, m: StructMember) -> list[str]:
    kind = _classify_member(reg, m)
    if kind == "pnext":
        return [
            "    if (!deserialize_pnext_chain(r, handles, "
            "reinterpret_cast<VkBaseOutStructure*>(out), arena)) { return false; }",
        ]
    if kind == "fixed_array":
        return [
            f"    if (!r.read(&out->{m.name}[0], sizeof(out->{m.name}))) {{ return false; }}"
        ]
    if kind == "nested_struct":
        return [
            f"    if (!{deserialize_call(reg, m.type.base, 'r', f'&out->{m.name}', 'arena')}) {{ return false; }}"
        ]
    if kind == "nested_handle":
        refuse = (
            [f"    if (out->{m.name} == VK_NULL_HANDLE) {{ return false; }}"]
            if m.handle_required and not reg.handles[m.type.base].dispatchable
            else []
        )
        return [
            f"    if (!{deserialize_call(reg, m.type.base, 'r', f'out->{m.name}', 'arena')}) {{ return false; }}",
            *refuse,
        ]
    if kind == "pod":
        if _is_size_t(m):
            return [
                "    {",
                "        uint64_t av_wide = 0;",
                "        if (!r.read_pod(av_wide)) { return false; }",
                f"        out->{m.name} = static_cast<decltype(out->{m.name})>(av_wide);",
                "    }",
            ]
        return [f"    if (!r.read_pod(out->{m.name})) {{ return false; }}"]
    if kind == "string":
        absent = (
            [
                "            char* empty = arena.allocate_chars(1u);",
                "            empty[0] = '\\0';",
                f"            out->{m.name} = empty;",
            ]
            if m.required
            else [f"            out->{m.name} = nullptr;"]
        )
        return [
            "    {",
            "        uint64_t n = 0;",
            "        if (!r.read_varint(n)) { return false; }",
            "        if (n == 0) {",
            *absent,
            "        } else {",
            "            // One byte more than the wire said, and a zero in it: the",
            "            // length is the guest's and a host API will strcmp what it",
            "            // is handed. A fuzzer found this in vkCreateInstance",
            "            // (feature 042).",
            "            char* s = arena.allocate_chars(static_cast<size_t>(n) + 1u);",
            "            if (!r.read(s, static_cast<size_t>(n))) { return false; }",
            "            s[n] = '\\0';",
            f"            out->{m.name} = s;",
            "        }",
            "    }",
        ]
    if kind == "optional_struct_ptr":
        return [
            "    {",
            "        uint8_t present = 0;",
            "        if (!r.read_pod(present)) { return false; }",
            "        if (present == 0) {",
            *_absent_struct(m),
            "        } else {",
            f"            auto* p = arena.allocate<{m.type.base}>();",
            f"            if (!{deserialize_call(reg, m.type.base, 'r', 'p', 'arena')}) {{ return false; }}",
            f"            out->{m.name} = p;",
            "        }",
            "    }",
        ]
    if kind == "struct_array":
        count = _len_components(m)[0]
        return [
            "    {",
            "        uint8_t present = 0;",
            "        if (!r.read_pod(present)) { return false; }",
            *_refuse_absent(m, count),
            f"        if (!present || {_expr_to_cpp(count, "out->")} == 0) {{",
            f"            out->{m.name} = nullptr;",
            "        } else {",
            f"            auto* arr = arena.allocate<{m.type.base}>(static_cast<size_t>({_expr_to_cpp(count, "out->")}));",
            f"            for (uint32_t i = 0; i < static_cast<uint32_t>({_expr_to_cpp(count, "out->")}); ++i) {{",
            f"                if (!{deserialize_call(reg, m.type.base, 'r', '&arr[i]', 'arena')}) {{ return false; }}",
            "            }",
            f"            out->{m.name} = arr;",
            "        }",
            *_zero_absent(m, count, m.type.base),
            "    }",
        ]
    if kind == "template_blob":
        template = _template_sibling(reg, m)
        return [
            "    {",
            "        uint64_t av_blob = 0;",
            "        if (!r.read_varint(av_blob)) { return false; }",
            f"        if (av_blob != handles.template_bytes(out->{template})) {{ return false; }}",
            "        if (av_blob == 0) {",
            f"            out->{m.name} = nullptr;",
            "        } else {",
            "            if (av_blob > r.remaining()) { return false; }",
            "            char* blob = arena.allocate_chars(static_cast<size_t>(av_blob));",
            "            if (blob == nullptr || !r.read(blob, static_cast<size_t>(av_blob))) { return false; }",
            f"            out->{m.name} = blob;",
            "        }",
            "    }",
        ]
    if kind == "void_blob":
        size = _len_components(m)[0]
        return [
            "    {",
            "        uint8_t present = 0;",
            "        if (!r.read_pod(present)) { return false; }",
            *_refuse_absent(m, size),
            f"        if (!present || {_expr_to_cpp(size, "out->")} == 0) {{",
            f"            out->{m.name} = nullptr;",
            "        } else {",
            f"            char* blob = arena.allocate_chars(static_cast<size_t>({_expr_to_cpp(size, "out->")}));",
            f"            if (!r.read(blob, static_cast<size_t>({_expr_to_cpp(size, "out->")}))) {{ return false; }}",
            f"            out->{m.name} = blob;",
            "        }",
            *_zero_absent(m, size, "char"),
            "    }",
        ]
    if kind == "pod_array":
        count = _len_components(m)[0]
        elem = m.type.base
        return [
            "    {",
            "        uint8_t present = 0;",
            "        if (!r.read_pod(present)) { return false; }",
            *_refuse_absent(m, count),
            f"        if (!present || {_expr_to_cpp(count, "out->")} == 0) {{",
            f"            out->{m.name} = nullptr;",
            "        } else {",
            f"            auto* arr = arena.allocate<{elem}>(static_cast<size_t>({_expr_to_cpp(count, "out->")}));",
            f"            for (uint32_t i = 0; i < static_cast<uint32_t>({_expr_to_cpp(count, "out->")}); ++i) {{",
            "                if (!r.read_pod(arr[i])) { return false; }",
            "            }",
            f"            out->{m.name} = arr;",
            "        }",
            *_zero_absent(m, count, elem),
            "    }",
        ]
    if kind == "handle_array":
        count = _len_components(m)[0]
        elem = m.type.base
        return [
            "    {",
            "        uint8_t present = 0;",
            "        if (!r.read_pod(present)) { return false; }",
            *_refuse_absent(m, count),
            f"        if (!present || {_expr_to_cpp(count, "out->")} == 0) {{",
            f"            out->{m.name} = nullptr;",
            "        } else {",
            f"            auto* arr = arena.allocate<{elem}>(static_cast<size_t>({_expr_to_cpp(count, "out->")}));",
            f"            for (uint32_t i = 0; i < static_cast<uint32_t>({_expr_to_cpp(count, "out->")}); ++i) {{",
            f"                if (!{deserialize_call(reg, elem, 'r', 'arr[i]', 'arena')}) {{ return false; }}",
            "            }",
            f"            out->{m.name} = arr;",
            "        }",
            *_zero_absent(m, count, elem),
            "    }",
        ]
    if kind == "string_array":
        count = _len_components(m)[0]
        return [
            "    {",
            "        uint8_t present = 0;",
            "        if (!r.read_pod(present)) { return false; }",
            *_refuse_absent(m, count),
            f"        if (!present || {_expr_to_cpp(count, "out->")} == 0) {{",
            f"            out->{m.name} = nullptr;",
            "        } else {",
            f"            auto** arr = arena.allocate<const char*>(static_cast<size_t>({_expr_to_cpp(count, "out->")}));",
            f"            for (uint32_t i = 0; i < static_cast<uint32_t>({_expr_to_cpp(count, "out->")}); ++i) {{",
            "                uint64_t n = 0;",
            "                if (!r.read_varint(n)) { return false; }",
            "                if (n == 0) {",
            "                    // An empty name, not a null one: an element of a",
            "                    // string array is a string the API will read, and a",
            "                    // null in it is a crash rather than an error. Found",
            "                    // by the fuzz target in vkCreateInstance's extension",
            "                    // list (feature 042).",
            "                    char* empty = arena.allocate_chars(1u);",
            "                    if (empty == nullptr) { return false; }",
            "                    empty[0] = '\\0';",
            "                    arr[i] = empty;",
            "                } else {",
            "                    char* s = arena.allocate_chars(static_cast<size_t>(n) + 1u);",
            "                    if (!r.read(s, static_cast<size_t>(n))) { return false; }",
            "                    s[n] = '\\0';",
            "                    arr[i] = s;",
            "                }",
            "            }",
            f"            out->{m.name} = arr;",
            "        }",
            "    }",
        ]
    return [
        f"    // TODO: unhandled member shape {kind!r} for {m.name!r} ({m.type.cpp()})",
        f"    (void)out->{m.name};",
    ]


def _chain_nodes(reg: Registry) -> list[Struct]:
    out: list[Struct] = []
    for name in reg.struct_order:
        s = reg.structs[name]
        if not s.extends or s.is_union:
            continue
        if not s.members or s.members[0].name != "sType":
            continue
        stype = next((v for v in s.members[0].values if v.startswith("VK_STRUCTURE_TYPE")), None)
        if stype is None:
            continue
        if any(_classify_member(reg, m) == "unknown_ptr" for m in s.members):
            continue
        if reg.declared_names and stype not in reg.declared_names:
            continue
        out.append(s)
    return out


def _stype_of(s: Struct) -> str:
    return next(v for v in s.members[0].values if v.startswith("VK_STRUCTURE_TYPE"))


def _emit_pnext_codec(reg: Registry) -> list[str]:
    nodes = _chain_nodes(reg)
    lines: list[str] = [
        "// ---- pNext chain codec ----------------------------------------------",
        "",
        f"// {len(nodes)} chain-extension structs are encodable in this build.",
        "inline bool pnext_is_encodable(VkStructureType t) {",
        "    switch (t) {",
    ]
    for s in nodes:
        lines.append(f"    case {_stype_of(s)}:")
    lines.extend([
        "        return true;",
        "    default:",
        "        return false;",
        "    }",
        "}",
        "",
        "inline void serialize_pnext_chain(::av::WireWriter& buf, HandleTranslation& handles, const void* pNext) {",
        "    const auto* p = static_cast<const VkBaseInStructure*>(pNext);",
        "    while (p != nullptr && !pnext_is_encodable(p->sType)) { p = p->pNext; }",
        "    if (p == nullptr) { buf.write_varint(0); return; }",
        "    buf.write_varint(1);",
        "    buf.write_pod(static_cast<uint32_t>(p->sType));",
        "    switch (p->sType) {",
    ])
    for s in nodes:
        lines.append(f"    case {_stype_of(s)}:")
        lines.append(
            f"        {serialize_call(reg, s.name, 'buf', f'*reinterpret_cast<const {s.name}*>(p)')};"
        )
        lines.append("        break;")
    lines.extend([
        "    default: break;",
        "    }",
        "}",
        "",
        "[[nodiscard]] inline bool deserialize_pnext_chain("
        "::av::WireReader& r, const HandleTranslation& handles, VkBaseOutStructure* out, "
        "::av::WireArena& arena) {",
        "    uint64_t present = 0;",
        "    if (!r.read_varint(present)) { return false; }",
        "    // Nothing on the wire: leave the caller's chain exactly as it is.",
        "    if (present == 0) { return true; }",
        "    uint32_t raw = 0;",
        "    if (!r.read_pod(raw)) { return false; }",
        "    // Switched on as the number it is. Loading an attacker's u32 as a",
        "    // VkStructureType is undefined for every value the enum does not",
        "    // have, and a fuzzer found one (feature 042).",
        "    VkBaseOutStructure* dst = nullptr;",
        "    for (VkBaseOutStructure* n = out->pNext; n != nullptr; n = n->pNext) {",
        "        if (static_cast<uint32_t>(n->sType) == raw) { dst = n; break; }",
        "    }",
        "    // Only adopt a freshly allocated node when the caller had no",
        "    // chain at all (the server side). Splicing one into an",
        "    // application's chain would hand it a struct it never asked",
        "    // for and never frees.",
        "    const bool adopt = (dst == nullptr && out->pNext == nullptr);",
        "    switch (raw) {",
    ])
    for s in nodes:
        lines.extend([
            f"    case static_cast<uint32_t>({_stype_of(s)}): {{",
            f"        auto* typed = (dst != nullptr) ? reinterpret_cast<{s.name}*>(dst)",
            f"                                       : arena.allocate<{s.name}>();",
            "        if (typed == nullptr) { return false; }",
            f"        if (!{deserialize_call(reg, s.name, 'r', 'typed', 'arena')}) {{ return false; }}",
            "        if (adopt) { out->pNext = reinterpret_cast<VkBaseOutStructure*>(typed); }",
            "        break;",
            "    }",
        ])
    lines.extend([
        "    default:",
        "        // Encoder and decoder disagree on what is encodable, which",
        "        // means the stream is no longer parseable from here.",
        "        return false;",
        "    }",
        "    return true;",
        "}",
        "",
    ])
    return lines


BESPOKE_STRUCT_BODIES: dict[str, tuple[list[str], list[str]]] = {
    "VkCopyMemoryToImageInfo": (
        [
            "    buf.write_pod(v.sType);",
            "    serialize_pnext_chain(buf, handles, v.pNext);",
            "    buf.write_pod(v.flags);",
            "    serialize_VkImage(buf, handles, v.dstImage);",
            "    buf.write_pod(v.dstImageLayout);",
            "    buf.write_pod(v.regionCount);",
            "    for (uint32_t i = 0; i < v.regionCount; ++i) {",
            "        serialize_VkMemoryToImageCopy(buf, handles, v.pRegions[i]);",
            "        buf.write_varint(0);",
            "    }",
        ],
        [
            "    if (!r.read_pod(out->sType)) { return false; }",
            "    if (!deserialize_pnext_chain(r, handles, "
            "reinterpret_cast<VkBaseOutStructure*>(out), arena)) { return false; }",
            "    if (!r.read_pod(out->flags)) { return false; }",
            "    if (!deserialize_VkImage(r, handles, out->dstImage)) { return false; }",
            "    if (!r.read_pod(out->dstImageLayout)) { return false; }",
            "    if (!r.read_pod(out->regionCount)) { return false; }",
            "    auto* regions = arena.allocate<VkMemoryToImageCopy>(out->regionCount);",
            "    for (uint32_t i = 0; i < out->regionCount; ++i) {",
            "        if (!deserialize_VkMemoryToImageCopy(r, handles, &regions[i], arena)) { return false; }",
            "        uint64_t n = 0;",
            "        if (!r.read_varint(n)) { return false; }",
            "        if (n == 0) { regions[i].pHostPointer = nullptr; continue; }",
            "        char* blob = arena.allocate_chars(static_cast<size_t>(n));",
            "        if (!r.read(blob, static_cast<size_t>(n))) { return false; }",
            "        regions[i].pHostPointer = blob;",
            "    }",
            "    out->pRegions = regions;",
            "    return true;",
        ],
    ),
    "VkDescriptorGetInfoEXT": (
        [
            "    buf.write_pod(v.sType);",
            "    serialize_pnext_chain(buf, handles, v.pNext);",
            "    buf.write_pod(v.type);",
            "    switch (v.type) {",
            "    case VK_DESCRIPTOR_TYPE_SAMPLER:",
            "        serialize_optional_sampler(buf, handles, v.data.pSampler);",
            "        break;",
            "    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:",
            "        serialize_optional_image_info(buf, handles, v.data.pCombinedImageSampler);",
            "        break;",
            "    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:",
            "        serialize_optional_image_info(buf, handles, v.data.pSampledImage);",
            "        break;",
            "    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:",
            "        serialize_optional_image_info(buf, handles, v.data.pStorageImage);",
            "        break;",
            "    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:",
            "        serialize_optional_image_info(buf, handles, v.data.pInputAttachmentImage);",
            "        break;",
            "    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:",
            "        serialize_optional_addr_info(buf, handles, v.data.pUniformTexelBuffer);",
            "        break;",
            "    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:",
            "        serialize_optional_addr_info(buf, handles, v.data.pStorageTexelBuffer);",
            "        break;",
            "    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:",
            "        serialize_optional_addr_info(buf, handles, v.data.pUniformBuffer);",
            "        break;",
            "    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:",
            "        serialize_optional_addr_info(buf, handles, v.data.pStorageBuffer);",
            "        break;",
            "    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:",
            "        buf.write_pod(v.data.accelerationStructure);",
            "        break;",
            "    default:",
            "        // No arm to encode; the descriptor type carries its",
            "        // own data elsewhere or is not one we support.",
            "        break;",
            "    }",
        ],
        [
            "    if (!r.read_pod(out->sType)) { return false; }",
            "    if (!deserialize_pnext_chain(r, handles, "
            "reinterpret_cast<VkBaseOutStructure*>(out), arena)) { return false; }",
            "    if (!r.read_pod(out->type)) { return false; }",
            "    switch (out->type) {",
            "    case VK_DESCRIPTOR_TYPE_SAMPLER:",
            "        return deserialize_optional_sampler(r, handles, &out->data.pSampler, arena);",
            "    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:",
            "        return deserialize_optional_image_info(r, handles, &out->data.pCombinedImageSampler, arena);",
            "    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:",
            "        return deserialize_optional_image_info(r, handles, &out->data.pSampledImage, arena);",
            "    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:",
            "        return deserialize_optional_image_info(r, handles, &out->data.pStorageImage, arena);",
            "    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:",
            "        return deserialize_optional_image_info(r, handles, &out->data.pInputAttachmentImage, arena);",
            "    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:",
            "        return deserialize_optional_addr_info(r, handles, &out->data.pUniformTexelBuffer, arena);",
            "    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:",
            "        return deserialize_optional_addr_info(r, handles, &out->data.pStorageTexelBuffer, arena);",
            "    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:",
            "        return deserialize_optional_addr_info(r, handles, &out->data.pUniformBuffer, arena);",
            "    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:",
            "        return deserialize_optional_addr_info(r, handles, &out->data.pStorageBuffer, arena);",
            "    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:",
            "        return r.read_pod(out->data.accelerationStructure);",
            "    default:",
            "        return true;",
            "    }",
        ],
    ),
}


def _is_address_union(s: Struct) -> bool:
   return s.is_union and {m.name for m in s.members} == {"deviceAddress", "hostAddress"}


def _serializer_head(reg: Registry, name: str) -> str:
    handles = "HandleTranslation& handles, " if name in carriers(reg) else ""
    return f"inline void serialize_{name}(::av::WireWriter& buf, {handles}const {name}& v) {{"


def _deserializer_head(reg: Registry, name: str) -> str:
    handles = "const HandleTranslation& handles, " if name in carriers(reg) else ""
    return ("[[nodiscard]] inline bool "
            f"deserialize_{name}(::av::WireReader& r, {handles}{name}* out, ::av::WireArena& arena) {{")


def _unread_translation(reg: Registry, name: str, body: list[str]) -> list[str]:
    if name in carriers(reg) and not any("handles" in line for line in body):
        return ["    (void)handles;"]
    return []


def _emit_struct_helpers(reg: Registry, s: Struct, outputs: set[str]) -> list[str]:
    bespoke = BESPOKE_STRUCT_BODIES.get(s.name)
    if bespoke is not None:
        ser_body, de_body = bespoke
        return [
            _serializer_head(reg, s.name),
            *ser_body,
            "}",
            "",
            _deserializer_head(reg, s.name),
            *de_body,
            "}",
            "",
        ]
    if _is_address_union(s):
        return [
            _serializer_head(reg, s.name),
            "    buf.write_pod(v.deviceAddress);",
            "}",
            "",
            _deserializer_head(reg, s.name),
            "    (void)arena;",
            "    return r.read_pod(out->deviceAddress);",
            "}",
            "",
        ]
    if s.is_union:
        return [
            _serializer_head(reg, s.name),
            "    buf.write(&v, sizeof(v));",
            "}",
            "",
            _deserializer_head(reg, s.name),
            "    (void)arena;",
            "    return r.read(out, sizeof(*out));",
            "}",
            "",
        ]
    out: list[str] = []
    out.append(_serializer_head(reg, s.name))
    encoded = [line for m in s.members for line in _serialize_member(reg, m)]
    out.extend(_unread_translation(reg, s.name, encoded))
    out.extend(encoded)
    out.append("}")
    out.append("")
    out.append(_deserializer_head(reg, s.name))
    out.append("    (void)arena;")
    decoded: list[str] = []
    arrays = {m.name: count for m, count in caller_arrays(reg, s)} if s.name in outputs else {}
    counts = {count.name for count in arrays.values()}
    bounds = _fixed_array_bounds(s)
    for m in s.members:
        if m.name in counts:
            decoded.append(f"    decltype(out->{m.name}) {m.name}_answered{{}};")
            decoded.append(f"    if (!r.read_pod({m.name}_answered)) {{ return false; }}")
        elif m.name in arrays:
            decoded.extend(_deserialize_caller_array(reg, m, arrays[m.name]))
        elif m.name in bounds:
            decoded.extend(_deserialize_bounded_count(m, bounds[m.name]))
        else:
            decoded.extend(_deserialize_member(reg, m))
        if m.type.is_fixed_array and m.type.base == "char" and _len_components(m) == ["null-terminated"]:
            decoded.append(f"    out->{m.name}[sizeof(out->{m.name}) - 1u] = '\\0';")
    out.extend(_unread_translation(reg, s.name, decoded))
    out.extend(decoded)
    out.append("    return true;")
    out.append("}")
    out.append("")
    return out


def _translation_declarations(reg: Registry) -> list[str]:
    lines = [
        "// The kinds of handle that cross the wire: one per handle type the",
        "// profile carries.",
        "enum class HandleKind : uint8_t {",
    ]
    lines += [f"    {handle_kind(h)}," for h in _referenced_handle_names(reg)]
    lines += [
        "};",
        "",
        "// What a handle is on the other side of the wire, and the one length a",
        "// call leaves for its own side to know. Every encoder and decoder below",
        "// that can meet a handle is handed one: on the host it is the",
        "// ServerDispatch the frame is for, which answers only for the handles it",
        "// made and refuses every other number a guest names (feature 042); in the",
        "// guest ICD it unwraps the handles the loader dispatches on into the",
        "// host's own; HandleIdentity is a caller's whose handles are already the",
        "// host's numbers. The same shape as OpenCL's ServerDispatch::handle_for.",
        "class HandleTranslation {",
        "public:",
        "    // The number `handle`, a `kind`, crosses the wire as.",
        "    [[nodiscard]] virtual uint64_t wire_for(HandleKind kind, uint64_t handle) = 0;",
        "    // The `kind` the number `wire` names, into `handle`: false for a",
        "    // number that names none, and the frame is refused. Null is a value",
        "    // Vulkan gives most handles and never a dispatchable one, so it is",
        "    // judged here too rather than passed.",
        "    [[nodiscard]] virtual bool handle_for(HandleKind kind, uint64_t wire, uint64_t& handle) const = 0;",
        "    // How many bytes one update through `made` reads. The length of a",
        "    // descriptor blob is the template's, from the entries it was created",
        "    // with, and nothing in the call carries it: each side answers from its",
        "    // own table of templates, 0 for one it never made (feature 047).",
        "    [[nodiscard]] virtual uint64_t template_bytes(VkDescriptorUpdateTemplate made) const = 0;",
        "",
        "protected:",
        "    HandleTranslation() = default;",
        "    ~HandleTranslation() = default;",
        "};",
        "",
        "// The numbers are the caller's own, and it made no templates: the",
        "// guest's test programs, which speak to the executor in the host's",
        "// handles directly.",
        "class HandleIdentity final : public HandleTranslation {",
        "public:",
        "    [[nodiscard]] uint64_t wire_for(HandleKind kind, uint64_t handle) override;",
        "    [[nodiscard]] bool handle_for(HandleKind kind, uint64_t wire, uint64_t& handle) const override;",
        "    [[nodiscard]] uint64_t template_bytes(VkDescriptorUpdateTemplate made) const override;",
        "};",
        "",
    ]
    return lines


def _optional_helpers(reg: Registry) -> list[str]:
    lines = [
        "// Optional pointer helpers (used by hand-written struct bodies).",
        "inline void serialize_optional_sampler(::av::WireWriter& buf, HandleTranslation& handles, "
        "const VkSampler* p) {",
        "    const uint8_t present = p != nullptr ? 1 : 0;",
        "    buf.write_pod(present);",
        "    if (present) { serialize_VkSampler(buf, handles, *p); }",
        "}",
        "",
        "[[nodiscard]] inline bool deserialize_optional_sampler("
        "::av::WireReader& r, const HandleTranslation& handles, const VkSampler** out, "
        "::av::WireArena& arena) {",
        "    uint8_t present = 0;",
        "    if (!r.read_pod(present)) { return false; }",
        "    if (!present) { *out = nullptr; return true; }",
        "    auto* v = arena.allocate<VkSampler>();",
        "    if (!deserialize_VkSampler(r, handles, *v)) { return false; }",
        "    *out = v;",
        "    return true;",
        "}",
        "",
        "inline void serialize_optional_image_info(::av::WireWriter& buf, HandleTranslation& handles, "
        "const VkDescriptorImageInfo* p) {",
        "    const uint8_t present = p != nullptr ? 1 : 0;",
        "    buf.write_pod(present);",
        f"    if (present) {{ {serialize_call(reg, 'VkDescriptorImageInfo', 'buf', '*p')}; }}",
        "}",
        "",
        "[[nodiscard]] inline bool deserialize_optional_image_info("
        "::av::WireReader& r, const HandleTranslation& handles, const VkDescriptorImageInfo** out, "
        "::av::WireArena& arena) {",
        "    uint8_t present = 0;",
        "    if (!r.read_pod(present)) { return false; }",
        "    if (!present) { *out = nullptr; return true; }",
        "    auto* v = arena.allocate<VkDescriptorImageInfo>();",
        f"    if (!{deserialize_call(reg, 'VkDescriptorImageInfo', 'r', 'v', 'arena')}) {{ return false; }}",
        "    *out = v;",
        "    return true;",
        "}",
        "",
    ]
    if "VkDescriptorAddressInfoEXT" in reg.structs:
        lines += [
            "inline void serialize_optional_addr_info(::av::WireWriter& buf, HandleTranslation& handles, "
            "const VkDescriptorAddressInfoEXT* p) {",
            "    const uint8_t present = p != nullptr ? 1 : 0;",
            "    buf.write_pod(present);",
            f"    if (present) {{ {serialize_call(reg, 'VkDescriptorAddressInfoEXT', 'buf', '*p')}; }}",
            "}",
            "",
            "[[nodiscard]] inline bool deserialize_optional_addr_info("
            "::av::WireReader& r, const HandleTranslation& handles, const VkDescriptorAddressInfoEXT** out, "
            "::av::WireArena& arena) {",
            "    uint8_t present = 0;",
            "    if (!r.read_pod(present)) { return false; }",
            "    if (!present) { *out = nullptr; return true; }",
            "    auto* v = arena.allocate<VkDescriptorAddressInfoEXT>();",
            f"    if (!{deserialize_call(reg, 'VkDescriptorAddressInfoEXT', 'r', 'v', 'arena')}) {{ return false; }}",
            "    *out = v;",
            "    return true;",
            "}",
            "",
        ]
    return lines


def _emit_identity(reg: Registry) -> str:
    return "\n".join([
        GENERATED_BANNER,
        "",
        '#include "vk_handle_translation.h"',
        "",
        "#include <stdint.h>",
        "",
        "namespace av::vkproto {",
        "",
        "uint64_t HandleIdentity::wire_for(HandleKind, uint64_t handle) {",
        "    return handle;",
        "}",
        "",
        "bool HandleIdentity::handle_for(HandleKind, uint64_t wire, uint64_t& handle) const {",
        "    handle = wire;",
        "    return true;",
        "}",
        "",
        "uint64_t HandleIdentity::template_bytes(VkDescriptorUpdateTemplate) const {",
        "    return 0;",
        "}",
        "",
        "}  // namespace av::vkproto",
        "",
    ])


def emit(reg: Registry, out_dir: Path) -> None:
    header = [
        GENERATED_BANNER,
        "",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        "#include <vulkan/vulkan_core.h>",
        "",
        '#include "proto/wire/wire_arena.hpp"',
        '#include "proto/wire/wire_writer.hpp"',
        '#include "proto/wire/wire_reader.hpp"',
        '#include "vk_handle_translation.h"',
        "",
        "namespace av::vkproto {",
        "",
        "// All Vulkan handles cross the wire as 64-bit values. See docs/protocol.md.",
        "// That is the wire's width, not the host's: a 32-bit guest is a real",
        "// case — DXVK translating an old Direct3D game is a 32-bit process,",
        "// and everything it loads must be too — and its dispatchable handles",
        "// are 4-byte pointers. They are widened on the way out and narrowed",
        "// on the way back, so both ends see the same bytes either way.",
        'static_assert(sizeof(void*) == 8 || sizeof(void*) == 4,',
        '              "AV assumes 32- or 64-bit pointers");',
        "",
        "// Any field, as the number it holds. A value off the wire is a value",
        "// its own type may not have - an enum with an unlisted value, most of",
        "// all - and loading one as that type is undefined. Everything that",
        "// reads a decoded field for its value rather than to pass it on goes",
        "// through here (feature 042).",
        "template <typename T>",
        "[[nodiscard]] inline uint64_t as_number(const T& value) noexcept {",
        "    uint64_t raw = 0;",
        "    static_assert(sizeof(T) <= sizeof(raw));",
        "    __builtin_memcpy(&raw, &value, sizeof(T));",
        "    return raw;",
        "}",
        "",
        "// The other way: a number as a handle of type H, in whichever shape the",
        "// build gives H - a pointer, or on x86 a 4-byte pointer for a",
        "// dispatchable handle and a NonDispatchableHandle for the rest.",
        "template <typename H>",
        "[[nodiscard]] inline H as_handle(uint64_t raw) noexcept {",
        "    H handle{};",
        "    static_assert(sizeof(H) <= sizeof(raw));",
        "    __builtin_memcpy(&handle, &raw, sizeof(H));",
        "    return handle;",
        "}",
        "",
    ]

    for h in _referenced_handle_names(reg):
        header.extend(_emit_handle_helpers(h))

    if reg.struct_order:
        header.append("// Forward declarations.")
        header.append(
            "inline void serialize_pnext_chain(::av::WireWriter&, HandleTranslation&, const void*);"
        )
        header.append(
            "[[nodiscard]] inline bool deserialize_pnext_chain("
            "::av::WireReader&, const HandleTranslation&, VkBaseOutStructure*, "
            "::av::WireArena&);"
        )
        for n in reg.struct_order:
            carried = n in carriers(reg)
            header.append(
                f"inline void serialize_{n}(::av::WireWriter&, "
                f"{'HandleTranslation&, ' if carried else ''}const {n}&);"
            )
            header.append(
                "[[nodiscard]] inline bool "
                f"deserialize_{n}(::av::WireReader&, "
                f"{'const HandleTranslation&, ' if carried else ''}{n}*, ::av::WireArena&);"
            )
        header.append("")
        header.extend(_optional_helpers(reg))

    outputs = _output_structs(reg)
    for n in reg.struct_order:
        header.extend(_emit_struct_helpers(reg, reg.structs[n], outputs))

    if reg.struct_order:
        header.extend(_emit_pnext_codec(reg))

    header.append("}  // namespace av::vkproto")
    header.append("")

    write(out_dir / "vk_serialize.h", header_guard("\n".join(header[1:])))
    translation = [
        "",
        "#include <stdint.h>",
        "",
        "#include <vulkan/vulkan_core.h>",
        "",
        "namespace av::vkproto {",
        "",
        *_translation_declarations(reg),
        "}  // namespace av::vkproto",
        "",
    ]
    write(out_dir / "vk_handle_translation.h", header_guard("\n".join(translation)))
    write(out_dir / "vk_handle_identity.cpp", _emit_identity(reg))
