#!/usr/bin/env python3
"""Gate for constitution Principle IV.1: 100% branch coverage of src/core,
src/device and src/proto - headers included - and an AV_UNTESTABLE mark count
that only moves down.

Run by the coverage_gate ctest after every unit test has written its profile.

  check_coverage.py <build dir>
"""
import glob
import json
import os
import re
import subprocess
import sys

SOURCE_DIRECTORIES = ("src/core/", "src/device/", "src/proto/")
BASELINE = "tests/untestable_baseline.txt"
MARK_RE = re.compile(r'AV_UNTESTABLE\(\s*"([^"]*)"')


class CoverageVerdict:
    """Judges one llvm-cov export against the source tree it was built from."""

    def __init__(self, root, export, binaries, profile_dir):
        self.root = os.path.realpath(str(root))
        self.export = export
        self.binaries = binaries
        self.profile_dir = str(profile_dir)
        self.notes_ = []

    def violations(self):
        self.notes_ = []
        return (self.missing_profiles() + self.unlinked() + self.uncovered()
                + self.untestable_marks())

    def notes(self):
        return self.notes_

    def missing_profiles(self):
        return [f"{name}: no profile written" for name in self.binaries
                if not os.path.isfile(os.path.join(self.profile_dir, f"{name}.profraw"))]

    def sources(self, suffixes):
        found = []
        for directory in SOURCE_DIRECTORIES:
            for base, _dirs, files in sorted(os.walk(os.path.join(self.root, directory))):
                found += [os.path.join(base, name) for name in sorted(files)
                          if name.endswith(suffixes)]
        return found

    def unlinked(self):
        """A source no test references is dropped by the linker, so it never
        reaches the coverage data and would otherwise pass unmeasured."""
        measured = {self.source_path(r["filename"]) for r in self.export["data"][0]["files"]}
        return [f"{os.path.relpath(path, self.root)}: not linked into any test binary"
                for path in self.sources((".cpp",))
                if os.path.relpath(path, self.root) not in measured]

    def source_path(self, filename):
        relative = os.path.relpath(os.path.realpath(filename), self.root)
        return relative if relative.startswith(SOURCE_DIRECTORIES) else None

    def uncovered(self):
        lines, covered, total = [], 0, 0
        for record in self.export["data"][0]["files"]:
            relative = self.source_path(record["filename"])
            if relative is None:
                continue
            summary = record["summary"]["branches"]
            covered += summary["covered"]
            total += summary["count"]
            lines += self.untaken(relative, record.get("branches", []))
            for expansion in record.get("expansions", []):
                lines += self.untaken(relative, expansion.get("branches", []))
        if total and covered < total:
            lines.append(f"branch coverage {100.0 * covered / total:.2f}% < 100%")
        self.notes_.append(f"branch coverage {100.0 * covered / total if total else 100.0:.2f}%")
        return lines

    @staticmethod
    def untaken(relative, branches):
        lines = []
        for line, column, _end_line, _end_column, true_count, false_count, *_rest in branches:
            if true_count == 0:
                lines.append(f"{relative}:{line}:{column}: true branch never taken")
            if false_count == 0:
                lines.append(f"{relative}:{line}:{column}: false branch never taken")
        return lines

    def untestable_marks(self):
        marks = []
        for path in self.sources((".cpp", ".hpp")):
            marks += self.marks_in(path)
        with open(os.path.join(self.root, BASELINE)) as baseline_file:
            baseline = int(baseline_file.read().strip())
        self.notes_.append(f"AV_UNTESTABLE marks {len(marks)} (baseline {baseline})")
        if len(marks) < baseline:
            self.notes_.append(f"lower {BASELINE} to {len(marks)}")
        if len(marks) <= baseline:
            return []
        return [f"AV_UNTESTABLE marks {len(marks)} > baseline {baseline}"] + marks

    def marks_in(self, path):
        relative = os.path.relpath(path, self.root)
        found = []
        with open(path) as source:
            for number, text in enumerate(source, 1):
                if text.lstrip().startswith("#define"):
                    continue
                for match in MARK_RE.finditer(text):
                    found.append(f"{relative}:{number}: {match.group(1)}")
        return found


def llvm_export(build, binaries):
    profiles = sorted(glob.glob(os.path.join(build, "coverage", "*.profraw")))
    merged = os.path.join(build, "coverage", "all.profdata")
    subprocess.run(["llvm-profdata", "merge", "-sparse", *profiles, "-o", merged], check=True)
    objects = [os.path.join(build, name) for name in binaries]
    command = ["llvm-cov", "export", objects[0], f"-instr-profile={merged}"]
    for extra in objects[1:]:
        command += ["-object", extra]
    return json.loads(subprocess.run(command, check=True, capture_output=True,
                                     text=True).stdout)


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: check_coverage.py <build dir>")
    build = sys.argv[1]
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    binaries = sorted(os.path.basename(p) for p in glob.glob(os.path.join(build, "test_*"))
                      if os.access(p, os.X_OK) and os.path.isfile(p))
    if not binaries:
        print(f"{build}: no test binaries", file=sys.stderr)
        sys.exit(1)
    profile_dir = os.path.join(build, "coverage")
    verdict = CoverageVerdict(root, {"data": [{"files": []}]}, binaries, profile_dir)
    missing = verdict.missing_profiles()
    if not missing:
        verdict = CoverageVerdict(root, llvm_export(build, binaries), binaries, profile_dir)
    violations = missing or verdict.violations()
    for violation in violations:
        print(violation, file=sys.stderr)
    if violations:
        sys.exit(1)
    print("coverage_gate: ok, " + "; ".join(verdict.notes()))


if __name__ == "__main__":
    main()
