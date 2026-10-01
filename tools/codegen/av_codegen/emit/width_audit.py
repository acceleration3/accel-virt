"""Refuse to generate a wire encoding whose width depends on the machine.

This is not a backend. It produces no file. It reads what the other
backends are about to encode and fails the build if any of it would be
written with a size that differs between a 32-bit guest and a 64-bit
server.

It exists because that single mistake has been made three times in this
codebase — ``size_t`` in struct members, ``size_t`` in command
parameters, and dispatchable handles — and each time it was fixed as an
instance rather than as a class. It costs an afternoon every time,
because of how it fails: the field itself is not reported. Its width
shifts every field after it, so the receiver rejects the whole command
and names the command; or worse, the fields still parse and the values
are quietly wrong. The last occurrence rejected 264,753 calls while the
application was told every one of them had succeeded, and the frames
came out blank rather than broken.

The generator already widens the types it knows about. This checks that
the set it knows about is the whole set — so the next type with this
property fails here, at generation, naming the field, rather than in a
guest three weeks later.
"""

from __future__ import annotations

import sys

from ..ir import Registry
from . import classify


def audit(reg: Registry) -> list[str]:
    """Every wire-crossing field whose width is the machine's.

    Returns the ones that are NOT handled — an empty list means the
    encoding is width-stable.
    """
    problems: list[str] = []

    for name, struct in reg.structs.items():
        for member in struct.members:
            if not classify.is_machine_width(member.type):
                continue
            # These are widened explicitly by the serializer; anything
            # else of this shape is not, and is what this looks for.
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
