"""Helpers shared by emitters."""

from __future__ import annotations

import re
from pathlib import Path

GENERATED_BANNER = """// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: cmake --build build --target regenerate-vulkan
//                  (or: python -m av_codegen --xml ... --out generated/)
"""


ARENA_ALLOCATION = re.compile(
    r"^(\s*)(?:auto|char|const char)\s*\*+\s*(\w+)\s*=\s*arena\.allocate\w*[<(].*;$")


def guard_arena_allocations(content: str) -> str:
    """A refused arena allocation is a null pointer, and the next emitted line
    writes through it.

    The arena is a fixed block the host gives a decode, so a frame describing
    more than fits is a frame the decoder must refuse - not one it writes past
    the block for. Rather than a guard beside each of the twenty-odd emission
    sites, where a new one would forget it, every emitted allocation gets the
    guard here. Found by the round-trip test in
    tests/unit/proto/generated/test_generated_vulkan.cpp, which asks for a
    string longer than the arena it hands over.
    """
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
    """Write atomically; only touches the file if content differs."""
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
    """All Vulkan handles travel as 64-bit on the wire (see docs/protocol.md)."""
    return "uint64_t"
