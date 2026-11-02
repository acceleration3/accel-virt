from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from pathlib import Path

from .ir import CType


def load_xml(path: Path) -> ET.Element:
    return ET.parse(path).getroot()


def parse_param(el: ET.Element, api_consts: dict[str, int] | None = None) -> tuple[CType, str]:
    if api_consts is None:
        api_consts = {}

    is_const = "const" in (el.text or "")
    base: str | None = None
    pointer_depth = 0
    name: str | None = None
    fixed_dim: int | None = None
    api_constant: str | None = None

    for child in el:
        if child.tag == "type":
            base = (child.text or "").strip()
            tail = child.tail or ""
            pointer_depth += tail.count("*")
        elif child.tag == "name":
            name = (child.text or "").strip()
            tail = (child.tail or "").strip()
            if tail.startswith("[") and tail.endswith("]") and tail != "[":
                inner = tail[1:-1]
                if inner.isdigit():
                    fixed_dim = int(inner)
        elif child.tag == "enum":
            sym = (child.text or "").strip()
            if sym in api_consts:
                fixed_dim = api_consts[sym]
                api_constant = sym

    if base is None or name is None:
        raise ValueError(f"could not parse param: {ET.tostring(el)!r}")

    return (
        CType(
            base=base,
            pointer_depth=pointer_depth,
            is_const=is_const,
            fixed_dim=fixed_dim,
            api_constant=api_constant,
        ),
        name,
    )


def parse_api_constants(root: ET.Element) -> dict[str, int]:
    out: dict[str, int] = {}
    for enums in root.findall("enums"):
        if enums.get("name") != "API Constants":
            continue
        for e in enums.findall("enum"):
            v = e.get("value")
            if v is None:
                continue
            try:
                out[e.get("name", "")] = int(v, 0)
            except ValueError:
                pass
    return out


def find_structs(root: ET.Element) -> dict[str, ET.Element]:
    out: dict[str, ET.Element] = {}
    types_el = root.find("types")
    if types_el is None:
        return out
    for t in types_el.findall("type"):
        if t.get("category") not in ("struct", "union"):
            continue
        if t.get("alias"):
            continue
        nm = t.get("name")
        if nm:
            out[nm] = t
    return out


def parse_optional(attr: str | None) -> tuple[bool, ...]:
    if not attr:
        return ()
    parts = [p.strip().lower() for p in attr.split(",")]
    return tuple(p == "true" for p in parts)


def find_handles(root: ET.Element) -> dict[str, dict]:
    out: dict[str, dict] = {}
    types_el = root.find("types")
    if types_el is None:
        return out

    for t in types_el.findall("type"):
        if t.get("category") != "handle":
            continue
        if t.get("alias"):
            continue
        name_el = t.find("name")
        type_el = t.find("type")
        if name_el is None or type_el is None:
            continue
        name = (name_el.text or "").strip()
        macro = (type_el.text or "").strip()
        out[name] = {
            "dispatchable": macro == "VK_DEFINE_HANDLE",
            "parent": t.get("parent"),
        }
    return out


def find_command(root: ET.Element, name: str) -> ET.Element | None:
    commands_el = root.find("commands")
    if commands_el is None:
        return None

    for cmd in commands_el.findall("command"):
        if cmd.get("alias"):
            continue
        proto = cmd.find("proto")
        if proto is None:
            continue
        if proto.findtext("name") == name:
            return cmd

    for cmd in commands_el.findall("command"):
        if cmd.get("name") == name and cmd.get("alias"):
            return find_command(root, cmd.get("alias"))

    return None


def parse_codes(attr: str | None) -> tuple[str, ...]:
    if not attr:
        return ()
    return tuple(s.strip() for s in attr.split(",") if s.strip())


def _collect_commands_from_requires(parent: ET.Element) -> list[str]:
    out: list[str] = []
    for req in parent.findall("require"):
        for cmd in req.findall("command"):
            name = cmd.get("name")
            if name:
                out.append(name)
    return out


def _wants_api(api_attr: str | None) -> bool:
    if not api_attr:
        return True
    apis = {a.strip() for a in api_attr.split(",")}
    return "vulkan" in apis


def declared_identifiers(header_path) -> set[str]:
    if header_path is None:
        return set()
    text = Path(header_path).read_text(encoding="utf-8", errors="replace")

    kept: list[str] = []
    depth = 0
    for line in text.splitlines():
        stripped = line.strip()
        if depth:
            if stripped.startswith("#if"):
                depth += 1
            elif stripped.startswith("#endif"):
                depth -= 1
            continue
        if stripped.startswith("#if") and "VK_ENABLE_BETA_EXTENSIONS" in stripped:
            depth = 1
            continue
        kept.append(line)

    return set(re.findall(r"\b(?:Vk|VK_)[A-Za-z0-9_]+", "\n".join(kept)))


def _collect_types_from_requires(parent: ET.Element) -> set[str]:
    out: set[str] = set()
    for req in parent.findall("require"):
        if not _wants_api(req.get("api")):
            continue
        for t in req.findall("type"):
            name = t.get("name")
            if name:
                out.add(name)
    return out


def parse_feature_types(root: ET.Element) -> dict[str, set[str]]:
    out: dict[str, set[str]] = {}
    for feat in root.findall("feature"):
        if not _wants_api(feat.get("api")):
            continue
        name = feat.get("name", "")
        if name:
            out[name] = _collect_types_from_requires(feat)
    return out


def parse_extension_types(root: ET.Element) -> dict[str, set[str]]:
    out: dict[str, set[str]] = {}
    container = root.find("extensions")
    if container is None:
        return out
    for ext in container.findall("extension"):
        name = ext.get("name")
        if name:
            out[name] = _collect_types_from_requires(ext)
    return out


def parse_features(root: ET.Element) -> dict[str, list[str]]:
    out: dict[str, list[str]] = {}
    for feat in root.findall("feature"):
        if not _wants_api(feat.get("api")):
            continue
        name = feat.get("name", "")
        if not name:
            continue
        out[name] = _collect_commands_from_requires(feat)
    return out


_FEATURE_PREFIXES = (
    "VK_VERSION_",
    "VK_BASE_VERSION_",
    "VK_COMPUTE_VERSION_",
    "VK_GRAPHICS_VERSION_",
)


def parse_feature_number(name: str) -> tuple[int, int] | None:
    for prefix in _FEATURE_PREFIXES:
        if not name.startswith(prefix):
            continue
        parts = name.removeprefix(prefix).split("_")
        if len(parts) != 2 or not all(p.isdigit() for p in parts):
            return None
        return (int(parts[0]), int(parts[1]))
    return None


def parse_extensions(root: ET.Element) -> dict[str, dict]:
    out: dict[str, dict] = {}
    container = root.find("extensions")
    if container is None:
        return out
    for ext in container.findall("extension"):
        name = ext.get("name")
        if not name:
            continue
        out[name] = {
            "commands": _collect_commands_from_requires(ext),
            "depends": ext.get("depends", ""),
            "supported": ext.get("supported", ""),
            "platform": ext.get("platform"),
            "type": ext.get("type"),
        }
    return out
