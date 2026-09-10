#!/usr/bin/env python3
"""Constitution III.5: a new exported core function without a test fails the
build. Every non-static function declared in a src/core, src/device or
src/proto header must be named by at least one file under tests/."""
import os, re, sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
SRC_DIRS = ["src/core", "src/device", "src/proto"]

# Headers that declare a *required* interface — functions core calls and the
# platform provides — rather than a *provided* one. Testing these would test
# the mock, not the driver. Every entry needs a reason; the list is short on
# purpose and adding to it is a review conversation.
EXCLUDE = {
    "src/core/platform.h": "the platform contract; implemented by src/km and "
                           "tests/harness, exercised through every core test",
}
DECL = re.compile(r"^\s*(?:[A-Za-z_][\w \t*]*?)\b(av_[a-z0-9_]+)\s*\(", re.M)

exported = {}
for d in SRC_DIRS:
    full = os.path.join(ROOT, d)
    if not os.path.isdir(full):
        continue
    for root, _dirs, files in os.walk(full):
        for f in files:
            if not f.endswith(".h"):
                continue
            p = os.path.join(root, f)
            if os.path.relpath(p, ROOT) in EXCLUDE:
                continue
            for m in DECL.finditer(open(p).read()):
                exported.setdefault(m.group(1), os.path.relpath(p, ROOT))

tested = set()
tdir = os.path.join(ROOT, "tests")
for root, _dirs, files in os.walk(tdir):
    if os.sep + "harness" in root:
        continue
    for f in files:
        if f.endswith((".c", ".h")):
            body = open(os.path.join(root, f)).read()
            for name in exported:
                if name in body:
                    tested.add(name)

missing = sorted(set(exported) - tested)
if missing:
    print("Untested exported symbols (constitution III.5):", file=sys.stderr)
    for m in missing:
        print(f"  {m}   declared in {exported[m]}", file=sys.stderr)
    sys.exit(1)
print(f"ok: {len(exported)} exported symbol(s), all referenced by tests")
