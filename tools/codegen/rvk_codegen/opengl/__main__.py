"""rvk-codegen for OpenGL: gl.xml and wgl.xml in, the GL half of the wire out."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from . import emit, lower, parse, profile


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="rvk-codegen-gl")
    p.add_argument("--xml", type=Path, required=True, help="Path to gl.xml")
    p.add_argument("--wgl", type=Path, default=None, help="Path to wgl.xml")
    p.add_argument("--profile", type=Path,
                   default=Path(__file__).resolve().parents[2] / "gl_profile.toml")
    p.add_argument("--ids", type=Path,
                   default=Path(__file__).resolve().parents[2] / "gl_protocol_ids.toml")
    p.add_argument("--out", type=Path, required=True)
    args = p.parse_args(argv)

    if not args.xml.is_file():
        print(f"error: --xml does not exist: {args.xml}", file=sys.stderr)
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
    emit.emit_all(reg, args.out, values)
    prof.write_ids()
    asynchronous = sum(1 for c in reg.commands.values() if c.is_async)
    print(f"rvk-codegen-gl: {len(reg.commands)} commands "
          f"({asynchronous} of them one-way) -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
