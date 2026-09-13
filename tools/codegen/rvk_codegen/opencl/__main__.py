"""rvk-codegen for OpenCL: cl.xml in, the OpenCL half of the wire out."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from . import emit, lower, parse, profile


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="rvk-codegen-cl")
    p.add_argument("--xml", type=Path, required=True, help="Path to cl.xml")
    p.add_argument("--profile", type=Path,
                   default=Path(__file__).resolve().parents[2] / "cl_profile.toml")
    p.add_argument("--ids", type=Path,
                   default=Path(__file__).resolve().parents[2] / "cl_protocol_ids.toml")
    p.add_argument("--out", type=Path, required=True)
    args = p.parse_args(argv)

    if not args.xml.is_file():
        print(f"error: --xml does not exist: {args.xml}", file=sys.stderr)
        return 2

    raw = parse.parse(args.xml)
    prof = profile.load(args.profile, args.ids)
    reg = lower.lower(raw, prof)
    emit.emit_all(reg, args.out, raw)
    prof.write_ids()
    print(f"rvk-codegen-cl: {len(reg.commands)} commands -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
