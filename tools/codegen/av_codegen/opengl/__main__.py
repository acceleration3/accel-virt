from __future__ import annotations

import argparse
import sys
from pathlib import Path

from . import emit, lower, parse, profile
from ..emit.common import GeneratedTree, missing_input

PROFILES = Path(__file__).resolve().parents[2] / "profiles"


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="av-codegen-gl")
    p.add_argument("--xml", type=Path, required=True, help="Path to gl.xml")
    p.add_argument("--wgl", type=Path, default=None, help="Path to wgl.xml")
    p.add_argument("--profile", type=Path, default=PROFILES / "opengl.toml")
    p.add_argument("--ids", type=Path, default=PROFILES / "opengl_ids.toml")
    p.add_argument("--proc-table", type=Path, default=PROFILES / "opengl_proc_table.txt",
                   help="opengl32's GLCLTPROCTABLE order (tools/re/gl_proc_table.py)")
    p.add_argument("--out", type=Path, required=True)
    p.add_argument(
        "--check",
        action="store_true",
        help="Exit non-zero if --out does not match what would be generated.",
    )
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)

    if missing_input(args, ("xml", "profile", "ids", "proc_table")):
        return 2
    try:
        proc_table = emit.read_proc_table(args.proc_table.read_text())
    except emit.GenerationError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    raw = parse.parse(args.xml, args.wgl)
    prof = profile.load(args.profile, args.ids)
    try:
        reg = lower.lower(raw, prof)
    except lower.LoweringError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    values: dict[str, int] = {}
    for name, text in raw.enums.items():
        try:
            values[name] = int(text, 0)
        except ValueError:
            continue

    try:
        if args.check:
            return GeneratedTree(args.out).check(lambda tmp: emit.emit_all(reg, tmp, values, proc_table))
        emit.emit_all(reg, args.out, values, proc_table)
    except emit.GenerationError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    prof.write_ids()
    asynchronous = sum(1 for c in reg.commands.values() if c.is_async)
    print(f"av-codegen-gl: {len(reg.commands)} commands "
          f"({asynchronous} of them one-way) -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
