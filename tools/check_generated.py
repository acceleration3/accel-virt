#!/usr/bin/env python3
"""The committed generated sources are what the generators write.

Every emitted tree is re-generated into a temporary directory and compared.
A file that differs, a file nobody would have written, or a file that vanished
fails the gate naming it - which is what makes "never hand-edit the output"
a rule rather than a hope.

Without the fetched registries there is nothing to generate from, and a
developer who has not fetched them has broken nothing: the gate says it skipped
and how to stop it skipping.

The plan is data, in tools/generated_sources.txt, one tree per line:

    <api> <generator-module-or-script> <registry> <out> [extra arguments...]
"""
import argparse
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_PLAN = ROOT / "tools" / "generated_sources.txt"
FETCH = "tools/fetch_khronos_registries.py"


def plan_from(path):
    """Every tree in `path`: (api, generator, registry, out, extra)."""
    trees = []
    for number, line in enumerate(path.read_text().splitlines(), 1):
        text = line.strip()
        if not text or text.startswith("#"):
            continue
        parts = text.split()
        if len(parts) < 4:
            raise SystemExit(f"{path}:{number}: want '<api> <generator> <registry> <out> ...'")
        trees.append((parts[0], parts[1], parts[2], parts[3], parts[4:]))
    return trees


def command_for(generator, registry, out, extra):
    """A module is run with -m, a script by path; the generator's own --check
    is what does the comparing."""
    if generator.endswith(".py"):
        argv = [sys.executable, str(ROOT / generator)]
    else:
        argv = [sys.executable, "-m", generator]
    argv += ["--xml", str(ROOT / registry), "--out", str(ROOT / out), "--check"]
    return argv + [argument.replace("@", str(ROOT) + "/") for argument in extra]


def check(api, generator, registry, out, extra):
    """Whether this tree matches, and what to say about it."""
    environment = {"PYTHONPATH": str(ROOT / "tools" / "codegen")}
    result = subprocess.run(command_for(generator, registry, out, extra), capture_output=True,
                            text=True, cwd=str(ROOT), env={**_environ(), **environment})
    if result.returncode == 0:
        return True, f"generated: {api} matches {out}"
    detail = (result.stderr.strip() or result.stdout.strip()).splitlines()
    return False, f"generated: {api} differs from {out}\n" + "\n".join(f"  {line}" for line in detail)


def _environ():
    import os
    return dict(os.environ)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--plan", type=pathlib.Path, default=DEFAULT_PLAN)
    arguments = parser.parse_args(argv)

    trees = plan_from(arguments.plan)
    missing = sorted({registry for _api, _generator, registry, _out, _extra in trees
                      if not (ROOT / registry).is_file()})
    if missing:
        print(f"generated: skipped, no {', '.join(missing)}; run {FETCH} to check them")
        return 0

    verdicts = [check(*tree) for tree in trees]
    for passed, message in verdicts:
        print(message) if passed else print(message, file=sys.stderr)
    if all(passed for passed, _message in verdicts):
        return 0
    print("generated: fix the emitter and regenerate; never edit what it wrote", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
