"""rvk-codegen for OpenGL: gl.xml and wgl.xml in, the GL half of the wire out."""

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
    p = argparse.ArgumentParser(prog="rvk-codegen-gl")
    p.add_argument("--xml", type=Path, required=True, help="Path to gl.xml")
    p.add_argument("--wgl", type=Path, default=None, help="Path to wgl.xml")
    p.add_argument("--profile", type=Path, default=PROFILES / "opengl.toml")
    p.add_argument("--ids", type=Path, default=PROFILES / "opengl_ids.toml")
    p.add_argument("--out", type=Path, required=True)
    p.add_argument(
        "--check",
        action="store_true",
        help="Exit non-zero if --out does not match what would be generated.",
    )
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)

    if not args.xml.is_file():
        print(f"error: --xml does not exist: {args.xml}", file=sys.stderr)
        return 2
    if not args.profile.is_file():
        print(f"error: --profile does not exist: {args.profile}", file=sys.stderr)
        return 2
    if not args.ids.is_file():
        print(f"error: --ids does not exist: {args.ids}", file=sys.stderr)
        return 2

    raw = parse.parse(args.xml, args.wgl)
    prof = profile.load(args.profile, args.ids)
    try:
        reg = lower.lower(raw, prof)
    except lower.LoweringError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    # The enum values the size tables are written with. They are in the
    # registry as text, in whatever base the extension author wrote.
    values: dict[str, int] = {}
    for name, text in raw.enums.items():
        try:
            values[name] = int(text, 0)
        except ValueError:
            continue

    if args.check:
        return _check(reg, values, args.out)

    emit.emit_all(reg, args.out, values)
    prof.write_ids()
    asynchronous = sum(1 for c in reg.commands.values() if c.is_async)
    print(f"rvk-codegen-gl: {len(reg.commands)} commands "
          f"({asynchronous} of them one-way) -> {args.out}")
    return 0


def _check(reg: Registry, values: dict[str, int], out: Path) -> int:
    """Generate to a temp dir and diff the whole directory against ``out``."""
    if not out.is_dir():
        print(f"error: --out does not exist: {out}", file=sys.stderr)
        return 1
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        emit.emit_all(reg, tmp, values)

        differ = False
        expected = sorted(p.name for p in tmp.iterdir())
        # Dotfiles are configuration that lives alongside the output,
        # not output: generated/.clang-format is what stops an editor
        # reformatting these files and breaking this very check.
        actual = sorted(p.name for p in out.iterdir() if not p.name.startswith("."))
        if expected != actual:
            print(
                f"error: file set differs.\n  expected: {expected}\n  actual:   {actual}",
                file=sys.stderr,
            )
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
