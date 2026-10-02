"""Reads the classes declared in the portable source headers.

Heuristic, deliberately: it reads headers line by line rather than parsing C++.
It exists to catch drift, and to be argued with in review when it is wrong.
Shared by check_design.py and check_symbol_tests.py so both gates see the same
classes.
"""
import os
import re
from dataclasses import dataclass, field

SOURCE_DIRECTORIES = ("src/core", "src/miniport", "src/device", "src/proto", "src/guest", "src/icd")
# What a generator wrote is not this project's design and is not judged as such:
# the registries decide its shape, `tools/check_generated.py` decides that it is
# what the generator would write, and the compiler judges it like any other
# source (feature 063). Nothing hand-written may live here, which is what that
# gate's own test asserts.
GENERATED_DIRECTORY = "generated"


def is_generated(relative):
    """Whether a repository-relative path is under a generated directory."""
    return f"{os.sep}{GENERATED_DIRECTORY}{os.sep}" in f"{os.sep}{relative}"


# Constitution II: the core is portable, the shim is not. A file here does
# nothing but call the host's own loaders, so a unit test on a machine that may
# have no GPU cannot drive it; it is proven at L3 and L5 instead. The directory
# is the rule so that moving a file out of it puts the coverage gate back
# (feature 042).
SHIM_DIRECTORY = "shim"


def is_shim(relative):
    """Whether a repository-relative path is under a shim directory."""
    return f"{os.sep}{SHIM_DIRECTORY}{os.sep}" in f"{os.sep}{relative}"

CLASS_RE = re.compile(r"^\s*(?:class|struct)\s+([A-Z]\w*)\s*(?:final\s*)?(?:[:{]|$)")
METHOD_RE = re.compile(
    r"^\s*(?:\[\[nodiscard\]\]\s*)?"
    r"(?:virtual\s+|explicit\s+|constexpr\s+|inline\s+)*"
    # The return type may end in the declarator's * or &: `void *name(`.
    r"(?:[\w:<>,\s*&]+?[\s*&]+)?"
    r"(\w+)\s*\([^;{]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?"
    r"(?:=\s*(?:0|default|delete)\s*)?;")
STATIC_METHOD_RE = re.compile(r"^\s*static\s+(?!constexpr\b|inline\s+constexpr\b)"
                              r"[\w:<>,\s*&]+\s+(\w+)\s*\(")
MEMBER_RE = re.compile(r"^\s*(?:mutable\s+)?[\w:<>,\s\[\]]+[\s*&]+(\w+_)\s*(?:\[[^\]]*\])*\s*(?:=[^;]*)?;")
ACCESS_RE = re.compile(r"^\s*(private|protected|public)\s*:")
PURE_RE = re.compile(r"=\s*0\s*;")
RECORD_DIRECTORIES = ("src/miniport/ddi/", "src/icd/d3d/ddi/")
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
    # Declared in a private section of another class: part of that class's
    # implementation, reached only through its public surface.
    is_private_nested: bool = False

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
                    relative = os.path.relpath(path, root)
                    # Filtered by path rather than by pruning `dirs`: this walk
                    # is materialised by sorted(), so pruning would come too
                    # late to skip anything.
                    if is_generated(relative):
                        continue
                    yield path, relative


def declared_classes(root):
    for path, relative in headers(root):
        with open(path) as header:
            yield from classes_in(header.read().split("\n"), relative)


def classes_in(lines, relative):
    """Only lines at the class body's own brace level declare members; a call
    inside an inline body (templates must define in the header) does not. A
    type declared in a class body is read as a class of its own, and the
    enclosing class's reading resumes after it: a table of complete nested
    slots is declared after the slot type, and its members are the class's
    state all the same."""
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
            # A DDI record overlays a structure its caller owns - dxgkrnl's, or a
            # Direct3D runtime's - so its fields keep the WDK's names.
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
