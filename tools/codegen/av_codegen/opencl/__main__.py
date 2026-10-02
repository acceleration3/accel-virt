"""av-codegen for OpenCL: cl.xml in, the OpenCL half of the wire out.

``--check`` re-emits into a temporary directory and diffs against ``--out``;
that is what tools/check_generated.py runs, and it is why nothing under the
generated tree may be hand-edited.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from . import emit, lower, parse, profile
from ..emit.common import GeneratedTree, missing_input

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

    if missing_input(args, ("xml", "profile", "ids")):
        return 2

    raw = parse.parse(args.xml)
    prof = profile.load(args.profile, args.ids)
    reg = lower.lower(raw, prof)

    if args.check:
        return GeneratedTree(args.out).check(lambda tmp: emit.emit_all(reg, tmp, raw))

    args.out.mkdir(parents=True, exist_ok=True)
    emit.emit_all(reg, args.out, raw)
    prof.write_ids()
    print(f"av-codegen-cl: {len(reg.commands)} commands -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
