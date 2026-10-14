"""Lower a parsed vk.xml DOM into our IR.

Driven by ``profile.toml``: the command set is computed by walking
``<feature>`` blocks (every core API version up to and including
``profile.api_version``) plus the requested ``<extension>`` blocks
(transitively, via ``depends="..."``). Anything in
``[commands].exclude`` is dropped — those are typically platform-tied
entry points hand-written in ``src/client``.

If a command's parameter list contains shapes the codegen doesn't know
how to serialize yet (function pointers, untagged unions, certain
external-handle types), it's logged and skipped instead of failing the
whole build. The summary at the end tells you what got dropped.
"""

from __future__ import annotations

import sys
import xml.etree.ElementTree as ET
from typing import Iterable

from . import parse
from .ir import Command, Handle, Param, Registry, Struct, StructMember
from .profile import Profile, assign_missing_ids


def lower(root: ET.Element, profile: Profile,
          declared_names: set[str] | None = None) -> Registry:
    reg = Registry()
    reg.declared_names = set(declared_names or ())

    api_consts = parse.parse_api_constants(root)

    # Pull in handles up front; we need them for parameter typing.
    raw_handles = parse.find_handles(root)
    for name, info in raw_handles.items():
        reg.handles[name] = Handle(
            name=name,
            dispatchable=bool(info["dispatchable"]),
            parent=info["parent"],
        )

    # ---- Resolve which commands are in scope ------------------------------
    implemented_extensions: dict[str, str] = {}
    extension_commands: dict[str, set[str]] = {}
    selected = _select_commands(root, profile, implemented_extensions, extension_commands)
    reg.implemented_extensions = implemented_extensions
    reg.manual_commands = set(profile.excluded_commands)

    # Validate / assign protocol IDs.
    sorted_selected = sorted(selected)
    new_ids = assign_missing_ids(profile, sorted_selected)
    if new_ids:
        print(
            f"codegen: assigned new wire IDs to {len(new_ids)} commands",
            file=sys.stderr,
        )

    # Commands any core <feature> block introduces. Everything else is
    # extension-only: the loader exports no symbol for it, so the server
    # has to resolve it at runtime instead of linking against it.
    core_commands: set[str] = set()
    for feat_name, cmds in parse.parse_features(root).items():
        version = parse.parse_feature_number(feat_name)
        if version is not None and version <= profile.api_version:
            core_commands.update(cmds)

    # ---- Lower each command (skip on unsupported shape) -------------------
    skipped: list[tuple[str, str]] = []
    for cmd_name in sorted_selected:
        cmd_el = parse.find_command(root, cmd_name)
        if cmd_el is None:
            skipped.append((cmd_name, "not in vk.xml"))
            continue
        try:
            cmd = _lower_command(cmd_el, cmd_name, api_consts, profile)
            # An alias of a core command resolves to the core symbol,
            # which the loader does export — so it is not extension-only
            # even though an extension is what named it.
            cmd.extension_only = (
                cmd_name not in core_commands
                and (cmd.alias_of or cmd_name) not in core_commands
            )
            reg.commands[cmd_name] = cmd
        except _UnsupportedShape as e:
            skipped.append((cmd_name, str(e)))

    if skipped:
        print(
            f"codegen: skipped {len(skipped)} commands due to unsupported "
            "param shapes:",
            file=sys.stderr,
        )
        for name, why in skipped:
            print(f"  {name}: {why}", file=sys.stderr)

    # ---- Walk the struct closure ------------------------------------------
    raw_structs = parse.find_structs(root)
    needed: set[str] = set()
    queue: list[str] = []
    omit = {"VkAllocationCallbacks"}

    def maybe_enqueue(type_name: str) -> None:
        if type_name in raw_structs and type_name not in needed:
            queue.append(type_name)

    for cmd in reg.commands.values():
        maybe_enqueue(cmd.return_type.base)
        for p in cmd.params:
            maybe_enqueue(p.type.base)

    def drain_queue() -> None:
        while queue:
            name = queue.pop()
            if name in needed or name in omit:
                continue
            if name not in raw_structs:
                continue
            needed.add(name)
            for m_el in raw_structs[name].findall("member"):
                mtype, _ = parse.parse_param(m_el, api_consts)
                if mtype.base in raw_structs and mtype.base not in needed:
                    queue.append(mtype.base)

    drain_queue()

    # ---- Pull in pNext chain extensions -----------------------------------
    # Nothing points at a chain struct: it attaches at runtime through
    # pNext, so the parameter closure above cannot see it. Leaving them
    # out is why every extended feature query came back zero-filled and
    # applications concluded the feature was unsupported.
    #
    # Gated to what the profile targets. Taking every ``structextends``
    # struct in vk.xml would drag in provisional extensions and
    # platform-specific ones whose types the headers do not declare on
    # this build, which is a compile error rather than a missing feature.
    chain_allowed = _chain_struct_allowlist(root, profile)
    if reg.declared_names:
        chain_allowed &= reg.declared_names
    while True:
        additions = [
            name
            for name, el in raw_structs.items()
            if name not in needed
            and name not in omit
            and name in chain_allowed
            and any(
                t.strip() in needed
                for t in (el.get("structextends") or "").split(",")
                if t.strip()
            )
        ]
        if not additions:
            break
        queue.extend(additions)
        drain_queue()

    for name in needed:
        s_el = raw_structs[name]
        members: list[StructMember] = []
        for m_el in s_el.findall("member"):
            if m_el.get("api") == "vulkansc":
                continue
            mtype, mname = parse.parse_param(m_el, api_consts)
            optional = parse.parse_optional(m_el.get("optional"))
            len_expr = m_el.get("altlen") or m_el.get("len")
            values_attr = m_el.get("values") or ""
            values = tuple(s.strip() for s in values_attr.split(",") if s.strip())
            caller_filled = s_el.get("returnedonly") != "true"
            unconditioned = m_el.get("noautovalidity") != "true"
            nullable = bool(optional and optional[0])
            pointer = mtype.pointer_depth >= 1
            required = pointer and caller_filled and unconditioned and not nullable
            conditional = pointer and caller_filled and not unconditioned and not nullable
            handle_required = not pointer and caller_filled and unconditioned and not nullable
            members.append(
                StructMember(
                    name=mname,
                    type=mtype,
                    optional_levels=optional,
                    len_expr=len_expr,
                    values=values,
                    required=required,
                    handle_required=handle_required,
                    conditional=conditional,
                )
            )
        extends_attr = s_el.get("structextends") or ""
        reg.structs[name] = Struct(
            name=name,
            members=members,
            returned_only=s_el.get("returnedonly") == "true",
            extends=tuple(t.strip() for t in extends_attr.split(",") if t.strip()),
            is_union=(s_el.get("category") == "union"),
        )

    # Profile-declared chain nodes: mark them as extending their target
    # so the chain codec treats them like any other pNext struct.
    for name, target in profile.chain_extras.items():
        st = reg.structs.get(name)
        if st is not None and target not in st.extends:
            st.extends = tuple(st.extends) + (target,)

    reg.struct_order = _topo_sort(reg.structs)

    # Final validation pass: emitters classify each parameter into one of
    # a fixed set of shapes. Anything that fails to classify (function
    # pointers, untagged unions, exotic pointer-of-pointer-of-X) gets
    # dropped here so codegen emit stages can assume every Command is
    # emit-friendly. Done AFTER struct closure so the classifier sees
    # the full struct registry.
    from .emit.classify import classify, is_count_array_pair

    classify_failed: list[tuple[str, str]] = []
    for cmd_name in list(reg.commands):
        cmd = reg.commands[cmd_name]
        reason = _validation_reason(reg, cmd, classify, is_count_array_pair)
        if reason is not None:
            classify_failed.append((cmd_name, reason))
            del reg.commands[cmd_name]

    if classify_failed:
        print(
            f"codegen: skipped {len(classify_failed)} commands due to "
            "unsupported param shapes:",
            file=sys.stderr,
        )
        for name, why in classify_failed:
            print(f"  {name}: {why}", file=sys.stderr)

    _withdraw_unbacked_extensions(reg, profile, implemented_extensions,
                                  extension_commands)

    return reg


def _withdraw_unbacked_extensions(
    reg: Registry,
    profile: Profile,
    implemented: dict[str, str],
    extension_commands: dict[str, set[str]],
) -> None:
    """Stop claiming any extension whose commands were not all emitted.

    Advertising an extension with a command missing behind it is the
    worst failure this generator can produce, because nothing downstream
    catches it. The application enables what it was offered, asks
    vkGetDeviceProcAddr for an entry point that was never generated,
    gets null, and calls it. That is a jump to address zero with no
    Vulkan error raised anywhere — it took a D3D8 title down inside
    DXVK, four frames deep, with `vkCmdSetSampleMaskEXT` the only clue
    and it not in any backtrace.

    This runs LAST, and against ``reg.commands`` rather than against a
    list of what was skipped. That is the whole correction: commands are
    dropped in two phases — once when a parameter shape cannot be
    lowered, and again when classification rejects one — and the check
    used to run between them. Every extension withdrawn for the first
    reason was caught; every extension withdrawn for the second was
    claimed anyway. Asking what actually survived cannot go out of step
    with the phases the way a skip list does.
    """
    withdrawn: list[tuple[str, str]] = []
    for ext_name, commands in extension_commands.items():
        if ext_name not in implemented:
            continue
        # Hand-written entry points count as implemented. They are
        # exactly what ``[commands].exclude`` declares: commands the
        # generator must not emit because they carry something that
        # cannot cross the wire — a window handle, a mapped pointer, a
        # surface naming a window in this process. Treating them as
        # missing withdrew VK_KHR_surface and VK_KHR_map_memory2, which
        # this build implements in full.
        missing = sorted(commands - set(reg.commands) - profile.excluded_commands)
        if missing:
            withdrawn.append((ext_name, missing[0]))
            del implemented[ext_name]

    if withdrawn:
        print(
            f"codegen: not claiming {len(withdrawn)} extension(s) whose commands "
            "could not be generated:",
            file=sys.stderr,
        )
        for ext_name, first in withdrawn:
            print(f"  {ext_name}: needs {first}", file=sys.stderr)


# Unions reached only from a struct that serialize.py encodes by hand.
_BESPOKE_UNIONS = {"VkDescriptorDataEXT"}


def _is_address_union(s) -> bool:
    """The ``{deviceAddress, hostAddress}`` unions from ray tracing.

    Both arms describe the same buffer; which one is live depends on
    whether the build runs on the device or the host, and the device path
    is the only one that can cross a process boundary — a host pointer is
    meaningless to the server. Encoding these as their 64-bit device
    address is therefore correct for every command the proxy can support,
    and the host-side build commands stay rejected for other reasons.
    """
    names = {m.name for m in s.members}
    return s.is_union and names == {"deviceAddress", "hostAddress"}


def _pointer_union(reg, type_name: str, seen: set[str] | None = None) -> str | None:
    """Name of a pointer-carrying union reachable from ``type_name``.

    Unions are encoded as raw bytes, which is correct for a union of
    values (VkClearValue) and actively dangerous for a union of pointers
    (VkDescriptorDataEXT). Memcpy'ing the latter ships client-side
    addresses to the server, which hands them to the driver and
    segfaults inside it — a client can crash the render server this way.

    Encoding one properly needs its discriminator, which lives in the
    enclosing struct and differs per union, so it is bespoke work rather
    than something the generic emitter can infer.
    """
    seen = seen if seen is not None else set()
    if type_name in seen:
        return None
    seen.add(type_name)
    s = reg.structs.get(type_name)
    if s is None:
        return None
    # Unions with a hand-written encoding in serialize.py are fine; the
    # generic raw-byte path is what is unsafe.
    if type_name in _BESPOKE_UNIONS:
        return None
    if s.is_union and any(m.type.pointer_depth > 0 for m in s.members) \
            and not _is_address_union(s):
        return type_name
    for m in s.members:
        found = _pointer_union(reg, m.type.base, seen)
        if found is not None:
            return found
    return None


def _validation_reason(reg, cmd, classify, is_count_array_pair) -> str | None:
    """Return a string explaining why ``cmd`` can't be emitted, or None
    if every param is in a shape the codegen handles."""
    for p in cmd.params:
        union = _pointer_union(reg, p.type.base)
        if union is not None:
            return (
                f"parameter {p.name!r} reaches union {union!r}, which holds "
                "pointers; raw-byte encoding would send client addresses to "
                "the server. Needs a discriminator-aware serializer"
            )

    # count_array_pair edge cases the emitters can't yet express:
    #   * size_t counts (only uint32_t handled)
    #   * void* element arrays (vkGetPipelineCacheData et al.)
    i = 0
    while i < len(cmd.params):
        if is_count_array_pair(cmd.params, i):
            count = cmd.params[i]
            arr = cmd.params[i + 1]
            if count.type.base != "uint32_t":
                return (
                    f"count_array_pair count {count.name!r} is "
                    f"{count.type.base}; only uint32_t handled"
                )
            if arr.type.base == "void":
                return (
                    f"count_array_pair element {arr.name!r} is void* "
                    "(opaque blob); needs bespoke serialization"
                )
            i += 2
        else:
            i += 1

    # Output ``void*`` byte buffers. Encodable when the size is a
    # by-value sibling parameter (vkGetQueryPoolResults dataSize,
    # vkGetRayTracingShaderGroupHandlesKHR dataSize) — that is just a
    # byte array going the other way.
    #
    # Without such a sibling the size is only knowable from a property
    # the caller queried earlier (the opaque-capture-descriptor calls
    # work this way), and there is nothing here to derive a length from.
    # Those stay rejected rather than guessed at.
    for p in cmd.params:
        if p.direction == "out" and p.type.pointer_depth == 1 and p.type.base == "void":
            has_size_sibling = p.len_expr is not None and any(
                q.name == p.len_expr
                and q.direction == "in"
                and q.type.pointer_depth == 0
                for q in cmd.params
            )
            if not has_size_sibling:
                return (
                    f"output void* buffer {p.name!r} has no by-value size "
                    "parameter; needs bespoke serialization"
                )

    # Per-param classification.
    try:
        for p in cmd.params:
            classify(reg, p, cmd.params)
    except ValueError as e:
        return str(e)
    return None


# ---- Command selection -----------------------------------------------------


def _chain_struct_allowlist(root: ET.Element, profile: Profile) -> set[str]:
    """Struct names a pNext chain is allowed to carry.

    A struct qualifies when it was introduced by a core version at or
    below the profile's ``api_version``, or by an extension that targets
    the Vulkan API, is not disabled, and whose platform the profile
    builds for.

    Deliberately wider than ``[extensions].include``: device extensions
    are enumerated from the *host* driver at runtime, so an application
    can legitimately enable one this profile never listed and then chain
    its feature struct onto ``vkGetPhysicalDeviceFeatures2``. Refusing to
    encode that struct would report the feature as missing on a host that
    supports it. The gate here is only about what the headers declare.
    """
    allowed: set[str] = set()

    for feat_name, types in parse.parse_feature_types(root).items():
        version = parse.parse_feature_number(feat_name)
        if version is not None and version <= profile.api_version:
            allowed.update(types)

    extensions = parse.parse_extensions(root)
    for ext_name, types in parse.parse_extension_types(root).items():
        info = extensions.get(ext_name)
        if info is None:
            continue
        supported = info["supported"]
        if supported == "disabled" or "vulkan" not in supported.split(","):
            continue
        # ``platform`` is None for portable extensions; otherwise it must
        # be one the profile builds for ("linux" covers xcb/xlib/wayland).
        platform = info["platform"]
        if platform is not None and platform not in profile.platforms:
            continue
        allowed.update(types)

    # Follow struct aliases. An extension's <require> names its own
    # spelling — VkPhysicalDeviceLineRasterizationFeaturesEXT — while the
    # struct that actually carries ``structextends`` is the unsuffixed
    # one, named only by a core <feature> that may be newer than the
    # profile targets. Without this the chain codec silently omits the
    # struct, the feature query comes back zero-filled, and the
    # application reports a feature the host genuinely supports as
    # missing.
    for t in root.findall("types/type"):
        if t.get("category") != "struct":
            continue
        name, target = t.get("name"), t.get("alias")
        if name and target and name in allowed:
            allowed.add(target)

    return allowed


def _select_commands(root: ET.Element, profile: Profile,
                     implemented: dict[str, str] | None = None,
                     extension_commands: dict[str, set[str]] | None = None) -> set[str]:
    features = parse.parse_features(root)
    extensions = parse.parse_extensions(root)

    selected: set[str] = set()

    # Core features: include every <feature> with version ≤ profile.api_version.
    for feat_name, cmds in features.items():
        version = parse.parse_feature_number(feat_name)
        if version is None:
            continue
        if version <= profile.api_version:
            selected.update(cmds)

    # Extensions: requested + transitive via ``depends``.
    visited: set[str] = set()
    queue: list[str] = list(profile.extensions)
    while queue:
        ext_name = queue.pop()
        if ext_name in visited:
            continue
        visited.add(ext_name)
        info = extensions.get(ext_name)
        if info is None:
            print(f"warning: extension {ext_name!r} not found in vk.xml", file=sys.stderr)
            continue
        if info["supported"] == "disabled":
            print(f"warning: extension {ext_name!r} is disabled in vk.xml", file=sys.stderr)
            continue
        if "vulkan" not in info["supported"].split(","):
            print(
                f"warning: extension {ext_name!r} doesn't support the vulkan API",
                file=sys.stderr,
            )
            continue
        selected.update(info["commands"])
        # Record it as implemented, with the type the runtime needs to
        # know: a device extension is the one an application can be
        # offered by the host driver without this generator ever having
        # heard of it.
        if implemented is not None and info["type"]:
            implemented[ext_name] = info["type"]
        if extension_commands is not None:
            extension_commands[ext_name] = set(info["commands"])
        for dep in _split_depends(info["depends"]):
            queue.append(dep)

    # Apply the explicit exclude list (hand-written entry points).
    selected -= profile.excluded_commands
    return selected


def _split_depends(depends: str) -> Iterable[str]:
    """Split a ``depends="A+B,C"`` attribute into individual extension names.

    The vk.xml grammar uses ``,`` for OR and ``+`` for AND; for our
    selection purpose we just want the union of mentioned extensions, so
    we treat both delimiters the same.
    """
    if not depends:
        return ()
    out: list[str] = []
    for part in depends.replace(",", "+").split("+"):
        p = part.strip()
        if p and not parse.parse_feature_number(p):  # ignore version deps
            out.append(p)
    return out


# ---- Per-command lowering --------------------------------------------------


class _UnsupportedShape(Exception):
    """Raised by ``_lower_command`` when a parameter cannot be lowered."""


def _lower_command(cmd_el: ET.Element, cmd_name: str,
                   api_consts: dict[str, int], profile: Profile) -> Command:
    proto = cmd_el.find("proto")
    if proto is None:
        raise _UnsupportedShape("command missing <proto>")

    return_type, _ = parse.parse_param(proto, api_consts)

    params: list[Param] = []
    for p_el in cmd_el.findall("param"):
        if p_el.get("api") == "vulkansc":
            continue
        ptype, pname = parse.parse_param(p_el, api_consts)
        optional = parse.parse_optional(p_el.get("optional"))
        len_expr = p_el.get("altlen") or p_el.get("len")
        direction = _infer_direction(ptype, pname)
        null_refused = p_el.get("noautovalidity") != "true" and not (optional and optional[0])
        params.append(
            Param(
                name=pname,
                type=ptype,
                optional_levels=optional,
                null_refused=null_refused,
                len_expr=len_expr,
                direction=direction,
                inner_len=profile.inner_lens.get(f"{cmd_name}.{pname}"),
            )
        )

    sync_mode = profile.sync_overrides.get(
        cmd_name, _default_sync_mode(return_type, params)
    )

    # ``find_command`` follows aliases, so ``cmd_el`` is always the core
    # element: its proto name is the symbol that actually exists in the
    # loader, which is not necessarily the name we were asked for.
    real_name = proto.findtext("name")

    return Command(
        name=cmd_name,
        return_type=return_type,
        params=params,
        cmd_id=profile.command_ids[cmd_name],
        alias_of=real_name if real_name and real_name != cmd_name else None,
        sync_mode=sync_mode,
        success_codes=parse.parse_codes(cmd_el.get("successcodes")),
        error_codes=parse.parse_codes(cmd_el.get("errorcodes")),
    )


# ---- Helpers ---------------------------------------------------------------


def _topo_sort(structs: dict[str, Struct]) -> list[str]:
    order: list[str] = []
    visiting: set[str] = set()
    visited: set[str] = set()

    def visit(name: str) -> None:
        if name in visited:
            return
        if name in visiting:
            raise ValueError(f"cycle in struct dependencies through {name}")
        visiting.add(name)
        s = structs.get(name)
        if s is not None:
            for m in s.members:
                if m.type.pointer_depth == 0 and m.type.base in structs:
                    visit(m.type.base)
        visiting.discard(name)
        visited.add(name)
        order.append(name)

    for n in sorted(structs):
        visit(n)
    return order


def _infer_direction(t, name: str) -> str:  # noqa: ARG001
    if t.pointer_depth == 0:
        return "in"
    if t.is_const:
        return "in"
    return "out"


def _default_sync_mode(ret_type, params) -> str:
    if ret_type.base != "void":
        return "sync"
    for p in params:
        if p.direction == "out":
            return "sync"
    return "async"
