from __future__ import annotations

import sys

from ..ir import Registry
from . import classify


def audit(reg: Registry) -> list[str]:
    problems: list[str] = []

    for name, struct in reg.structs.items():
        for member in struct.members:
            if not classify.is_machine_width(member.type):
                continue
            if classify.is_size_t(member.type):
                continue
            problems.append(f"struct {name}::{member.name} is {member.type.cpp()}")

    for name, cmd in reg.commands.items():
        for param in cmd.params:
            if not classify.is_machine_width(param.type):
                continue
            if classify.is_size_t(param.type):
                continue
            problems.append(f"command {name}({param.name}) is {param.type.cpp()}")

    return problems


def check(reg: Registry) -> None:
    problems = audit(reg)
    if not problems:
        return
    print(
        "codegen: these cross the wire with a width that differs between a "
        "32-bit guest and a 64-bit server, and nothing widens them:",
        file=sys.stderr,
    )
    for problem in problems:
        print(f"  {problem}", file=sys.stderr)
    print(
        "codegen: add them to classify.MACHINE_WIDTH_TYPES handling, or give "
        "them a bespoke encoding. Generating this would produce a protocol "
        "that works on one architecture and silently corrupts on the other.",
        file=sys.stderr,
    )
    raise SystemExit(2)
