"""Reads the classes declared in the portable source headers.

Heuristic, deliberately: it reads headers line by line rather than parsing C++.
It exists to catch drift, and to be argued with in review when it is wrong.
Shared by check_design.py and check_symbol_tests.py so both gates see the same
classes.
"""
import os
import re
from dataclasses import dataclass, field

SOURCE_DIRECTORIES = ("src/core", "src/device", "src/proto")

CLASS_RE = re.compile(r"^\s*(?:class|struct)\s+([A-Z]\w*)\s*(?:final\s*)?(?:[:{]|$)")
METHOD_RE = re.compile(
    r"^\s*(?:\[\[nodiscard\]\]\s*)?"
    r"(?:virtual\s+|explicit\s+|constexpr\s+|inline\s+)*"
    r"(?:[\w:<>,\s*&]+?\s+)?"
    r"(\w+)\s*\([^;{]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?"
    r"(?:=\s*(?:0|default|delete)\s*)?;")
STATIC_METHOD_RE = re.compile(r"^\s*static\s+(?!constexpr\b|inline\s+constexpr\b)"
                              r"[\w:<>,\s*&]+\s+(\w+)\s*\(")
MEMBER_RE = re.compile(r"^\s*(?:mutable\s+)?[\w:<>,\s\[\]]+[\s*&]+(\w+_)\s*(?:\[[^\]]*\])*\s*(?:=[^;]*)?;")
ACCESS_RE = re.compile(r"^\s*(private|protected|public)\s*:")
PURE_RE = re.compile(r"=\s*0\s*;")
RECORD_DIRECTORY = "src/core/ddi/"
RECORD_FIELD_RE = re.compile(r"^\s*[\w:]+(?:\s*\*+\s*|\s+)[A-Za-z_]\w*(?:\[\w+\])?\s*;")


@dataclass
class DeclaredClass:
    name: str
    header: str
    has_state: bool = False
    all_pure: bool = True
    method_count: int = 0
    public_methods: list = field(default_factory=list)
    static_methods: list = field(default_factory=list)
    is_record: bool = False

    def is_contract(self):
        """A pure abstract interface has no state by definition."""
        return self.all_pure and self.method_count > 0


def headers(root):
    for directory in SOURCE_DIRECTORIES:
        full = os.path.join(root, directory)
        for base, _dirs, files in sorted(os.walk(full)):
            for name in sorted(files):
                if name.endswith(".hpp"):
                    path = os.path.join(base, name)
                    yield path, os.path.relpath(path, root)


def declared_classes(root):
    for path, relative in headers(root):
        with open(path) as header:
            yield from classes_in(header.read().split("\n"), relative)


def classes_in(lines, relative):
    """Only lines at the class body's own brace level declare members; a call
    inside an inline body (templates must define in the header) does not."""
    current, public, depth, opened = None, True, 0, False
    for line in lines:
        match = CLASS_RE.match(line)
        if match:
            if current:
                yield current
            current = DeclaredClass(match.group(1), relative)
            public = line.lstrip().startswith("struct")
            # A DDI record overlays a WDK structure, so its fields keep the WDK's names.
            current.is_record = (public and relative.startswith(RECORD_DIRECTORY)
                                 and current.name.endswith("Record"))
            depth = line.count("{") - line.count("}")
            opened = depth > 0
            continue
        if current is None:
            continue
        at_body_level = depth == 1
        depth += line.count("{") - line.count("}")
        if depth > 0:
            opened = True
        elif opened:
            yield current
            current = None
            continue
        if not at_body_level:
            continue
        access = ACCESS_RE.match(line)
        if access:
            public = access.group(1) == "public"
            continue
        if MEMBER_RE.match(line) and "(" not in line:
            current.has_state = True
        if current.is_record and RECORD_FIELD_RE.match(line):
            current.has_state = True
        static_match = STATIC_METHOD_RE.match(line)
        if static_match:
            current.static_methods.append(static_match.group(1))
            continue
        method_match = METHOD_RE.match(line)
        if not method_match:
            continue
        name = method_match.group(1)
        if name in (current.name, "~" + current.name) or name.startswith("operator"):
            continue
        current.method_count += 1
        if not PURE_RE.search(line):
            current.all_pure = False
        if public:
            current.public_methods.append(name)
    if current:
        yield current
