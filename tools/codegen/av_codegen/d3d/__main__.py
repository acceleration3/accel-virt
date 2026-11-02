from __future__ import annotations

import argparse
import sys
from pathlib import Path

from . import emit, model
from ..emit.common import GeneratedTree, missing_input


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="av-codegen-d3d")
    p.add_argument("--xml", type=Path, required=True, help="Path to the D3D registry (TOML)")
    p.add_argument("--out", type=Path, required=True, help="Output directory; created if missing.")
    p.add_argument("--check", action="store_true",
                   help="Exit non-zero if --out does not match what would be generated.")
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if missing_input(args, ("xml",)):
        return 2
    try:
        reg = model.load(args.xml)
    except model.RegistryError as error:
        print(f"error: {args.xml}: {error}", file=sys.stderr)
        return 2
    if args.check:
        return GeneratedTree(args.out).check(lambda tmp: emit.emit_all(reg, tmp))
    args.out.mkdir(parents=True, exist_ok=True)
    emit.emit_all(reg, args.out)
    print(f"av-codegen-d3d: {len(reg.commands)} commands, {len(reg.structs)} structs -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
