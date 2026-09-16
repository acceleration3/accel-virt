#!/usr/bin/env python3
"""Microsoft's OpenGL 1.1 dispatch-table order, read from opengl32.dll.

`DrvSetContext` answers opengl32 a table of function pointers in the order of
opengl32's own `glCltRGBAProcTable` (feature 050, research.md §2): a u32 entry
count, four bytes of padding, then one pointer per entry, each to a `glclt*`
function. This reads that table from the pinned binary and its public PDB in
the reverse-engineering workspace (tools/re/build_workspace.sh) and writes the
order as data the OpenGL generator reads:

    # opengl32.dll sha256 <hash>
    0 glNewList
    ...
    335 glPushClientAttrib

Identical-code folding can leave two `glclt*` names at one address (on
10.0.26100.5074, `glcltFinish` and `glcltFlush` are slots 216 and 217 of one
function). Such a slot is named by the exported `gl*` stub that dispatches to
it: the stub loads the thread's table and jumps through `offset(%rax)`, and the
offset is the slot's.

`--check` compares the committed file with the binary, and says it skipped when
the workspace has no binary or PDB to read.
"""
import argparse
import hashlib
import pathlib
import re
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from check_guest_image import Malformed, PeImage  # noqa: E402

TABLE_SYMBOL = "glCltRGBAProcTable"
ENTRIES = 336
CLIENT_PREFIX = "glclt"
DEFAULT_BINARY = ROOT / "re" / "bin" / "opengl32.dll"
DEFAULT_OUT = ROOT / "tools" / "codegen" / "profiles" / "opengl_proc_table.txt"
PUBLIC = re.compile(r"S_PUB32 \[size = \d+\] `([^`]+)`\s+flags = [^\n]*?addr = (\d+):(\d+)")
POINTER_SIZE = 8
ENTRIES_AT = 8
STUB_BYTES = 64
TABLE_LOAD = re.compile(r"movq\s+\(%rcx,%rax\), %rax")
SLOT_LOAD = re.compile(r"movq\s+0x([0-9a-f]+)\(%rax\), %rax")


class TableError(Exception):
    """What makes a table, read from a binary or a file, not the order."""


class ProcTable:
    """The order: entry names without the client prefix, index by position."""

    def __init__(self, binary_hash, names):
        if len(names) != ENTRIES:
            raise TableError(f"{len(names)} entries, not {ENTRIES}")
        seen = set()
        for index, name in enumerate(names):
            if not name.startswith("gl"):
                raise TableError(f"entry {index} is {name!r}, which names no GL function")
            if name in seen:
                raise TableError(f"entry {index} names {name} twice")
            seen.add(name)
        self.binary_hash = binary_hash
        self.names = list(names)

    def render(self):
        lines = [f"# opengl32.dll sha256 {self.binary_hash}"]
        lines += [f"{index} {name}" for index, name in enumerate(self.names)]
        return "\n".join(lines) + "\n"


class ProcTableFile:
    """The committed order, read back line by line."""

    def __init__(self, text):
        self.text = text

    def table(self):
        lines = self.text.splitlines()
        if not lines or not lines[0].startswith("# opengl32.dll sha256 "):
            raise TableError("no header naming the binary's hash")
        binary_hash = lines[0].split()[-1]
        names = []
        for number, line in enumerate(lines[1:], 2):
            parts = line.split()
            if len(parts) != 2 or not parts[0].isdigit():
                raise TableError(f"line {number}: want '<index> <name>', got {line!r}")
            if int(parts[0]) != len(names):
                raise TableError(f"line {number}: index {parts[0]} where {len(names)} was due")
            names.append(parts[1])
        return ProcTable(binary_hash, names)


class BinaryProcTable:
    """The order as one opengl32.dll holds it, located through its PDB."""

    def __init__(self, binary, pdb):
        self.binary = pathlib.Path(binary)
        self.pdb = pathlib.Path(pdb)

    def available(self):
        return self.binary.is_file() and self.pdb.is_file()

    def table(self):
        data = self.binary.read_bytes()
        try:
            image = PeImage(data)
            publics = self.publics(image)
            if TABLE_SYMBOL not in publics:
                raise TableError(f"the PDB names no {TABLE_SYMBOL}")
            at = publics[TABLE_SYMBOL]
            count = image.u32_at(at)
            if count != ENTRIES:
                raise TableError(f"{TABLE_SYMBOL} holds {count} entries, not {ENTRIES}")
            functions = {}
            for name, rva in publics.items():
                functions.setdefault(rva, []).append(name)
            base = self.image_base(data)
            names = [self.entry_name(functions, publics, base,
                                     image.thunk_at(at + ENTRIES_AT + POINTER_SIZE * index) - base, index)
                     for index in range(count)]
        except Malformed as error:
            raise TableError(f"{self.binary}: {error}") from error
        return ProcTable(hashlib.sha256(data).hexdigest(), names)

    def publics(self, image):
        dump = subprocess.run(["llvm-pdbutil", "dump", "--publics", str(self.pdb)],
                              capture_output=True, text=True)
        if dump.returncode != 0:
            raise TableError(f"{self.pdb}: llvm-pdbutil refused it: {dump.stderr.strip()}")
        found = {}
        for match in PUBLIC.finditer(dump.stdout):
            section = int(match.group(2))
            if 1 <= section <= len(image.sections):
                found[match.group(1)] = image.sections[section - 1][0] + int(match.group(3))
        return found

    def image_base(self, data):
        optional = struct.unpack_from("<I", data, 0x3C)[0] + 24
        return struct.unpack_from("<Q", data, optional + 24)[0]

    def entry_name(self, functions, publics, base, rva, index):
        named = functions.get(rva)
        if named is None:
            raise TableError(f"entry {index} points at RVA 0x{rva:X}, which no public symbol names")
        candidates = sorted(name for name in named if name.startswith(CLIENT_PREFIX))
        if not candidates:
            raise TableError(f"entry {index} is {sorted(named)[0]}, not a {CLIENT_PREFIX} function")
        if len(candidates) == 1:
            return "gl" + candidates[0][len(CLIENT_PREFIX):]
        for candidate in candidates:
            exported = "gl" + candidate[len(CLIENT_PREFIX):]
            if exported in publics and self.stub_slot(base + publics[exported]) == index:
                return exported
        raise TableError(f"entry {index} is one of {', '.join(candidates)}, and no exported stub "
                         "dispatches through it")

    def stub_slot(self, address):
        """The slot a `gl*` export jumps through, or None for another shape."""
        listing = subprocess.run(["llvm-objdump", "-d", "--no-show-raw-insn",
                                  f"--start-address=0x{address:X}",
                                  f"--stop-address=0x{address + STUB_BYTES:X}", str(self.binary)],
                                 capture_output=True, text=True).stdout.splitlines()
        for number, line in enumerate(listing):
            if TABLE_LOAD.search(line) and number + 1 < len(listing):
                slot = SLOT_LOAD.search(listing[number + 1])
                if slot and int(slot.group(1), 16) % POINTER_SIZE == 0:
                    return int(slot.group(1), 16) // POINTER_SIZE
        return None

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--binary", type=pathlib.Path, default=DEFAULT_BINARY)
    parser.add_argument("--pdb", type=pathlib.Path, help="default: the binary's sibling .pdb")
    parser.add_argument("--out", type=pathlib.Path, default=DEFAULT_OUT)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args(argv)
    source = BinaryProcTable(arguments.binary, arguments.pdb or arguments.binary.with_suffix(".pdb"))
    if not source.available():
        if arguments.check:
            print(f"gl_proc_table: skipped: {arguments.binary} or its PDB is absent "
                  "(tools/re/build_workspace.sh fetches them)")
            return 0
        print(f"gl_proc_table: {arguments.binary} or its PDB is absent "
              "(tools/re/build_workspace.sh fetches them)", file=sys.stderr)
        return 1
    try:
        table = source.table()
        if not arguments.check:
            arguments.out.write_text(table.render())
            print(f"gl_proc_table: wrote {ENTRIES} entries to {arguments.out}")
            return 0
        if not arguments.out.is_file():
            print(f"gl_proc_table: {arguments.out} is missing; run without --check", file=sys.stderr)
            return 1
        committed = ProcTableFile(arguments.out.read_text()).table()
    except TableError as error:
        print(f"gl_proc_table: {error}", file=sys.stderr)
        return 1
    if committed.render() != table.render():
        differing = [index for index, (a, b) in enumerate(zip(committed.names, table.names)) if a != b]
        where = f"entry {differing[0]}" if differing else "the header"
        print(f"gl_proc_table: {arguments.out} differs from {arguments.binary} at {where}", file=sys.stderr)
        return 1
    print(f"gl_proc_table: {arguments.out} matches {arguments.binary}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
