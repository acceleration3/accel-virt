"""Intermediate representation for the codegen pipeline.

vk.xml is large and quirky. We don't want every emitter touching the raw
DOM — `lower.py` boils it down to these dataclasses, and emitters consume
them. The IR is intentionally typed and frozen where possible.
"""

from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class CType:
    """A C-level type reference with optional pointer indirection, const, fixed-array dim."""

    base: str
    pointer_depth: int = 0
    is_const: bool = False
    fixed_dim: int | None = None
    # If fixed_dim came from an API constant (e.g. ``VK_UUID_SIZE``), the
    # original symbol — useful for emitting readable code. ``None`` for
    # numeric literals.
    api_constant: str | None = None
    # ``const T *const *``: the pointers the outer one points at are const
    # too. GL declares its arrays of strings and of index pointers this way,
    # and an entry point written without it is a different function type.
    inner_const: bool = False

    @property
    def is_fixed_array(self) -> bool:
        return self.fixed_dim is not None

    def cpp(self) -> str:
        s = self.base
        if self.pointer_depth and self.inner_const and self.pointer_depth >= 2:
            s += "*const" + "*" * (self.pointer_depth - 1)
        elif self.pointer_depth:
            s += "*" * self.pointer_depth
        if self.is_const:
            s = "const " + s
        return s


@dataclass(frozen=True)
class Handle:
    name: str
    dispatchable: bool = True
    parent: str | None = None


@dataclass
class Param:
    name: str
    type: CType
    # Per-pointer-level optional flags from vk.xml `optional="false,true"`.
    # E.g. for a uint32_t* with optional="false,true": optional_levels = (False, True)
    # meaning the pointer is required (False) but the pointee is optional (True).
    # For a non-pointer with no optional attr: ().
    optional_levels: tuple[bool, ...] = ()
    len_expr: str | None = None
    # Neither ``optional`` nor ``noautovalidity``: a handle passed this way
    # is never the null handle in a valid call, and a decoder refuses a frame
    # naming it, since the driver dereferences a non-dispatchable handle too.
    null_refused: bool = False
    # For a pointer-to-pointer array: "<sibling array>.<member>", giving
    # the length of each inner array. See profile.inner_lens.
    inner_len: str | None = None
    direction: str = "in"  # "in" | "out" | "inout"


@dataclass
class Command:
    name: str
    return_type: CType
    params: list[Param]
    cmd_id: int
    # For a promoted command, the core command this one is an alias of
    # (``vkGetPhysicalDeviceProperties2`` for the ``…2KHR`` spelling).
    # Only the core symbol is exported by the loader, so the server's
    # real dispatch has to call that one — the alias spelling would not
    # link. None when the command is not an alias.
    alias_of: str | None = None
    # True when no core Vulkan feature block introduces this command, so
    # the loader exports no symbol for it and it can only be reached
    # through vkGetInstanceProcAddr at runtime.
    extension_only: bool = False
    sync_mode: str = "sync"  # "sync" | "async"
    success_codes: tuple[str, ...] = ()
    error_codes: tuple[str, ...] = ()


@dataclass
class StructMember:
    name: str
    type: CType
    optional_levels: tuple[bool, ...] = ()
    len_expr: str | None = None
    values: tuple[str, ...] = ()  # from ``values="VK_STRUCTURE_TYPE_..."``
    # A pointer the API requires whenever its length is not zero: not
    # ``optional``, not ``noautovalidity`` (valid null under conditions the
    # registry does not spell out), in a struct the caller fills. A decoder
    # refuses a frame that leaves one out, since the driver dereferences it.
    required: bool = False
    # A handle member the API requires: the same rule as ``required``, for
    # the null handle rather than a null pointer.
    handle_required: bool = False
    # A pointer the registry leaves to conditions (``noautovalidity`` and not
    # ``optional``): valid null when another member makes the driver ignore
    # it. A decoder hands the driver zeroes rather than a null it might read.
    conditional: bool = False


@dataclass
class Struct:
    name: str
    members: list[StructMember]
    returned_only: bool = False
    extends: tuple[str, ...] = ()  # struct names this one can extend via pNext
    # Vulkan distinguishes ``struct`` and ``union`` types via the ``category``
    # attribute in vk.xml. Unions (``VkClearColorValue``, ``VkClearValue``)
    # are encoded by the codegen as raw bytes — every member overlaps the
    # same storage, so memcpy of the whole struct is the simplest correct
    # encoding. The discriminator is context-dependent (e.g. the format of
    # the image being cleared), so we don't try to be clever.
    is_union: bool = False

    @property
    def has_stype(self) -> bool:
        """Whether the struct's first member is the standard ``sType`` discriminator.

        Vulkan v1 structs (e.g. ``VkPhysicalDeviceFeatures``) have no sType;
        v2-and-later structs (e.g. ``VkPhysicalDeviceFeatures2``) do. The
        codegen treats the two differently for ``out_struct`` parameters:
        the sType-bearing form forwards sType + an empty pNext chain on the
        wire, while the non-sType form sends nothing.
        """
        return bool(self.members) and self.members[0].name == "sType"


@dataclass
class Registry:
    """Everything the emitters need."""

    handles: dict[str, Handle] = field(default_factory=dict)
    commands: dict[str, Command] = field(default_factory=dict)
    structs: dict[str, Struct] = field(default_factory=dict)
    # Struct names lowered in topological order (dependencies first), so the
    # emitter can place serializer definitions in a sane order. Only set after
    # ``lower.lower``.
    struct_order: list[str] = field(default_factory=list)
    # Identifiers the target vulkan_core.h actually declares. Empty means
    # no header was supplied and nothing is filtered on it.
    declared_names: set[str] = field(default_factory=set)
    # Extensions this profile actually generated entry points for, as
    # ``name -> "device" | "instance"``.
    #
    # The ICD advertises device extensions by forwarding whatever the
    # host driver reports — every extension that driver has, roughly two
    # hundred of them against the four dozen here. An application is
    # entitled to enable anything it is offered and call its commands,
    # and for an extension that was never generated
    # ``vkGetDeviceProcAddr`` returns null. The application then calls
    # through a null pointer and dies with no Vulkan error anywhere:
    # DXVK crashed at EIP 0 running a D3D8 title exactly this way.
    #
    # So the list has to reach the runtime, and it has to come from the
    # generator rather than be maintained by hand beside it — the entire
    # point is that it cannot drift from what was generated.
    implemented_extensions: dict[str, str] = field(default_factory=dict)
    # Commands the profile excludes because the ICD writes them by hand.
    # Emitted so the ICD can verify that it actually did; see
    # emit/supported_extensions.py.
    manual_commands: set[str] = field(default_factory=set)
