"""Parameter classification + len-expression utilities.

Both ``client_dispatch`` and ``server_dispatch`` need to bucket each
parameter into one of a fixed set of shapes (in_pod, in_struct_ptr,
out_handle_array, etc.). They also need to evaluate vk.xml ``len=`` /
``altlen=`` expressions in the right context (caller side vs server
side, where in-struct-pointer params have been decoded into local
by-value structs).

Keeping all of that in one file lets future emitters (a tracing emitter,
a validating emitter, an alternate wire format) reuse the same
classification without dragging in client/server-specific code.
"""

from __future__ import annotations

import re


# Handles that are pointers on the local machine and therefore change
# width between a 32- and a 64-bit build. Everything else Vulkan calls a
# handle is already a fixed 64-bit integer.
#
# This matters for storage, not for the wire: the wire is 64 bits wide on
# both, so a client must not hold a server's handle in one of these.
DISPATCHABLE_HANDLES = frozenset(
    {"VkInstance", "VkPhysicalDevice", "VkDevice", "VkQueue", "VkCommandBuffer"}
)


# Types whose size is the machine's rather than the wire's.
#
# This is the shape of every protocol bug this project has had that took
# more than an hour to find, and it has had three: size_t in struct
# members, size_t in command parameters, and dispatchable handles. Each
# one encoded four bytes on a 32-bit guest where the server read eight.
# None of them failed loudly. They shifted every field after themselves,
# so the server rejected the command as undecodable and named the
# command rather than the field — or, worse, decoded it into plausible
# nonsense.
#
# Fixing instances one at a time is what made it take three attempts. So
# the generator now knows the whole set, and refuses to emit a wire
# encoding for any of them that does not say its width explicitly.
MACHINE_WIDTH_TYPES = frozenset({
    "size_t", "size_t", "ptrdiff_t", "intptr_t", "uintptr_t",
    "long", "unsigned long", "signed long",
})


def is_size_t(type_) -> bool:
    return type_.base in ("size_t", "size_t") and type_.pointer_depth == 0


def is_machine_width(type_) -> bool:
    """A scalar whose byte count differs between a 32- and 64-bit build."""
    return type_.base in MACHINE_WIDTH_TYPES and type_.pointer_depth == 0

from ..ir import Command, CType, Param, Registry


# ---- Shared signature helpers ---------------------------------------------


def param_cpp(t: CType) -> str:
    """Render a CType for a function-parameter slot.

    Fixed-array params (``const float blendConstants[4]``) decay to
    pointers in C function signatures, so we emit them as
    ``const float* blendConstants``. This keeps the dispatch signature
    callable from any code that has either a pointer or an array.
    """
    s = t.base
    eff_ptr = t.pointer_depth + (1 if t.fixed_dim is not None else 0)
    if eff_ptr:
        s += "*" * eff_ptr
    if t.is_const:
        s = "const " + s
    return s


# ---- Param shape predicates ----------------------------------------------


def is_alloc_callbacks(p: Param) -> bool:
    return p.type.base == "VkAllocationCallbacks"


def is_count_array_pair(params: list[Param], i: int) -> bool:
    """The two-call enumeration idiom: a ``uint32_t*`` count followed by an
    array param whose ``len=`` references the count."""
    if i + 1 >= len(params):
        return False
    a, b = params[i], params[i + 1]
    if a.type.base not in ("uint32_t", "size_t"):
        return False
    if a.type.pointer_depth != 1 or b.type.pointer_depth != 1:
        return False
    return b.len_expr == a.name


def stype_value_for_struct(reg: Registry, type_base: str) -> str | None:
    """Return the canonical ``sType`` value for an sType-bearing struct.

    Used to pre-fill ``sType`` on freshly-allocated output array elements
    server-side so the real Vulkan driver accepts the call.
    """
    s = reg.structs.get(type_base)
    if s is None or not s.has_stype:
        return None
    stype_member = s.members[0]
    if stype_member.values:
        return stype_member.values[0]
    return None


def len_is_sibling(params: list[Param], len_expr: str | None) -> bool:
    """True if ``len_expr`` resolves to a sibling param in the same command.

    Two patterns count:
      * ``len="X"`` where ``X`` is a by-value sibling -- e.g.
        ``const T* pX`` with ``len="COUNT"`` and a sibling
        ``uint32_t COUNT``.
      * ``len="X->Y"`` where ``X`` is an in-struct-ptr sibling -- e.g.
        ``VkCommandBuffer* pCommandBuffers`` with
        ``len="pAllocateInfo->commandBufferCount"`` where
        ``pAllocateInfo`` is a sibling
        ``const VkCommandBufferAllocateInfo*``.

    Output two-call enumeration (``uint32_t*`` count) is a separate
    pattern and goes through ``is_count_array_pair`` instead.
    """
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


# An ``altlen`` is already a C expression, which is the whole reason
# vk.xml carries one next to the LaTeX ``len``.
_LEN_EXPR_SAFE = re.compile(r"^[A-Za-z_0-9+\-*/() ]+$")
_LEN_EXPR_NAME = re.compile(r"[A-Za-z_][A-Za-z_0-9]*")


def _len_is_sibling_expression(params: list[Param], len_expr: str) -> bool:
    """True for a length written as arithmetic over by-value siblings.

    ``vkCmdSetSampleMaskEXT`` has ``altlen="(samples + 31) / 32"``, and
    before this the generator rejected the command outright — it could
    match a length that WAS a sibling's name and nothing else.

    Rejecting it was not a small loss. An unencodable command means the
    extension defining it is not claimed, and
    ``VK_EXT_extended_dynamic_state3`` is one DXVK enables on sight; a
    D3D9 title without it bakes far more state into pipelines, or does
    not run at all. One arithmetic expression stood between the proxy
    and that extension.

    Accepting it is safe because such an expression is emitted verbatim
    into C++ on both ends, where every name in it is a parameter in
    scope with the same spelling. The character class is what keeps that
    true: identifiers, digits, the four operators, parentheses and
    spaces. No dereference, no call, no comma — nothing whose meaning
    could differ between the two sides or reach outside the argument
    list.
    """
    if not _LEN_EXPR_SAFE.match(len_expr):
        return False
    names = set(_LEN_EXPR_NAME.findall(len_expr))
    if not names:
        return False  # a constant length is a fixed array, handled above
    by_value = {q.name for q in params if q.type.pointer_depth == 0}
    return names <= by_value


def server_len_expr(params: list[Param], len_expr: str | None) -> str:
    """Rewrite a vk.xml ``len`` expression for server-side use.

    Server-side, in-struct-ptr params get decoded into ``X_local`` (a
    by-value struct), so ``X->Y`` must become ``X_local.Y``. Sibling
    by-value names need no rewrite.
    """
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


# ---- Classification --------------------------------------------------------


TEMPLATE_HANDLE = "VkDescriptorUpdateTemplate"
"""The handle whose entries size a descriptor blob (feature 047)."""


def template_sibling(params: list[Param] | None) -> str | None:
    """The name of the ``VkDescriptorUpdateTemplate`` parameter beside ``p``.

    ``pData`` in the three ``WithTemplate`` commands has no ``len`` in vk.xml:
    its length is the template's, computed from the entries the template was
    created with. That is not in the call, so the only thing that can answer it
    is each side's ``HandleTranslation::template_bytes``, over its own table of
    templates.
    """
    if params is None:
        return None
    for other in params:
        if other.type.base == TEMPLATE_HANDLE and other.type.pointer_depth == 0:
            return other.name
    return None


def classify(reg: Registry, p: Param, params: list[Param] | None = None) -> str:
    if is_alloc_callbacks(p):
        return "in_null"
    if p.direction == "in" and p.type.base in reg.handles and p.type.pointer_depth == 0:
        return "in_handle"
    # Fixed-size input array: ``const float blendConstants[4]``.
    if p.direction == "in" and p.type.fixed_dim is not None and p.type.pointer_depth == 0:
        return "in_fixed_array"
    if p.direction == "in" and p.type.pointer_depth == 0:
        return "in_pod"
    if p.direction == "in" and p.type.base == "char" and p.type.pointer_depth == 1:
        return "in_string"
    # A blob whose length is a descriptor update template's, not the call's.
    if (
        p.direction == "in"
        and p.type.base == "void"
        and p.type.pointer_depth == 1
        and not p.len_expr
        and template_sibling(params) is not None
    ):
        return "in_template_blob"
    # Input arrays (count is a sibling by-value param).
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
    # Pointer-to-pointer array with a known inner length (profile
    # inner_lens): an outer array of pointers, each to an array of its
    # own length. vk.xml gives only the outer count.
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
    # Output array with a sibling-by-value count.
    if (
        p.direction == "out"
        and p.type.pointer_depth == 1
        and params is not None
        and len_is_sibling(params, p.len_expr)
    ):
        # ``void*`` first: it is not a POD array — sizeof(void) is not a
        # thing — and the size sibling is already a byte count.
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
    # Bare POD output: ``uint32_t* pImageIndex`` in vkAcquireNextImageKHR,
    # ``VkBool32* pSupported`` in vkGetPhysicalDeviceSurfaceSupportKHR.
    if p.direction == "out" and p.type.pointer_depth == 1:
        return "out_pod"
    raise ValueError(
        f"unhandled parameter shape for codegen: name={p.name!r} type={p.type.cpp()!r}"
    )


def signature(cmd: Command, *, prefix: str) -> str:
    """Render a function-prototype-style signature for ``cmd``.

    ``prefix`` is what comes before the parameter list, e.g.
    ``"VkResult av_vkCreateInstance"``. The caller decides whether to
    include a transport parameter, an ``override`` keyword, etc.
    """
    args = ", ".join(f"{param_cpp(p.type)} {p.name}" for p in cmd.params)
    return f"{prefix}({args})"


# 64-bit scalar return types. Vulkan commands overwhelmingly return
# VkResult, and the wire format encoded every return as int32 on that
# assumption. The address queries break it: a VkDeviceAddress truncated
# to 32 bits is a valid-looking pointer into the wrong place, and the
# first shader to dereference it faults the GPU. The failure surfaces as
# VK_ERROR_DEVICE_LOST from an unrelated later call, so it is worth
# keeping this list explicit rather than inferring "not VkResult".
WIDE_RETURN_TYPES = frozenset({
    "VkDeviceAddress",
    "VkDeviceSize",
    "uint64_t",
})


def returns_wide(cmd) -> bool:
    """Whether ``cmd``'s return value needs 64 bits on the wire."""
    return cmd.return_type.base in WIDE_RETURN_TYPES and cmd.return_type.pointer_depth == 0
