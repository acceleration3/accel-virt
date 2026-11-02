from __future__ import annotations

import filecmp
import re
import sys
import tempfile
from pathlib import Path
from typing import Callable

GENERATED_BANNER = """// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: cmake --build build --target regenerate-vulkan
//                  (or: python -m av_codegen --xml ... --out generated/)
"""


ARENA_ALLOCATION = re.compile(
    r"^(\s*)(?:auto|char|const char)\s*\*+\s*(\w+)\s*=\s*arena\.allocate\w*[<(].*;$")


def guard_arena_allocations(content: str) -> str:
    out: list[str] = []
    lines = content.split("\n")
    for number, line in enumerate(lines):
        out.append(line)
        matched = ARENA_ALLOCATION.match(line)
        if not matched:
            continue
        indent, name = matched.group(1), matched.group(2)
        following = lines[number + 1] if number + 1 < len(lines) else ""
        if f"{name} == nullptr" in following:
            continue
        out.append(f"{indent}if ({name} == nullptr) {{ return false; }}")
    return "\n".join(out)


def write(path: Path, content: str) -> None:
    content = guard_arena_allocations(content)
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(content, encoding="utf-8")
    tmp.replace(path)


def header_guard(content: str) -> str:
    return "#pragma once\n\n" + content


def is_handle(reg, type_name: str) -> bool:
    return type_name in reg.handles


def wire_handle_type() -> str:
    return "uint64_t"


class GeneratedTree:
    def __init__(self, out: Path):
        self.out = out

    def check(self, emit: Callable[[Path], None]) -> int:
        if not self.out.is_dir():
            print(f"error: --out does not exist: {self.out}", file=sys.stderr)
            return 1
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            emit(tmp)
            expected = sorted(p.name for p in tmp.iterdir())
            actual = sorted(p.name for p in self.out.iterdir() if not p.name.startswith("."))
            differ = expected != actual
            if differ:
                print(f"error: file set differs.\n  expected: {expected}\n  actual:   {actual}",
                      file=sys.stderr)
            for name in expected:
                if name in actual and not filecmp.cmp(tmp / name, self.out / name, shallow=False):
                    print(f"error: {name} differs from committed copy", file=sys.stderr)
                    differ = True
            return 1 if differ else 0


def missing_input(args, names: tuple[str, ...]) -> bool:
    missing = False
    for name in names:
        path = getattr(args, name)
        if not path.is_file():
            print(f"error: --{name.replace('_', '-')} does not exist: {path}", file=sys.stderr)
            missing = True
    return missing
