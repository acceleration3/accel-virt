from __future__ import annotations

from .model import Command, Param, Registry, Role
from .parse import RawCommand, RawRegistry

_COUNT_PREFIXES = ("num_", "count", "n_")

_PROPERTY_TYPES = {
    "cl_context_properties",
    "cl_queue_properties",
    "cl_pipe_properties",
    "cl_sampler_properties",
    "cl_mem_properties",
}

_PROPERTY_LIST_TYPES = {
    "cl_device_partition_property",
    "cl_pipe_properties",
}


def _element_of(base: str) -> str:
    return base


def _infer(raw: RawCommand, index: int, handles: set[str],
           structs: dict) -> Param:
    p = raw.params[index]
    t = p.type
    name = p.name
    prev = raw.params[index - 1] if index > 0 else None
    nxt = raw.params[index + 1] if index + 1 < len(raw.params) else None

    if p.is_callback:
        return Param(name, t, Role.IGNORED)
    if name == "user_data" and prev is not None and prev.is_callback:
        return Param(name, t, Role.IGNORED)

    if t.pointer_depth == 0:
        role = Role.HANDLE if t.base in handles else Role.SCALAR
        return Param(name, t, role, element=t.base)

    if t.base in _PROPERTY_TYPES:
        return Param(name, t, Role.PROPERTIES, element=t.base)

    if t.base in _PROPERTY_LIST_TYPES:
        return Param(name, t, Role.PROPERTY_LIST, element=t.base)

    if t.base == "char":
        if t.pointer_depth == 2:
            return Param(name, t, Role.STRING_ARRAY,
                         len_param=prev.name if prev is not None else None,
                         lengths_param=nxt.name if nxt is not None else None)
        return Param(name, t, Role.STRING)

    if t.base in handles:
        if name == "event":
            return Param(name, t, Role.HANDLE_OUT, element=t.base)
        if t.is_const:
            return Param(name, t, Role.HANDLE_ARRAY,
                         len_param=prev.name if prev is not None else None,
                         element=t.base)
        capacity = prev.name if prev is not None and prev.type.pointer_depth == 0 else None
        count_out = nxt.name if nxt is not None and nxt.type.pointer_depth == 1 else None
        if capacity is not None and count_out is not None:
            return Param(name, t, Role.HANDLE_ARRAY_OUT, len_param=capacity,
                         size_ret_param=count_out, element=t.base)
        return Param(name, t, Role.HANDLE_OUT, element=t.base)

    if t.base == "void":
        if name == "param_value":
            return Param(name, t, Role.INFO_VALUE,
                         size_param=prev.name if prev is not None else None,
                         size_ret_param=nxt.name if nxt is not None else None)
        return Param(name, t, Role.BYTES_IN if t.is_const else Role.BYTES_OUT)

    if t.base in structs:
        return Param(name, t, Role.STRUCT_IN if t.is_const else Role.STRUCT_OUT,
                     element=t.base)

    if t.is_const:
        return Param(name, t, Role.SCALAR_ARRAY, element=t.base)
    return Param(name, t, Role.SCALAR_OUT, element=t.base)


def _needs_len(param: Param) -> bool:
    return param.role in (Role.HANDLE_ARRAY, Role.SCALAR_ARRAY, Role.BYTES_IN,
                          Role.BYTES_OUT) and param.len_param is None


def lower(raw: RawRegistry, profile) -> Registry:
    reg = Registry(handles=set(raw.handles), structs=dict(raw.structs))

    for name in profile.commands:
        rc = raw.commands.get(name)
        if rc is None:
            raise KeyError(f"{name} is not in cl.xml")
        cmd = Command(name=name, return_type=rc.return_type,
                      cmd_id=profile.command_id(name), since=rc.since,
                      is_async=name in profile.async_commands)
        for i in range(len(rc.params)):
            param = _infer(rc, i, reg.handles, reg.structs)
            override = profile.param_override(name, param.name)
            if override:
                if "role" in override:
                    param.role = Role(override["role"])
                for key in ("len", "lengths", "size", "size_ret"):
                    if key not in override:
                        continue
                    field = {"len": "len_param",
                             "lengths": "lengths_param",
                             "size": "size_param",
                             "size_ret": "size_ret_param"}[key]
                    setattr(param, field, override[key] or None)
                if "optional" in override:
                    param.optional = bool(override["optional"])
                if "element" in override:
                    param.element = override["element"]
                if "lent" in override:
                    param.lent = bool(override["lent"])
            if param.lent and param.role not in (Role.BYTES_IN, Role.BYTES_OUT):
                raise ValueError(f"{name}.{param.name} is lent but is not bytes")
            if _needs_len(param):
                raise ValueError(
                    f"{name}.{param.name} is a {param.role.value} and cl.xml "
                    f"does not say what counts it; annotate it in the profile")
            cmd.params.append(param)
        reg.commands[name] = cmd

    _carry_extensions(raw, reg, profile)
    return reg


def _carry_extensions(raw: RawRegistry, reg: Registry, profile) -> None:
    answered = set(reg.commands) | set(profile.driver_implemented)
    carried = {name for name, ext in raw.extensions.items()
               if all(command in answered for command in ext.commands)}
    dropped = True
    while dropped:
        dropped = False
        for name in sorted(carried):
            if any(need not in carried for need in raw.extensions[name].depends):
                carried.discard(name)
                dropped = True
    carried |= set(profile.driver_extensions)
    reg.carried_extensions = [name for name in sorted(raw.extensions) if name in carried]
