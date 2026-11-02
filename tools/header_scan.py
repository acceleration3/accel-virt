import os
import re
from dataclasses import dataclass, field

SOURCE_DIRECTORIES = ("src/core", "src/miniport", "src/device", "src/executor", "src/proto", "src/guest", "src/icd")
GENERATED_DIRECTORY = "generated"


def is_generated(relative):
    return f"{os.sep}{GENERATED_DIRECTORY}{os.sep}" in f"{os.sep}{relative}"


SHIM_DIRECTORY = "shim"


def is_shim(relative):
    return f"{os.sep}{SHIM_DIRECTORY}{os.sep}" in f"{os.sep}{relative}"

CLASS_RE = re.compile(r"^\s*(?:class|struct)\s+([A-Z]\w*)\s*(?:final\s*)?(?:[:{]|$)")
METHOD_RE = re.compile(
    r"^\s*(?:\[\[nodiscard\]\]\s*)?"
    r"(?:virtual\s+|explicit\s+|constexpr\s+|inline\s+)*"
    r"(?:[\w:<>,\s*&]+?[\s*&]+)?"
    r"(\w+)\s*\([^;{]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?"
    r"(?:=\s*(?:0|default|delete)\s*)?;")
STATIC_METHOD_RE = re.compile(r"^\s*static\s+(?!constexpr\b|inline\s+constexpr\b)"
                              r"[\w:<>,\s*&]+\s+(\w+)\s*\(")
MEMBER_RE = re.compile(r"^\s*(?:mutable\s+)?[\w:<>,\s\[\]]+[\s*&]+(\w+_)\s*(?:\[[^\]]*\])*\s*(?:=[^;]*)?;")
ACCESS_RE = re.compile(r"^\s*(private|protected|public)\s*:")
PURE_RE = re.compile(r"=\s*0\s*;")
RECORD_DIRECTORIES = ("src/miniport/ddi/", "src/icd/d3d/ddi/", "src/icd/vendor/ddi/")
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
    is_private_nested: bool = False

    def is_contract(self):
        return self.all_pure and self.method_count > 0


def headers(root):
    for directory in SOURCE_DIRECTORIES:
        full = os.path.join(root, directory)
        for base, _dirs, files in sorted(os.walk(full)):
            for name in sorted(files):
                if name.endswith(".hpp"):
                    path = os.path.join(base, name)
                    relative = os.path.relpath(path, root)
                    if is_generated(relative):
                        continue
                    yield path, relative


def declared_classes(root):
    for path, relative in headers(root):
        with open(path) as header:
            yield from classes_in(header.read().split("\n"), relative)


def classes_in(lines, relative):
    current, public, depth, opened = None, True, 0, False
    enclosing = []
    for line in lines:
        match = CLASS_RE.match(line)
        if match:
            nested = current is not None and depth == 1
            if current and not nested:
                yield current
            if nested:
                enclosing.append((current, public, depth, opened))
            current = DeclaredClass(match.group(1), relative)
            current.is_private_nested = nested and not public
            public = line.lstrip().startswith("struct")
            current.is_record = (public and relative.startswith(RECORD_DIRECTORIES)
                                 and current.name.endswith("Record"))
            depth = line.count("{") - line.count("}")
            opened = depth > 0
            if nested and not opened and "}" in line:
                body = line[line.index("{") + 1:line.rindex("}")] if "{" in line else ""
                current.has_state = any(MEMBER_RE.match(f"{piece};") for piece in body.split(";") if piece.strip())
                yield current
                current, public, depth, opened = enclosing.pop()
            continue
        if current is None:
            continue
        at_body_level = depth == 1
        depth += line.count("{") - line.count("}")
        if depth > 0:
            opened = True
        elif opened:
            yield current
            current, public, depth, opened = enclosing.pop() if enclosing else (None, True, 0, False)
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
    for unclosed, _public, _depth, _opened in reversed(enclosing):
        yield unclosed
