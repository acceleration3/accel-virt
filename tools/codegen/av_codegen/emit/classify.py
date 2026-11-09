from __future__ import annotations

import re


DISPATCHABLE_HANDLES = frozenset(
    {"VkInstance", "VkPhysicalDevice", "VkDevice", "VkQueue", "VkCommandBuffer"}
)


MACHINE_WIDTH_TYPES = frozenset({
    "size_t", "size_t", "ptrdiff_t", "intptr_t", "uintptr_t",
    "long", "unsigned long", "signed long",
})


def is_size_t(type_) -> bool:
    return type_.base in ("size_t", "size_t") and type_.pointer_depth == 0


def is_machine_width(type_) -> bool:
    return type_.base in MACHINE_WIDTH_TYPES and type_.pointer_depth == 0

from ..ir import Command, CType, Param, Registry


def param_cpp(t: CType) -> str:
    s = t.base
    eff_ptr = t.pointer_depth + (1 if t.fixed_dim is not None else 0)
    if eff_ptr:
        s += "*" * eff_ptr
    if t.is_const:
        s = "const " + s
    return s


def is_alloc_callbacks(p: Param) -> bool:
    return p.type.base == "VkAllocationCallbacks"


def is_count_array_pair(params: list[Param], i: int) -> bool:
    if i + 1 >= len(params):
        return False
    a, b = params[i], params[i + 1]
    if a.type.base not in ("uint32_t", "size_t"):
        return False
    if a.type.pointer_depth != 1 or b.type.pointer_depth != 1:
        return False
    return b.len_expr == a.name


def stype_value_for_struct(reg: Registry, type_base: str) -> str | None:
    s = reg.structs.get(type_base)
    if s is None or not s.has_stype:
        return None
    stype_member = s.members[0]
    if stype_member.values:
        return stype_member.values[0]
    return None


def len_is_sibling(params: list[Param], len_expr: str | None) -> bool:
    if not len_expr:
        return False
    for q in params:
        if q.name == len_expr and q.type.pointer_depth == 0:
            return True
    if "->" in len_expr:
        head = len_expr.split("->", 1)[0].strip()
        for q in params:
            if q.name == head and q.type.pointer_depth == 1:
                return True
    return _len_is_sibling_expression(params, len_expr)


_LEN_EXPR_SAFE = re.compile(r"^[A-Za-z_0-9+\-*/() ]+$")
_LEN_EXPR_NAME = re.compile(r"[A-Za-z_][A-Za-z_0-9]*")


def _len_is_sibling_expression(params: list[Param], len_expr: str) -> bool:
    if not _LEN_EXPR_SAFE.match(len_expr):
        return False
    names = set(_LEN_EXPR_NAME.findall(len_expr))
    if not names:
        return False
    by_value = {q.name for q in params if q.type.pointer_depth == 0}
    return names <= by_value


def server_len_expr(params: list[Param], len_expr: str | None) -> str:
    if not len_expr:
        return ""
    if "->" not in len_expr:
        return len_expr
    head, _, rest = len_expr.partition("->")
    head = head.strip()
    rest = rest.strip()
    for q in params:
        if q.name == head and q.type.pointer_depth == 1:
            return f"{head}_local.{rest}"
    return len_expr


TEMPLATE_HANDLE = "VkDescriptorUpdateTemplate"


def template_sibling(params: list[Param] | None) -> str | None:
    if params is None:
        return None
    for other in params:
        if other.type.base == TEMPLATE_HANDLE and other.type.pointer_depth == 0:
            return other.name
    return None


def stride_array(params: list[Param] | None, p: Param) -> Param | None:
    if params is None or p.type.pointer_depth != 0:
        return None
    for other in params:
        if other.stride == p.name:
            return other
    return None


def classify(reg: Registry, p: Param, params: list[Param] | None = None) -> str:
    if is_alloc_callbacks(p):
        return "in_null"
    if stride_array(params, p) is not None:
        return "in_stride"
    if p.direction == "in" and p.type.base in reg.handles and p.type.pointer_depth == 0:
        return "in_handle"
    if p.direction == "in" and p.type.fixed_dim is not None and p.type.pointer_depth == 0:
        return "in_fixed_array"
    if p.direction == "in" and p.type.pointer_depth == 0:
        return "in_pod"
    if p.direction == "in" and p.type.base == "char" and p.type.pointer_depth == 1:
        return "in_string"
    if (
        p.direction == "in"
        and p.type.base == "void"
        and p.type.pointer_depth == 1
        and not p.len_expr
        and template_sibling(params) is not None
    ):
        return "in_template_blob"
    if (
        p.direction == "in"
        and p.type.pointer_depth == 1
        and params is not None
        and len_is_sibling(params, p.len_expr)
    ):
        if p.type.base == "void":
            return "in_byte_array"
        if p.type.base in reg.handles:
            return "in_handle_array"
        if p.type.base in reg.structs:
            return "in_struct_array"
        return "in_pod_array"
    if (
        p.direction == "in"
        and p.type.pointer_depth == 2
        and p.inner_len
        and params is not None
        and len_is_sibling(params, p.len_expr)
    ):
        return "in_struct_array2" if p.type.base in reg.structs else "in_pod_array2"
    if p.direction == "in" and p.type.base in reg.structs and p.type.pointer_depth == 1:
        return "in_struct_ptr"
    if (
        p.direction == "in"
        and p.type.pointer_depth == 1
        and not p.len_expr
        and p.type.base not in reg.handles
        and p.type.base not in ("void", "char")
        and bool(p.optional_levels)
        and p.optional_levels[0]
    ):
        return "in_pod_ptr"
    if (
        p.direction == "out"
        and p.type.pointer_depth == 1
        and params is not None
        and len_is_sibling(params, p.len_expr)
    ):
        if p.type.base == "void":
            return "out_byte_array"
        if p.type.base in reg.handles:
            return "out_handle_array"
        if p.type.base in reg.structs:
            return "out_struct_array"
        return "out_pod_array"
    if p.direction == "out" and p.type.base in reg.handles and p.type.pointer_depth == 1:
        return "out_handle"
    if p.direction == "out" and p.type.base in reg.structs and p.type.pointer_depth == 1:
        return "out_struct"
    if p.direction == "out" and p.type.pointer_depth == 1:
        return "out_pod"
    raise ValueError(
        f"unhandled parameter shape for codegen: name={p.name!r} type={p.type.cpp()!r}"
    )


def signature(cmd: Command, *, prefix: str) -> str:
    args = ", ".join(f"{param_cpp(p.type)} {p.name}" for p in cmd.params)
    return f"{prefix}({args})"


WIDE_RETURN_TYPES = frozenset({
    "VkDeviceAddress",
    "VkDeviceSize",
    "uint64_t",
})


def returns_wide(cmd) -> bool:
    return cmd.return_type.base in WIDE_RETURN_TYPES and cmd.return_type.pointer_depth == 0


def is_size_blob(count: Param, array: Param) -> bool:
    return count.type.base == "size_t" and array.type.base == "void"
