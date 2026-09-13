#!/usr/bin/env python3
"""The documented id space and the generator's id files say the same thing.

docs/protocol.md gives every command id's top 16 bits a meaning - which API
owns it - and tools/codegen/profiles/*_ids.toml is what the generator will
actually emit. A regeneration that moves a command into another API's range,
or an API nobody documented, is a wire change that the diff would not make
obvious; this is what makes it obvious (feature 063, FR-007).

Ids never move. Two commands sharing one is the same bug seen from the guest.
"""
import argparse
import pathlib
import re
import sys
import tomllib

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_DOC = ROOT / "docs" / "protocol.md"
DEFAULT_PROFILES = ROOT / "tools" / "codegen" / "profiles"

# "top 16 bits: the API - 0 transport, 1 Vulkan, 2 OpenCL, 3 OpenGL"
SPACE = re.compile(r"top 16 bits: the API - (.+?)\s*\|")
MEMBER = re.compile(r"(\d+)\s+(\w+)")
API_BITS = 16


def documented(doc):
    """Every API the document gives a range to, as {name lowered: high bits}.

    The transport owns range 0 and generates nothing, so it is not an id file
    anyone is missing.
    """
    found = SPACE.search(doc.read_text())
    if found is None:
        raise SystemExit(f"protocol ids: {doc} no longer says what a command id's top bits mean")
    space = {name.lower(): int(number) for number, name in MEMBER.findall(found.group(1))}
    space.pop("transport", None)
    if not space:
        raise SystemExit(f"protocol ids: {doc} names no API that generates commands")
    return space


def violations(space, profiles):
    """Everything the two disagree about, one line each."""
    said = []
    files = sorted(profiles.glob("*_ids.toml"))
    for path in files:
        api = path.name.removesuffix("_ids.toml")
        if api not in space:
            said.append(f"{path.name}: no API named {api} in the protocol document")
            continue
        commands = tomllib.loads(path.read_text()).get("commands", {})
        seen = {}
        for name, value in sorted(commands.items()):
            if value >> API_BITS != space[api]:
                said.append(f"{path.name}: {name} = 0x{value:x} is not in {api}'s range "
                            f"0x{space[api] << API_BITS:x}")
            if value in seen:
                said.append(f"{path.name}: {name} and {seen[value]} share 0x{value:x}")
            seen[value] = name
    have = {path.name.removesuffix("_ids.toml") for path in files}
    for api in sorted(space):
        if api not in have:
            said.append(f"{api}: the protocol document gives it range "
                        f"0x{space[api] << API_BITS:x} and no id file claims it")
    return said


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--doc", type=pathlib.Path, default=DEFAULT_DOC)
    parser.add_argument("--profiles", type=pathlib.Path, default=DEFAULT_PROFILES)
    arguments = parser.parse_args(argv)

    space = documented(arguments.doc)
    said = violations(space, arguments.profiles)
    for line in said:
        print(f"protocol ids: {line}", file=sys.stderr)
    if said:
        return 1
    for api in sorted(space):
        commands = tomllib.loads((arguments.profiles / f"{api}_ids.toml").read_text())
        print(f"protocol ids: {api} owns 0x{space[api] << API_BITS:x}, "
              f"{len(commands.get('commands', {}))} commands")
    return 0


if __name__ == "__main__":
    sys.exit(main())
