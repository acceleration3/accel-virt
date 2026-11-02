from __future__ import annotations

import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

from ..ir import CType


@dataclass
class RawParam:
    name: str
    type: CType
    len_text: str | None = None
    group: str | None = None
    text: str = ""


@dataclass
class RawCommand:
    name: str
    return_type: CType
    params: list[RawParam] = field(default_factory=list)
    since: str = ""
    api: str = "gl"
    alias: str | None = None


@dataclass
class RawRegistry:
    commands: dict[str, RawCommand] = field(default_factory=dict)
    features: dict[str, set[str]] = field(default_factory=dict)
    removals: dict[str, set[str]] = field(default_factory=dict)
    extensions: dict[str, set[str]] = field(default_factory=dict)
    groups: dict[str, set[str]] = field(default_factory=dict)
    enums: dict[str, str] = field(default_factory=dict)
    types: dict[str, str] = field(default_factory=dict)


def _ctype_from(node: ET.Element) -> CType:
    ptype = node.find("ptype")
    text = "".join(node.itertext())
    name = node.findtext("name") or ""
    declaration = text.replace(name, " ", 1) if name else text
    if ptype is not None and ptype.text:
        base = ptype.text.strip()
    else:
        words = [w for w in declaration.replace("*", " ").split()
                 if w not in ("const", "GLAPI", "APIENTRY")]
        base = words[0] if words else "void"
    return CType(base=base, pointer_depth=declaration.count("*"),
                 is_const="const" in declaration,
                 inner_const="*const*" in "".join(declaration.split()))


def _read(path: Path, api: str, reg: RawRegistry) -> None:
    root = ET.parse(path).getroot()

    if api == "gl":
        for t in root.findall("types/type"):
            name = t.get("name") or t.findtext("name") or ""
            text = "".join(t.itertext())
            if not name or "#include" in text:
                continue
            reg.types.setdefault(name, text)

    for enums in root.findall("enums"):
        group_attr = enums.get("group")
        for e in enums.findall("enum"):
            name = e.get("name") or ""
            if not name:
                continue
            reg.enums.setdefault(name, e.get("value") or "")
            for group in (e.get("group") or group_attr or "").split(","):
                group = group.strip()
                if group:
                    reg.groups.setdefault(group, set()).add(name)

    for feature in root.findall("feature"):
        name = feature.get("name") or ""
        wanted: set[str] = set()
        gone: set[str] = set()
        for req in feature.findall("require"):
            for c in req.findall("command"):
                wanted.add(c.get("name") or "")
        for rem in feature.findall("remove"):
            for c in rem.findall("command"):
                gone.add(c.get("name") or "")
        reg.features[name] = {w for w in wanted if w}
        reg.removals[name] = {g for g in gone if g}

    for extension in root.findall("extensions/extension"):
        name = extension.get("name") or ""
        wanted = set()
        for req in extension.findall("require"):
            for c in req.findall("command"):
                wanted.add(c.get("name") or "")
        if name:
            reg.extensions[name] = wanted
        if wanted:
            reg.features[name] = wanted

    for cmd in root.find("commands") or []:
        proto = cmd.find("proto")
        if proto is None:
            continue
        name = proto.findtext("name") or ""
        if not name:
            continue
        alias = cmd.find("alias")
        raw = RawCommand(name=name, return_type=_ctype_from(proto), api=api,
                         alias=alias.get("name") if alias is not None else None)
        for p in cmd.findall("param"):
            raw.params.append(RawParam(
                name=p.findtext("name") or "",
                type=_ctype_from(p),
                len_text=p.get("len"),
                group=p.get("group"),
                text=" ".join("".join(p.itertext()).split()),
            ))
        reg.commands[name] = raw

    for feature in root.findall("feature"):
        fname = feature.get("name") or ""
        for c in feature.findall(".//require/command"):
            cname = c.get("name") or ""
            if cname in reg.commands and not reg.commands[cname].since:
                reg.commands[cname].since = fname


def parse(gl_path: Path, wgl_path: Path | None = None) -> RawRegistry:
    reg = RawRegistry()
    _read(gl_path, "gl", reg)
    if wgl_path is not None and wgl_path.is_file():
        _read(wgl_path, "wgl", reg)
    return reg
