"""cl.xml to raw commands.

cl.xml is a much smaller document than vk.xml and a much plainer one:
no ``len``, no ``optional``, no struct extension chains. What it gives
is a list of commands, their return types, and their parameters' C
declarations. Everything the wire needs beyond that — which pointer is
an array and what counts it — is not in the registry at all and comes
from the profile instead.
"""

from __future__ import annotations

import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

from ..ir import CType


@dataclass
class RawParam:
    name: str
    type: CType
    # A function-pointer parameter (``pfn_notify``). The registry writes
    # these without a <type> child, which is how they are recognised.
    is_callback: bool = False
    # The declaration as the registry wrote it. Kept for callbacks,
    # whose type cannot be rebuilt from a base and a pointer count.
    text: str = ""


@dataclass
class RawCommand:
    name: str
    return_type: CType
    params: list[RawParam] = field(default_factory=list)
    # The feature block that introduced it, e.g. "CL_VERSION_1_2".
    since: str = ""


@dataclass
class RawExtension:
    name: str
    # The commands the registry puts in it; empty for the ones that only
    # add enums to another extension, as the external-memory family does.
    commands: list[str] = field(default_factory=list)
    # The other extensions it is written on top of, from `depends`. Version
    # names there (CL_VERSION_3_0) are not extensions and are dropped: the
    # wire's tier is not this list's business.
    depends: list[str] = field(default_factory=list)


@dataclass
class RawRegistry:
    commands: dict[str, RawCommand] = field(default_factory=dict)
    # Type name -> category, for the categories the emitters care about.
    handles: set[str] = field(default_factory=set)
    structs: dict[str, list[tuple[str, CType]]] = field(default_factory=dict)
    extensions: dict[str, RawExtension] = field(default_factory=dict)


def _ctype_from(node: ET.Element) -> CType:
    """The C type of a <proto> or <param>, from its mixed content."""
    type_node = node.find("type")
    base = type_node.text.strip() if type_node is not None and type_node.text else "void"
    # Text after <type> carries the stars; text before it carries const.
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
                # cl_platform_id and friends are declared as opaque
                # pointer typedefs; the registry marks them nowhere else.
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

    # Which feature block first mentions a command.
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
