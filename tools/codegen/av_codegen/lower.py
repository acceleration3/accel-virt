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

    raw_handles = parse.find_handles(root)
    for name, info in raw_handles.items():
        reg.handles[name] = Handle(
            name=name,
            dispatchable=bool(info["dispatchable"]),
            parent=info["parent"],
        )

    implemented_extensions: dict[str, str] = {}
    extension_commands: dict[str, set[str]] = {}
    selected = _select_commands(root, profile, implemented_extensions, extension_commands)
    reg.implemented_extensions = implemented_extensions
    reg.manual_commands = set(profile.excluded_commands)

    sorted_selected = sorted(selected)
    new_ids = assign_missing_ids(profile, sorted_selected)
    if new_ids:
        print(
            f"codegen: assigned new wire IDs to {len(new_ids)} commands",
            file=sys.stderr,
        )

    core_commands: set[str] = set()
    for feat_name, cmds in parse.parse_features(root).items():
        version = parse.parse_feature_number(feat_name)
        if version is not None and version <= profile.api_version:
            core_commands.update(cmds)

    skipped: list[tuple[str, str]] = []
    for cmd_name in sorted_selected:
        cmd_el = parse.find_command(root, cmd_name)
        if cmd_el is None:
            skipped.append((cmd_name, "not in vk.xml"))
            continue
        try:
            cmd = _lower_command(cmd_el, cmd_name, api_consts, profile)
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

    for name, target in profile.chain_extras.items():
        st = reg.structs.get(name)
        if st is not None and target not in st.extends:
            st.extends = tuple(st.extends) + (target,)

    reg.struct_order = _topo_sort(reg.structs)

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
    _record_command_extensions(reg, root)

    return reg


def _record_command_extensions(reg: Registry, root: ET.Element) -> None:
    listed: dict[str, list[tuple[str, str]]] = {}
    for ext_name, info in parse.parse_extensions(root).items():
        if info["supported"] == "disabled" or "vulkan" not in info["supported"].split(",") \
                or not info["type"]:
            continue
        for command in info["commands"]:
            listed.setdefault(command, []).append((ext_name, info["type"]))
    for name, cmd in reg.commands.items():
        found = set(listed.get(name, ())) | set(listed.get(cmd.alias_of or name, ()))
        reg.command_extensions[name] = sorted(found)


def _withdraw_unbacked_extensions(
    reg: Registry,
    profile: Profile,
    implemented: dict[str, str],
    extension_commands: dict[str, set[str]],
) -> None:
    withdrawn: list[tuple[str, str]] = []
    for ext_name, commands in extension_commands.items():
        if ext_name not in implemented:
            continue
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


_BESPOKE_UNIONS = {"VkDescriptorDataEXT"}


def _is_address_union(s) -> bool:
    names = {m.name for m in s.members}
    return s.is_union and names == {"deviceAddress", "hostAddress"}


def _pointer_union(reg, type_name: str, seen: set[str] | None = None) -> str | None:
    seen = seen if seen is not None else set()
    if type_name in seen:
        return None
    seen.add(type_name)
    s = reg.structs.get(type_name)
    if s is None:
        return None
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
    for p in cmd.params:
        union = _pointer_union(reg, p.type.base)
        if union is not None:
            return (
                f"parameter {p.name!r} reaches union {union!r}, which holds "
                "pointers; raw-byte encoding would send client addresses to "
                "the server. Needs a discriminator-aware serializer"
            )

    blobs: set[str] = set()
    i = 0
    while i < len(cmd.params):
        if is_count_array_pair(cmd.params, i):
            count = cmd.params[i]
            arr = cmd.params[i + 1]
            wide_blob = count.type.base == "size_t" and arr.type.base == "void"
            if count.type.base != "uint32_t" and not wide_blob:
                return (
                    f"count_array_pair count {count.name!r} is "
                    f"{count.type.base}; only uint32_t handled"
                )
            if arr.type.base == "void" and not wide_blob:
                return (
                    f"count_array_pair element {arr.name!r} is void* "
                    "(opaque blob); needs bespoke serialization"
                )
            if wide_blob:
                blobs.add(arr.name)
            i += 2
        else:
            i += 1

    for p in cmd.params:
        if p.name in blobs:
            continue
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

    try:
        for p in cmd.params:
            classify(reg, p, cmd.params)
    except ValueError as e:
        return str(e)
    return None


def _chain_struct_allowlist(root: ET.Element, profile: Profile) -> set[str]:
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
        platform = info["platform"]
        if platform is not None and platform not in profile.platforms:
            continue
        allowed.update(types)

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

    for feat_name, cmds in features.items():
        version = parse.parse_feature_number(feat_name)
        if version is None:
            continue
        if version <= profile.api_version:
            selected.update(cmds)

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
        if implemented is not None and info["type"]:
            implemented[ext_name] = info["type"]
        if extension_commands is not None:
            extension_commands[ext_name] = set(info["commands"])
        for dep in _split_depends(info["depends"]):
            queue.append(dep)

    selected -= profile.excluded_commands
    return selected


def _split_depends(depends: str) -> Iterable[str]:
    if not depends:
        return ()
    out: list[str] = []
    for part in depends.replace(",", "+").split("+"):
        p = part.strip()
        if p and not parse.parse_feature_number(p):
            out.append(p)
    return out


class _UnsupportedShape(Exception):
    pass


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
                stride=p_el.get("stride"),
            )
        )

    sync_mode = profile.sync_overrides.get(
        cmd_name, _default_sync_mode(return_type, params)
    )

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
        queues=parse.parse_codes(cmd_el.get("queues")),
    )


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
