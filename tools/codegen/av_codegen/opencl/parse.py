from __future__ import annotations

import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

from ..ir import CType


@dataclass
class RawParam:
    name: str
    type: CType
    is_callback: bool = False
    text: str = ""


@dataclass
class RawCommand:
    name: str
    return_type: CType
    params: list[RawParam] = field(default_factory=list)
    since: str = ""


@dataclass
class RawExtension:
    name: str
    commands: list[str] = field(default_factory=list)
    depends: list[str] = field(default_factory=list)


@dataclass
class RawRegistry:
    commands: dict[str, RawCommand] = field(default_factory=dict)
    handles: set[str] = field(default_factory=set)
    structs: dict[str, list[tuple[str, CType]]] = field(default_factory=dict)
    extensions: dict[str, RawExtension] = field(default_factory=dict)


def _ctype_from(node: ET.Element) -> CType:
    type_node = node.find("type")
    base = type_node.text.strip() if type_node is not None and type_node.text else "void"
    trailing = (type_node.tail or "") if type_node is not None else ""
    leading = node.text or ""
    pointer_depth = trailing.count("*")
    return CType(base=base, pointer_depth=pointer_depth,
                 is_const="const" in leading)


def parse(path: Path) -> RawRegistry:
    root = ET.parse(path).getroot()
    reg = RawRegistry()

    types = root.find("types")
    if types is not None:
        for t in types.findall("type"):
            category = t.get("category")
            if category == "define":
                name = t.findtext("name") or ""
                text = "".join(t.itertext())
                if name.startswith("cl_") and "_cl_" in text and "*" in text:
                    reg.handles.add(name)
            elif category == "struct":
                name = t.get("name") or ""
                members: list[tuple[str, CType]] = []
                for m in t.findall("member"):
                    mname = m.findtext("name") or ""
                    members.append((mname, _ctype_from(m)))
                if name:
                    reg.structs[name] = members

    since: dict[str, str] = {}
    for feature in root.findall("feature"):
        api_name = feature.get("name") or ""
        for cmd in feature.findall(".//command"):
            name = cmd.get("name") or ""
            since.setdefault(name, api_name)

    for cmd in root.find("commands") or []:
        proto = cmd.find("proto")
        if proto is None:
            continue
        name = proto.findtext("name") or ""
        if not name:
            continue
        raw = RawCommand(name=name, return_type=_ctype_from(proto),
                         since=since.get(name, ""))
        for p in cmd.findall("param"):
            pname = p.findtext("name") or ""
            if p.find("type") is None:
                declaration = " ".join("".join(p.itertext()).split())
                raw.params.append(RawParam(pname, CType("void", 1),
                                           is_callback=True,
                                           text=declaration))
                continue
            raw.params.append(RawParam(pname, _ctype_from(p)))
        reg.commands[name] = raw

    for ext in root.findall("./extensions/extension"):
        name = ext.get("name") or ""
        if not name.startswith("cl_"):
            continue
        depends = [part for part in (ext.get("depends") or "").replace("(", "")
                   .replace(")", "").replace(",", "+").split("+")
                   if part.startswith("cl_")]
        reg.extensions[name] = RawExtension(
            name=name,
            commands=[c.get("name") or "" for c in ext.iter("command")],
            depends=depends)

    return reg
