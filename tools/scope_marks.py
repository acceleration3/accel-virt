#!/usr/bin/env python3
"""Gate and writer for the trace scope marks (docs/trace.md): every function
definition in src/ opens with AV_SCOPE("<Class::function>") naming itself.

  scope_marks.py [--check] [root]   report unmarked and misnamed definitions
  scope_marks.py --apply [root]     write the missing marks and fix misnamed ones
  scope_marks.py --names [root]     print "<id> <name>" for every mark in src/

Definitions are found with libclang rather than a pattern, because the ones a
pattern misreads - constructors with initializer lists, operators, templates in
headers, bodies opened on the signature's line - are exactly the ones that
would go unmarked. The mark itself is read as text: a tree under test need not
define AV_SCOPE for its body to be judged.
"""
import argparse
import concurrent.futures
import json
import os
import pathlib
import re
import shlex
import subprocess
import sys

import clang.cindex as ci

INCLUDE = '#include "core/trace/function_scope.hpp"'
MARK = re.compile(r'\s*AV_SCOPE\(\s*"([^"]*)"\s*\)\s*;')
MARK_ANYWHERE = re.compile(r'AV_SCOPE\(\s*"([^"]*)"\s*\)')
QUOTED_INCLUDE = re.compile(r'^#include "([^"]+)"')
INDENT = "    "
FNV_OFFSET = 2166136261
FNV_PRIME = 16777619

# The table's own code, and every TraceHost it calls into (a `*_trace_host.cpp`
# and the clock one reads), would recurse into itself: a scope's enter asks the
# host for the thread, whose scope enters again. The layout proofs are data
# the kernel build checks at compile time; generated code is its generator's.
EXCLUDED_DIRECTORIES = ("src/core/trace/", "src/proto/generated/")
EXCLUDED_FILES = ("src/km/ddi_record_layout.cpp", "src/km/ddi_table_layout.cpp",
                  "src/um/trace/interrupt_clock.cpp")
EXCLUDED_SUFFIXES = ("_trace_host.cpp", "_trace_host.hpp")
SOURCE_SUFFIXES = (".cpp", ".hpp")
DEFINITION_KINDS = {
    ci.CursorKind.FUNCTION_DECL, ci.CursorKind.CXX_METHOD, ci.CursorKind.CONSTRUCTOR,
    ci.CursorKind.DESTRUCTOR, ci.CursorKind.FUNCTION_TEMPLATE, ci.CursorKind.CONVERSION_FUNCTION,
}
SCOPE_KINDS = {
    ci.CursorKind.STRUCT_DECL, ci.CursorKind.CLASS_DECL, ci.CursorKind.UNION_DECL,
    ci.CursorKind.CLASS_TEMPLATE, ci.CursorKind.CLASS_TEMPLATE_PARTIAL_SPECIALIZATION,
}
# Descended into for definitions; a function body never is, so a lambda or a
# local class inside one is never taken for a definition of its own.
CONTAINER_KINDS = SCOPE_KINDS | {ci.CursorKind.NAMESPACE, ci.CursorKind.LINKAGE_SPEC, ci.CursorKind.UNEXPOSED_DECL}


def scope_id(name):
    value = FNV_OFFSET
    for byte in name.encode():
        value = ((value ^ byte) * FNV_PRIME) & 0xFFFFFFFF
    return value


def resource_flags():
    """libclang does not find the compiler's own headers (stddef.h) by itself."""
    try:
        found = subprocess.run(["clang++", "-print-resource-dir"], capture_output=True, text=True, check=True)
    except (OSError, subprocess.CalledProcessError):
        return []
    return ["-resource-dir", found.stdout.strip()]


def fallback_flags(root, relative):
    flags = ["-x", "c++", "-std=c++23", "-fsyntax-only", f"-I{root}/src", f"-I{root}/tests",
             f"-I{root}/third_party/khronos/include", f"-I{root}/src/proto/generated/opengl",
             "-DAV_CHECKED=1"]
    # The Windows shims include the kit through the case-insensitive overlay the
    # cross-builds use (cmake/windows_*_flags.cmake), with those builds' paths.
    kit = f"{root}/third_party/windows-kit"
    windows = ["--target=x86_64-pc-windows-msvc", "-fms-extensions", "-fms-compatibility",
               "-fms-compatibility-version=19.40", "-nostdlibinc", "-D_AMD64_", "-DAMD64", "-D_WIN64",
               f"-ivfsoverlay{kit}/overlay.yaml", "-isystem/windows-kit/km/crt", "-isystem/windows-kit/sdk-shared"]
    if relative.startswith("src/km/"):
        flags += windows + ["-isystem/windows-kit/km", "-isystem/windows-kit/wdk-shared", "-D_KERNEL_MODE"]
    elif relative.startswith("src/um/"):
        flags += windows + ["-isystem/windows-kit/um", "-DWIN32_LEAN_AND_MEAN", "-DNOMINMAX"]
    return flags


def compile_flags(root):
    """Each listed source's own flags, less what names the compiler and the output."""
    database = pathlib.Path(root) / "build" / "compile_commands.json"
    if not database.exists():
        return {}
    flags = {}
    for entry in json.loads(database.read_text()):
        words = shlex.split(entry["command"]) if "command" in entry else list(entry["arguments"])
        kept, skip = [], False
        for word in words[1:]:
            if skip:
                skip = False
                continue
            if word in ("-o", "-c", "-MF", "-MT", "-MQ"):
                skip = word != "-c"
                continue
            if word == entry["file"] or word.startswith("-fsanitize") or word == "-Werror":
                continue
            kept.append(word)
        flags[os.path.realpath(os.path.join(entry["directory"], entry["file"]))] = kept
    return flags


def sources(root):
    base = pathlib.Path(root) / "src"
    for path in sorted(base.rglob("*")):
        relative = path.relative_to(root).as_posix()
        if path.suffix not in SOURCE_SUFFIXES or not path.is_file():
            continue
        if (relative.startswith(EXCLUDED_DIRECTORIES) or relative in EXCLUDED_FILES
                or relative.endswith(EXCLUDED_SUFFIXES)):
            continue
        yield path, relative


def qualified_name(cursor):
    """The name as written at the definition, classes included, namespaces not
    - anonymous ones neither, so a file's own helper is its bare name."""
    spelling = cursor.spelling
    # A class template's constructor spells its arguments; the definition does not.
    if cursor.kind in (ci.CursorKind.CONSTRUCTOR, ci.CursorKind.DESTRUCTOR):
        spelling = re.sub(r"<.*>$", "", spelling)
    parts = [spelling]
    parent = cursor.semantic_parent
    while parent is not None and parent.kind != ci.CursorKind.TRANSLATION_UNIT:
        if parent.kind in SCOPE_KINDS:
            parts.append(parent.spelling)
        parent = parent.semantic_parent
    return "::".join(reversed(parts))


def body_of(cursor):
    body = None
    for child in cursor.get_children():
        if child.kind == ci.CursorKind.COMPOUND_STMT:
            body = child
    return body


def exempt(cursor, text, body_start):
    """constexpr and constinit, and C linkage: the one-line ABI entry points."""
    head = text[cursor.extent.start.offset:body_start]
    if re.search(r"\b(constexpr|constinit)\b", head):
        return True
    if re.search(r'\bextern\s+"C"', head):
        return True
    lexical = cursor.lexical_parent
    while lexical is not None and lexical.kind != ci.CursorKind.TRANSLATION_UNIT:
        if lexical.kind == ci.CursorKind.LINKAGE_SPEC:
            return True
        lexical = lexical.lexical_parent
    if cursor.kind == ci.CursorKind.FUNCTION_DECL:
        mangled = cursor.mangled_name or ""
        return mangled == cursor.spelling
    return False


class Definition:
    def __init__(self, name, line, open_at, close_at, exempt_from_marks):
        self.name = name
        self.line = line
        self.open_at = open_at
        self.close_at = close_at
        self.exempt = exempt_from_marks


def definitions(path, flags):
    """(Definition, ...) of the file's own function definitions, by byte offset."""
    # latin-1 maps each byte to one character, so libclang's byte offsets index
    # the text directly and a write gives back every byte it did not change.
    text = path.read_bytes().decode("latin-1")
    index = ci.Index.create()
    unit = index.parse(str(path), args=flags,
                       options=ci.TranslationUnit.PARSE_INCOMPLETE)
    own = os.path.realpath(path)
    found = {}

    def visit(cursor):
        for child in cursor.get_children():
            location = child.location
            if location.file is None or os.path.realpath(location.file.name) != own:
                continue
            if child.kind in DEFINITION_KINDS and child.is_definition():
                take(child)
            elif child.kind in CONTAINER_KINDS:
                visit(child)

    def take(cursor):
        body = body_of(cursor)
        if body is None or body.extent.start.file is None:
            return
        open_at = body.extent.start.offset
        close_at = body.extent.end.offset - 1
        # A body a macro expanded has no brace of its own to put a mark after.
        if text[open_at:open_at + 1] != "{" or text[close_at:close_at + 1] != "}":
            return
        found[open_at] = Definition(qualified_name(cursor), cursor.location.line, open_at, close_at,
                                    exempt(cursor, text, open_at))

    visit(unit.cursor)
    # A fatal error ends the parse where it stands, and every definition after
    # it would pass unseen.
    fatal = [str(diagnostic.spelling) for diagnostic in unit.diagnostics
             if diagnostic.severity >= ci.Diagnostic.Fatal]
    return text, sorted(found.values(), key=lambda definition: definition.open_at), fatal


def judge(text, relative, found, fatal):
    """Findings for one file, and the marks it holds."""
    findings, marks = [f"{relative}: cannot be parsed: {message}" for message in fatal], []
    for definition in found:
        mark = MARK.match(text, definition.open_at + 1)
        where = f"{relative}:{definition.line}"
        if definition.exempt:
            if mark:
                findings.append(f"{where}: {definition.name} is exempt and must not be marked")
            continue
        if mark is None:
            findings.append(f"{where}: {definition.name} has no scope mark")
            continue
        marks.append(mark.group(1))
        if mark.group(1) != definition.name:
            findings.append(f'{where}: {definition.name} is marked "{mark.group(1)}"')
    if marks and INCLUDE not in text:
        findings.append(f"{relative}: marks without {INCLUDE}")
    return findings, marks


def line_bounds(text, offset):
    start = text.rfind("\n", 0, offset) + 1
    end = text.find("\n", offset)
    return start, len(text) if end < 0 else end


def indent_of(text, line_start):
    return re.match(r"[ \t]*", text[line_start:]).group(0)


def edit(text, definition):
    """The text with this definition's mark written or corrected."""
    mark = MARK.match(text, definition.open_at + 1)
    literal = f'AV_SCOPE("{definition.name}");'
    if mark:
        if mark.group(1) == definition.name:
            return text
        start, end = mark.span(1)
        return text[:start] + definition.name + text[end:]
    open_at, close_at = definition.open_at, definition.close_at
    line_start, line_end = line_bounds(text, open_at)
    indent = indent_of(text, line_start)
    after = text[open_at + 1:line_end]
    if after.strip() == "":
        return text[:open_at + 1] + "\n" + indent + INDENT + literal + text[open_at + 1:]
    if close_at < line_end:
        # A body on one line: brace, mark, statements and brace each on their own.
        before = text[line_start:open_at].rstrip()
        inside = text[open_at + 1:close_at].strip()
        tail = text[close_at + 1:line_end]
        lines = []
        if before.strip():
            lines.append(before)
        lines.append(indent + "{")
        lines.append(indent + INDENT + literal)
        if inside:
            lines.append(indent + INDENT + inside)
        lines.append(indent + "}" + tail)
        return text[:line_start] + "\n".join(lines) + text[line_end:]
    return (text[:open_at + 1] + "\n" + indent + INDENT + literal + "\n" + indent + INDENT + after.lstrip()
            + text[line_end:])


def add_include(text, relative):
    if INCLUDE in text:
        return text
    lines = text.split("\n")
    own_stem = pathlib.PurePosixPath(relative).stem
    blocks, current = [], []
    for number, line in enumerate(lines):
        if QUOTED_INCLUDE.match(line):
            current.append(number)
        elif current:
            blocks.append(current)
            current = []
    if current:
        blocks.append(current)

    def is_own(number):
        included = QUOTED_INCLUDE.match(lines[number]).group(1)
        return relative.endswith(".cpp") and pathlib.PurePosixPath(included).stem == own_stem \
            and included.endswith(".hpp")

    if not blocks:
        # No project include yet: before the first system one, or after the guard.
        at = next((n for n, line in enumerate(lines) if line.startswith("#include")), None)
        guard = next((n + 1 for n, line in enumerate(lines) if line.startswith("#define")), None)
        if at is not None:
            lines[at:at] = [INCLUDE, ""]
        elif guard is not None:
            lines[guard:guard] = ["", INCLUDE]
        else:
            lines[0:0] = [INCLUDE, ""]
        return "\n".join(lines)
    block = blocks[0]
    if len(block) == 1 and is_own(block[0]):
        if len(blocks) == 1:
            lines[block[0] + 1:block[0] + 1] = ["", INCLUDE]
            return "\n".join(lines)
        block = blocks[1]
    candidates = [n for n in block if not is_own(n)]
    target = INCLUDE[len('#include "'):-1]
    at = next((n for n in candidates if QUOTED_INCLUDE.match(lines[n]).group(1) > target), None)
    if at is None:
        at = (candidates or block)[-1] + 1
    lines.insert(at, INCLUDE)
    return "\n".join(lines)


def examine(job):
    root, path, relative, flags, apply = job
    text, found, fatal = definitions(path, flags)
    if apply and not fatal:
        edited = text
        for definition in reversed(found):
            if not definition.exempt:
                edited = edit(edited, definition)
        if edited != text:
            edited = add_include(edited, relative)
            path.write_bytes(edited.encode("latin-1"))
            text, found, fatal = definitions(path, flags)
    findings, marks = judge(text, relative, found, fatal)
    return relative, findings, marks


def all_marks(root):
    """Every mark in src/, generated code included, by name."""
    names = set()
    for path in sorted((pathlib.Path(root) / "src").rglob("*")):
        if path.suffix in SOURCE_SUFFIXES + (".h", ".cc") and path.is_file():
            names.update(MARK_ANYWHERE.findall(path.read_text(errors="replace")))
    names.discard("")
    return names


def collisions(names):
    seen, found = {}, []
    for name in sorted(names):
        other = seen.setdefault(scope_id(name), name)
        if other != name:
            found.append(f'scope id {scope_id(name):08x} is both "{other}" and "{name}"')
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--apply", action="store_true")
    mode.add_argument("--names", action="store_true")
    parser.add_argument("root", nargs="?", default=".")
    args = parser.parse_args()
    root = os.path.realpath(args.root)
    if args.names:
        for name in sorted(all_marks(root), key=lambda n: (scope_id(n), n)):
            print(f"{scope_id(name):08x} {name}")
        return 0
    known = compile_flags(root)
    resources = resource_flags()
    jobs = [(root, path, relative,
             known.get(os.path.realpath(path), fallback_flags(root, relative)) + resources, args.apply)
            for path, relative in sources(root)]
    findings = []
    with concurrent.futures.ProcessPoolExecutor() as pool:
        for relative, found, _marks in pool.map(examine, jobs, chunksize=4):
            findings += found
    findings += collisions(all_marks(root))
    for finding in findings:
        print(finding, file=sys.stderr)
    if findings:
        print(f"scope_marks: {len(findings)} findings", file=sys.stderr)
        return 1
    print("scope_marks: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
