#!/usr/bin/env python3
"""Build gate for the constitution's design and testing rules.

Principle III  - every unit of behaviour is a class that owns state:
                 no static member functions, no stateless classes.
Principle IV.5 - no exported class or public method without a test naming it.

Heuristic, deliberately: it reads headers rather than parsing C++. It is meant
to catch drift, and to be argued with in review when it is wrong.
"""
import os
import re
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
SRC_DIRS = ["src/core", "src/device", "src/proto"]

# Headers that declare a *required* interface - what core calls and the
# platform provides - rather than a *provided* one. Testing these would test
# the mock. Every entry needs a reason; the list is short on purpose.
EXCLUDE = {
    "src/core/platform.hpp": "the platform contract, implemented by src/km and "
                             "tests/harness and exercised through every core test",
}

CLASS_RE = re.compile(r"^\s*(?:class|struct)\s+([A-Z]\w*)\s*(?:final\s*)?(?:[:{]|$)")
METHOD_RE = re.compile(
    r"^\s*(?:\[\[nodiscard\]\]\s*)?"
    r"(?:virtual\s+|explicit\s+|constexpr\s+|inline\s+)*"
    r"(?:[\w:<>,\s*&]+?\s+)?"
    r"(\w+)\s*\([^;{]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?"
    r"(?:=\s*(?:0|default|delete)\s*)?;")
STATIC_METHOD_RE = re.compile(r"^\s*static\s+(?!constexpr\b|inline\s+constexpr\b)"
                              r"[\w:<>,\s*&]+\s+(\w+)\s*\(")
MEMBER_RE = re.compile(r"^\s*(?:mutable\s+)?[\w:<>,\s\[\]]+[\s*&]+(\w+_)\s*(?:=[^;]*)?;")
ACCESS_RE = re.compile(r"^\s*(private|protected|public)\s*:")
PURE_RE = re.compile(r"=\s*0\s*;")

def headers():
    for directory in SRC_DIRS:
        full = os.path.join(ROOT, directory)
        if not os.path.isdir(full):
            continue
        for base, _dirs, files in os.walk(full):
            for name in sorted(files):
                if name.endswith(".hpp"):
                    path = os.path.join(base, name)
                    yield path, os.path.relpath(path, ROOT)

def test_corpus():
    body = []
    tests = os.path.join(ROOT, "tests")
    for base, _dirs, files in os.walk(tests):
        if os.sep + "harness" in base:
            continue
        for name in files:
            if name.endswith((".cpp", ".hpp")):
                body.append(open(os.path.join(base, name)).read())
    return "\n".join(body)

def main():
    corpus = test_corpus()
    untested, statics, stateless = [], [], []

    for path, relative in headers():
        text = open(path).read()
        lines = text.split("\n")
        current, has_state, declared = None, False, []
        public, all_pure, method_count = True, True, 0

        def close_class():
            if current is None:
                return
            # A pure abstract contract has no state by definition; that is what
            # makes it a contract rather than a class pretending to be one.
            if not has_state and not (all_pure and method_count > 0):
                stateless.append(f"{relative}: class {current} has no data members")
            if relative in EXCLUDE:
                return
            if current not in corpus:
                untested.append(f"{relative}: class {current}")
            for method in declared:
                if not re.search(r"\b" + re.escape(method) + r"\b", corpus):
                    untested.append(f"{relative}: {current}::{method}")

        for line in lines:
            match = CLASS_RE.match(line)
            if match:
                close_class()
                current, has_state, declared = match.group(1), False, []
                public = line.lstrip().startswith("struct")
                all_pure, method_count = True, 0
                continue
            if current is None:
                continue
            access = ACCESS_RE.match(line)
            if access:
                public = access.group(1) == "public"
                continue
            if MEMBER_RE.match(line) and "(" not in line:
                has_state = True
            static_match = STATIC_METHOD_RE.match(line)
            if static_match:
                statics.append(f"{relative}: {current}::{static_match.group(1)}")
                continue
            method_match = METHOD_RE.match(line)
            if method_match:
                name = method_match.group(1)
                if name not in (current, "~" + current) and not name.startswith("operator"):
                    method_count += 1
                    if not PURE_RE.search(line):
                        all_pure = False
                    # Private methods are covered through the public surface;
                    # requiring a test to name them would test the shape of the
                    # implementation instead of its behaviour.
                    if public:
                        declared.append(name)
        close_class()

    failed = False
    if statics:
        failed = True
        print("Static member functions (constitution III):", file=sys.stderr)
        for entry in statics:
            print(f"  {entry}", file=sys.stderr)
    if stateless:
        failed = True
        print("Classes with no state (constitution III):", file=sys.stderr)
        for entry in stateless:
            print(f"  {entry}", file=sys.stderr)
    if untested:
        failed = True
        print("Untested classes or methods (constitution IV.5):", file=sys.stderr)
        for entry in untested:
            print(f"  {entry}", file=sys.stderr)
    if failed:
        sys.exit(1)
    print("design rules: ok")

if __name__ == "__main__":
    main()
