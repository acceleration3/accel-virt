from __future__ import annotations

import argparse
import sys
from pathlib import Path

from . import lower, parse, profile
from .emit import ALL_BACKENDS, width_audit
from .emit.common import GeneratedTree, missing_input


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="av-codegen",
        description="Generate AV serialization code from Khronos vk.xml.",
    )
    p.add_argument("--xml", type=Path, required=True, help="Path to Vulkan-Headers/registry/vk.xml")
    p.add_argument(
        "--profile",
        type=Path,
        default=Path(__file__).resolve().parent.parent / "profile.toml",
        help="Path to a profile.toml selecting commands/extensions.",
    )
    p.add_argument(
        "--ids",
        type=Path,
        default=Path(__file__).resolve().parent.parent / "protocol_ids.toml",
        help="Path to a protocol_ids.toml mapping commands to wire IDs.",
    )
    p.add_argument(
        "--header",
        type=Path,
        default=None,
        help="Path to the vulkan_core.h the output will be compiled against. "
             "pNext chain structs absent from it are not emitted; without "
             "this the registry's superset is trusted and the result may "
             "not compile.",
    )
    p.add_argument("--out", type=Path, required=True, help="Output directory; created if missing.")
    p.add_argument(
        "--check",
        action="store_true",
        help="Exit non-zero if --out does not match what would be generated.",
    )
    p.add_argument("--verbose", "-v", action="store_true", help="Verbose progress output.")
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)

    if missing_input(args, ("xml", "profile", "ids")):
        return 2

    if args.verbose:
        print("av-codegen v0.0.1")
        print(f"  xml:     {args.xml}")
        print(f"  profile: {args.profile}")
        print(f"  ids:     {args.ids}")
        print(f"  out:     {args.out}")

    prof = profile.load(args.profile, args.ids)
    if args.verbose:
        print(f"  api_version: {prof.api_version[0]}.{prof.api_version[1]}")
        print(f"  extensions:  {len(prof.extensions)} ({', '.join(prof.extensions)})")

    root = parse.load_xml(args.xml)
    reg = lower.lower(root, prof, parse.declared_identifiers(args.header))

    if args.verbose:
        print(f"  commands lowered: {len(reg.commands)}")

    width_audit.check(reg)

    if args.check:
        return GeneratedTree(args.out).check(lambda tmp: [backend.emit(reg, tmp) for backend in ALL_BACKENDS])

    args.out.mkdir(parents=True, exist_ok=True)

    profile.write_ids(prof)

    for backend in ALL_BACKENDS:
        if args.verbose:
            print(f"  emit {backend.__name__.rsplit('.', 1)[-1]}")
        backend.emit(reg, args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
