"""The D3D registry: this project's own description of the commands its
Direct3D user-mode driver sends and the device's executor runs.

There is no Khronos registry for Direct3D, so the registry is written here
(tools/codegen/registry/d3d.toml), in the smallest vocabulary that carries the
DDI: fixed-width scalars, plain structs, counted arrays of either, and bulk
bytes that cross inline or through the context's blob arena. What the wire
carries is plain data both ends lay out identically - the guest and the host
are both little-endian x86-64 - so a struct crosses as its bytes, and every
field of it is judged by the server before the host's Direct3D sees it.
"""

from __future__ import annotations

import re
import tomllib
from dataclasses import dataclass, field
from pathlib import Path

SCALARS = {
    "u8": ("uint8_t", 1),
    "u16": ("uint16_t", 2),
    "u32": ("uint32_t", 4),
    "u64": ("uint64_t", 8),
    "i32": ("int32_t", 4),
    "f32": ("float", 4),
}

PARAM = re.compile(r"^(?P<type>\w+)\s+(?P<name>\w+)(?:\[(?P<count>\w+)\]\s+max\s+(?P<max>\d+))?$")
FIELD = re.compile(r"^(?P<type>\w+)\s+(?P<name>\w+)(?:\[(?P<length>\d+)\])?$")


class RegistryError(Exception):
    pass


@dataclass
class Field:
    type: str
    name: str
    length: int  # 0 for a single value

    def c_decl(self, structs) -> str:
        base = SCALARS[self.type][0] if self.type in SCALARS else self.type
        suffix = f"[{self.length}]" if self.length else ""
        return f"{base} {self.name}{suffix};"


@dataclass
class Struct:
    name: str
    fields: list[Field]
    size: int = 0
    align: int = 1


@dataclass
class Param:
    type: str      # a scalar, a struct, "bytes", or (reply only) "out_bytes"
    name: str
    count: str = ""  # the u32 parameter counting an array
    max: int = 0

    @property
    def is_array(self) -> bool:
        return bool(self.count)


@dataclass
class Command:
    name: str
    id: int
    params: list[Param]
    reply: list[Param] = field(default_factory=list)
    doc: str = ""

    @property
    def sync(self) -> bool:
        return bool(self.reply)


@dataclass
class Registry:
    api: int
    prefix: str
    structs: dict[str, Struct]
    commands: list[Command]


def _parse_field(text: str) -> Field:
    m = FIELD.match(text.strip())
    if not m:
        raise RegistryError(f"cannot read field {text!r}")
    return Field(m["type"], m["name"], int(m["length"] or 0))


def _parse_param(text: str) -> Param:
    m = PARAM.match(text.strip())
    if not m:
        raise RegistryError(f"cannot read parameter {text!r}")
    return Param(m["type"], m["name"], m["count"] or "", int(m["max"] or 0))


def _layout(structs: dict[str, Struct]) -> None:
    """Sizes and alignments as the C++ compiler lays the emitted structs out;
    the emitter static_asserts each, so a disagreement is a build failure."""
    done: set[str] = set()

    def visit(s: Struct, stack: tuple[str, ...]) -> None:
        if s.name in done:
            return
        if s.name in stack:
            raise RegistryError(f"struct {s.name} contains itself")
        offset = 0
        align = 1
        for f in s.fields:
            if f.type in SCALARS:
                size, a = SCALARS[f.type][1], SCALARS[f.type][1]
            elif f.type in structs:
                inner = structs[f.type]
                visit(inner, stack + (s.name,))
                size, a = inner.size, inner.align
            else:
                raise RegistryError(f"struct {s.name}: unknown type {f.type}")
            if offset % a:
                raise RegistryError(f"struct {s.name}: field {f.name} is not naturally aligned; "
                                    f"add explicit padding so both ends agree without the compiler's help")
            offset += size * (f.length or 1)
            align = max(align, a)
        if offset % align:
            raise RegistryError(f"struct {s.name}: size {offset} is not a multiple of {align}; pad it")
        s.size, s.align = offset, align
        done.add(s.name)

    for s in structs.values():
        visit(s, ())


def _check_params(reg: Registry, cmd: Command, params: list[Param], reply: bool) -> None:
    seen: dict[str, Param] = {}
    for p in params:
        if p.name in seen:
            raise RegistryError(f"{cmd.name}: {p.name} twice")
        if p.type == "out_bytes" and not reply:
            raise RegistryError(f"{cmd.name}: out_bytes {p.name} belongs in the reply")
        if p.type not in SCALARS and p.type not in reg.structs and p.type not in ("bytes", "out_bytes"):
            raise RegistryError(f"{cmd.name}: {p.name} has unknown type {p.type}")
        if p.is_array:
            if reply:
                raise RegistryError(f"{cmd.name}: arrays in a reply are out_bytes")
            counter = seen.get(p.count)
            if counter is None or counter.type != "u32" or counter.is_array:
                raise RegistryError(f"{cmd.name}: {p.name}'s count {p.count} must be an earlier u32")
            if p.max <= 0:
                raise RegistryError(f"{cmd.name}: {p.name} needs a max")
            if p.type in ("bytes", "out_bytes"):
                raise RegistryError(f"{cmd.name}: {p.name}: an array of bytes is bytes")
        seen[p.name] = p


def load(path: Path) -> Registry:
    raw = tomllib.loads(path.read_text(encoding="utf-8"))
    meta = raw.get("meta", {})
    structs: dict[str, Struct] = {}
    for name, body in raw.get("structs", {}).items():
        structs[name] = Struct(name, [_parse_field(f) for f in body["fields"]])
    _layout(structs)
    reg = Registry(int(meta["api"]), meta.get("prefix", "d3d"), structs, [])
    # Registries that share an api share its id space: each keeps to its range.
    first_id, last_id = int(meta.get("first_id", 1)), int(meta.get("last_id", 0xffff))
    ids: set[int] = set()
    names: set[str] = set()
    for body in raw.get("command", []):
        cmd = Command(body["name"], int(body["id"]), [_parse_param(p) for p in body.get("params", [])],
                      [_parse_param(p) for p in body.get("reply", [])], body.get("doc", ""))
        if cmd.id in ids or not first_id <= cmd.id <= last_id or not 0 < cmd.id < 0x10000:
            raise RegistryError(f"{cmd.name}: id {cmd.id:#x} is taken or out of range")
        if cmd.name in names:
            raise RegistryError(f"{cmd.name} twice")
        ids.add(cmd.id)
        names.add(cmd.name)
        _check_params(reg, cmd, cmd.params, False)
        _check_params(reg, cmd, cmd.reply, True)
        clash = {p.name for p in cmd.params} & {p.name for p in cmd.reply}
        if clash:
            raise RegistryError(f"{cmd.name}: {sorted(clash)} named in both the parameters and the reply")
        reg.commands.append(cmd)
    return reg
