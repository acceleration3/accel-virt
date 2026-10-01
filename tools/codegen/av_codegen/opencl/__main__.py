"""av-codegen for OpenCL: cl.xml in, the OpenCL half of the wire out.

``--check`` re-emits into a temporary directory and diffs against ``--out``;
that is what tools/check_generated.py runs, and it is why nothing under the
generated tree may be hand-edited.
"""

from __future__ import annotations

import argparse
import filecmp
import sys
import tempfile
from pathlib import Path

from . import emit, lower, parse, profile
from .model import Registry

PROFILES = Path(__file__).resolve().parents[2] / "profiles"


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="av-codegen-cl")
    p.add_argument("--xml", type=Path, required=True, help="Path to cl.xml")
    p.add_argument("--profile", type=Path, default=PROFILES / "opencl.toml",
                   help="Profile selecting commands and annotating their arrays.")
    p.add_argument("--ids", type=Path, default=PROFILES / "opencl_ids.toml",
                   help="Stable command id assignments.")
    p.add_argument("--out", type=Path, required=True,
                   help="Output directory; created if missing.")
    p.add_argument("--check", action="store_true",
                   help="Exit non-zero if --out does not match what would be generated.")
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)

    for name in ("xml", "profile", "ids"):
        path = getattr(args, name)
        if not path.is_file():
            print(f"error: --{name} does not exist: {path}", file=sys.stderr)
            return 2

    raw = parse.parse(args.xml)
    prof = profile.load(args.profile, args.ids)
    reg = lower.lower(raw, prof)

    if args.check:
        return _check(reg, raw, args.out)

    args.out.mkdir(parents=True, exist_ok=True)
    emit.emit_all(reg, args.out, raw)
    prof.write_ids()
    print(f"av-codegen-cl: {len(reg.commands)} commands -> {args.out}")
    return 0


def _check(reg: Registry, raw, out: Path) -> int:
    """Generate to a temp dir and diff the whole directory against ``out``."""
    if not out.is_dir():
        print(f"error: --out does not exist: {out}", file=sys.stderr)
        return 1
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        emit.emit_all(reg, tmp, raw)

        differ = False
        expected = sorted(p.name for p in tmp.iterdir())
        # Dotfiles alongside the output are configuration, not output.
        actual = sorted(p.name for p in out.iterdir() if not p.name.startswith("."))
        if expected != actual:
            print(f"error: file set differs.\n  expected: {expected}\n"
                  f"  actual:   {actual}", file=sys.stderr)
            differ = True
        for name in expected:
            if name not in actual:
                continue
            if not filecmp.cmp(tmp / name, out / name, shallow=False):
                print(f"error: {name} differs from committed copy", file=sys.stderr)
                differ = True
        return 1 if differ else 0


if __name__ == "__main__":
    raise SystemExit(main())
