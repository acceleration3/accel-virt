import bisect
import json
import pathlib
import re
import subprocess
import sys
import urllib.request

SERVER = "https://msdl.microsoft.com/download/symbols"
CALLING_CONVENTIONS = ("__cdecl ", "__stdcall ", "__fastcall ", "__thiscall ", "__vectorcall ")


def short_name(demangled):
    text = demangled
    for convention in CALLING_CONVENTIONS:
        position = text.rfind(convention)
        if position >= 0:
            text = text[position + len(convention):]
            break
    depth = 0
    for index, char in enumerate(text):
        if char == "<":
            depth += 1
        elif char == ">":
            depth -= 1
        elif char == "(" and depth == 0:
            return text[:index].strip()
    return text.strip()


def demangle(names):
    mangled = [name for name in names if name.startswith("?")]
    result = {name: name for name in names}
    if not mangled:
        return result
    output = subprocess.run(["llvm-undname"], input="\n".join(mangled) + "\n", capture_output=True, text=True).stdout
    blocks = [block.split("\n") for block in output.strip("\n").split("\n\n")]
    for block in blocks:
        if len(block) >= 2 and block[0] in result and not block[1].startswith("error"):
            result[block[0]] = short_name(block[1])
    return result


class PublicTable:
    def __init__(self, entries):
        self.entries = sorted(entries)
        self.rvas = [rva for rva, _ in self.entries]

    @classmethod
    def from_pdb(cls, pdb):
        sections = [int(value, 16) for value in re.findall(
            r"^\s+([0-9A-F]+) virtual address", subprocess.run(
                ["llvm-pdbutil", "dump", "-section-headers", str(pdb)], capture_output=True, text=True, errors="replace").stdout,
            flags=re.MULTILINE)]
        dump = subprocess.run(["llvm-pdbutil", "dump", "-publics", str(pdb)], capture_output=True, text=True,
                              errors="replace").stdout
        raw = []
        for match in re.finditer(r"`([^`]+)`\n\s+flags = ([^,\n]*), addr = (\d+):(\d+)", dump):
            if "function" not in match.group(2):
                continue
            section, offset = int(match.group(3)), int(match.group(4))
            if 1 <= section <= len(sections):
                raw.append((sections[section - 1] + offset, match.group(1)))
        names = demangle({name for _, name in raw})
        return cls([(rva, names[name]) for rva, name in raw])

    def name(self, rva):
        index = bisect.bisect_right(self.rvas, rva) - 1
        return self.entries[index][1] if index >= 0 else None


class Symbols:
    def __init__(self, kernel, local=(), cache=None, offline=False):
        self.kernel = kernel
        self.local = [pathlib.Path(folder) for folder in local]
        self.cache = pathlib.Path(cache) if cache else None
        self.offline = offline
        self.named = {}
        self.tables = {}
        self.local_pdbs = {}

    def name(self, pid, address):
        self.resolve(pid, [address])
        return self.named[(self.space(pid, address), address)]

    def space(self, pid, address):
        return 0 if address >= 1 << 63 else pid

    def resolve(self, pid, addresses):
        by_image = {}
        for address in addresses:
            key = (self.space(pid, address), address)
            if key in self.named:
                continue
            image = self.kernel.image_at(pid, address)
            if image is None:
                self.named[key] = "?"
                continue
            by_image.setdefault((key[0], image.base), (image, []))[1].append(address)
        for (space, base), (image, wanted) in by_image.items():
            module = image.path.replace("/", "\\").split("\\")[-1]
            offsets = {address: address - image.base for address in wanted}
            functions = self.name_offsets(space, image, module, sorted(set(offsets.values())))
            for address, offset in offsets.items():
                function = functions.get(offset)
                self.named[(space, address)] = f"{module}!{function}" if function else f"{module}+{offset:#x}"

    def name_offsets(self, space, image, module, offsets):
        pdb_id = self.kernel.pdbs.get((space, image.base))
        local = self.local_binary(module, pdb_id)
        if local is not None:
            return self.symbolize(local, offsets)
        table = self.table(pdb_id)
        if table is None:
            return {}
        return {offset: table.name(offset) for offset in offsets}

    def local_binary(self, module, pdb_id):
        if module not in self.local_pdbs:
            self.local_pdbs[module] = None
            for folder in self.local:
                binary = folder / module
                pdb = binary.with_suffix(".pdb")
                if binary.exists() and pdb.exists():
                    summary = subprocess.run(["llvm-pdbutil", "dump", "-summary", str(pdb)], capture_output=True,
                                             text=True).stdout
                    guid = re.search(r"GUID: \{([0-9A-F-]+)\}", summary)
                    self.local_pdbs[module] = (binary, guid.group(1).replace("-", "") if guid else "")
                    break
        found = self.local_pdbs[module]
        if found is None or (pdb_id is not None and found[1] and found[1] != pdb_id[1]):
            return None
        return found[0]

    def symbolize(self, binary, offsets):
        headers = subprocess.run(["llvm-readobj", "--file-headers", str(binary)], capture_output=True, text=True).stdout
        image_base = int(re.search(r"ImageBase: (0x[0-9A-Fa-f]+)", headers).group(1), 16)
        answers = subprocess.run(["llvm-symbolizer", "--obj", str(binary), "--functions=short", "--no-inlines"] +
                                 [hex(image_base + offset) for offset in offsets],
                                 capture_output=True, text=True).stdout.split("\n\n")
        named = {}
        for offset, answer in zip(offsets, answers):
            function = answer.strip().split("\n")[0]
            named[offset] = None if function in ("", "??") else function
        return named

    def table(self, pdb_id):
        if pdb_id is None or self.cache is None:
            return None
        path, guid, age = pdb_id
        name = path.replace("/", "\\").split("\\")[-1]
        key = f"{guid}{age:X}"
        if key in self.tables:
            return self.tables[key]
        folder = self.cache / name.lower() / key
        saved = folder / "publics.json"
        table = None
        if saved.exists():
            table = PublicTable([tuple(entry) for entry in json.loads(saved.read_text())])
        else:
            pdb = self.fetch(folder, name, key)
            if pdb is not None:
                table = PublicTable.from_pdb(pdb)
                saved.write_text(json.dumps(table.entries))
        self.tables[key] = table
        return table

    def fetch(self, folder, name, key):
        pdb = folder / name
        if pdb.exists():
            return pdb
        if self.offline:
            return None
        folder.mkdir(parents=True, exist_ok=True)
        try:
            with urllib.request.urlopen(f"{SERVER}/{name}/{key}/{name}", timeout=120) as reply:
                pdb.write_bytes(reply.read())
        except OSError as error:
            print(f"no symbols for {name} {key}: {error}", file=sys.stderr)
            return None
        return pdb
