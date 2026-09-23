"""rvk-codegen for Direct3D: this project's D3D registry in, the D3D half of the
wire out.

``--xml`` names the registry for symmetry with the other generators and with
tools/check_generated.py, which passes every registry that way; it is TOML.
``--check`` re-emits into a temporary directory and diffs against ``--out``.
"""

from __future__ import annotations

import argparse
import filecmp
import sys
import tempfile
from pathlib import Path

from . import emit, model


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="rvk-codegen-d3d")
    p.add_argument("--xml", type=Path, required=True, help="Path to the D3D registry (TOML)")
    p.add_argument("--out", type=Path, required=True, help="Output directory; created if missing.")
    p.add_argument("--check", action="store_true",
                   help="Exit non-zero if --out does not match what would be generated.")
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if not args.xml.is_file():
        print(f"error: --xml does not exist: {args.xml}", file=sys.stderr)
        return 2
    try:
        reg = model.load(args.xml)
    except model.RegistryError as error:
        print(f"error: {args.xml}: {error}", file=sys.stderr)
        return 2
    if args.check:
        return _check(reg, args.out)
    args.out.mkdir(parents=True, exist_ok=True)
    emit.emit_all(reg, args.out)
    print(f"rvk-codegen-d3d: {len(reg.commands)} commands, {len(reg.structs)} structs -> {args.out}")
    return 0


def _check(reg: model.Registry, out: Path) -> int:
    if not out.is_dir():
        print(f"error: --out does not exist: {out}", file=sys.stderr)
        return 1
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        emit.emit_all(reg, tmp)
        expected = sorted(p.name for p in tmp.iterdir())
        actual = sorted(p.name for p in out.iterdir() if not p.name.startswith("."))
        differ = expected != actual
        if differ:
            print(f"error: file set differs.\n  expected: {expected}\n  actual:   {actual}", file=sys.stderr)
        for name in expected:
            if name in actual and not filecmp.cmp(tmp / name, out / name, shallow=False):
                print(f"error: {name} differs from committed copy", file=sys.stderr)
                differ = True
        return 1 if differ else 0


if __name__ == "__main__":
    raise SystemExit(main())
