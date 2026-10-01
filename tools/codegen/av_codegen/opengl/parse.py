"""gl.xml and wgl.xml to raw commands.

The OpenGL registry is the opposite of cl.xml in the one way that
matters: it says how long its pointers are. ``len="count"`` names a
sibling parameter, ``len="COMPSIZE(pname)"`` says the length depends on
an enum whose meaning the registry does not carry, and a bare pointer
with no ``len`` at all is either a string, an offset into a bound
buffer, or something the profile has to explain. So the parser's job
is to keep the registry's own words — including the ``len`` text and
the ``group`` an enum belongs to — and leave the judgement to lowering.

Two documents are read into one registry. wgl.xml describes the
Windows window-system binding, whose commands cross the same wire and
are told apart only by their names.
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
    # The registry's own ``len``: a sibling parameter, an expression
    # over siblings, or COMPSIZE(...) — kept as written.
    len_text: str | None = None
    # The enum group, where the registry names one. This is what makes
    # a pname-driven length knowable at all.
    group: str | None = None
    # The declaration as written, for the parameters whose type cannot
    # be rebuilt from a base and a star count.
    text: str = ""


@dataclass
class RawCommand:
    name: str
    return_type: CType
    params: list[RawParam] = field(default_factory=list)
    # The feature that first required it, e.g. "GL_VERSION_3_3", or the
    # extension where the command has no feature.
    since: str = ""
    # "gl" or "wgl": which document it came from.
    api: str = "gl"
    # An alias the registry declares, e.g. glDrawArraysEXT -> glDrawArrays.
    alias: str | None = None


@dataclass
class RawRegistry:
    commands: dict[str, RawCommand] = field(default_factory=dict)
    # Feature name -> the commands it requires.
    features: dict[str, set[str]] = field(default_factory=dict)
    # Feature name -> the commands it takes away again. The core
    # profile is the compatibility one minus these, and a removal in
    # 3.2 applies to what 1.0 asked for — so they are kept apart from
    # the requires and subtracted once the tier is known.
    removals: dict[str, set[str]] = field(default_factory=dict)
    # Extension name -> the commands it requires. Kept apart from the
    # features because the two are asked different questions: a feature
    # decides what is generated, an extension decides what may be
    # claimed.
    extensions: dict[str, set[str]] = field(default_factory=dict)
    # Enum group -> the enum names in it, which is how a pname-driven
    # length is checked against something rather than guessed.
    groups: dict[str, set[str]] = field(default_factory=dict)
    # Enum name -> value, for the tables the emitters need.
    enums: dict[str, str] = field(default_factory=dict)
    # Type name -> the C the registry declares it with, in document
    # order, which is dependency order: GLDEBUGPROC is written in terms
    # of GLenum and the registry declares GLenum first.
    types: dict[str, str] = field(default_factory=dict)


def _ctype_from(node: ET.Element) -> CType:
    """The C type of a <proto> or <param>.

    GL writes the base type as <ptype> when it has a name of its own and
    as plain text when it does not — ``const void *pixels`` has no
    <ptype> at all. The stars are always in the text around the name.
    """
    ptype = node.find("ptype")
    text = "".join(node.itertext())
    name = node.findtext("name") or ""
    # Everything but the name is the type.
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

    # Only gl.xml's. The window-system binding declares its parameters
    # in the platform's own types - HDC, HGLRC - which come from
    # windows.h and cannot be written out here.
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

    # Which feature introduced each command, taken in document order so
    # the earliest wins.
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
