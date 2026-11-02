#!/usr/bin/env python3
"""Gate: a guest binary is x64 and binds only to the user-mode modules every"""
import argparse
import struct
import sys

MACHINE_I386 = 0x014C
MACHINE_AMD64 = 0x8664
MACHINES = {MACHINE_I386: "IMAGE_FILE_MACHINE_I386", MACHINE_AMD64: "IMAGE_FILE_MACHINE_AMD64",
            0xAA64: "IMAGE_FILE_MACHINE_ARM64"}
MACHINE_NAMES = {"amd64": MACHINE_AMD64, "i386": MACHINE_I386}
SUBSYSTEM_WINDOWS_GUI = 2
SUBSYSTEM_WINDOWS_CUI = 3
SUBSYSTEMS = {1: "IMAGE_SUBSYSTEM_NATIVE", SUBSYSTEM_WINDOWS_GUI: "IMAGE_SUBSYSTEM_WINDOWS_GUI",
              SUBSYSTEM_WINDOWS_CUI: "IMAGE_SUBSYSTEM_WINDOWS_CUI",
              10: "IMAGE_SUBSYSTEM_EFI_APPLICATION"}
FILE_DLL = 0x2000
GUEST_MODULES = {"gdi32.dll", "kernel32.dll", "ntdll.dll"}

PE_SIGNATURE = b"PE\0\0"
OPTIONAL_PE32 = 0x10B
OPTIONAL_PE32_PLUS = 0x20B
EXPORT_DIRECTORY = 0
IMPORT_DIRECTORY = 1
RELOCATION_DIRECTORY = 5
DELAY_IMPORT_DIRECTORY = 13
RELOCATION_BLOCK_HEADER = 8
RELOCATED_HIGHLOW = 3
RELOCATED_DIR64 = 10
KNOWN_DIRECTORIES = 16
SECTION_HEADER_SIZE = 40
IMPORT_DESCRIPTOR_SIZE = 20
DELAY_DESCRIPTOR_SIZE = 32
LONGEST_NAME = 4096


class Malformed(Exception):
    pass


class PeImage:
    def __init__(self, data):
        self.data = data
        if self.bytes_at(0, 2) != b"MZ":
            raise Malformed("no MZ header")
        signature = self.u32(0x3C)
        if self.bytes_at(signature, 4) != PE_SIGNATURE:
            raise Malformed("no PE signature")
        coff = signature + 4
        self.machine = self.u16(coff)
        section_count = self.u16(coff + 2)
        optional_size = self.u16(coff + 16)
        self.characteristics = self.u16(coff + 18)
        optional = coff + 20
        magic = self.u16(optional)
        if magic == OPTIONAL_PE32_PLUS:
            count_at, self.thunk_size, base_at = 108, 8, 24
        elif magic == OPTIONAL_PE32:
            count_at, self.thunk_size, base_at = 92, 4, 28
        else:
            raise Malformed(f"optional header magic 0x{magic:X}")
        directory_count = min(self.u32(optional + count_at), KNOWN_DIRECTORIES)
        directories_at = optional + count_at + 4
        if directories_at + 8 * directory_count > optional + optional_size:
            raise Malformed("the data directories overrun the optional header")
        self.subsystem = self.u16(optional + 68)
        self.image_base = int.from_bytes(self.bytes_at(optional + base_at, self.thunk_size), "little")
        self.directories = [(self.u32(directories_at + 8 * index), self.u32(directories_at + 8 * index + 4))
                            for index in range(directory_count)]
        sections = optional + optional_size
        self.sections = [self.section(sections + SECTION_HEADER_SIZE * index)
                         for index in range(section_count)]

    def section(self, header):
        virtual_size, rva, raw_size, raw_at = struct.unpack("<IIII", self.bytes_at(header + 8, 16))
        return rva, min(raw_size, virtual_size) if virtual_size else raw_size, raw_at

    def bytes_at(self, offset, size):
        if offset + size > len(self.data):
            raise Malformed(f"{size} bytes at 0x{offset:X} are past the end of the file")
        return self.data[offset:offset + size]

    def u16(self, offset):
        return struct.unpack("<H", self.bytes_at(offset, 2))[0]

    def u32(self, offset):
        return struct.unpack("<I", self.bytes_at(offset, 4))[0]

    def offset_of(self, rva, size):
        for start, extent, raw_at in self.sections:
            if start <= rva and rva + size <= start + extent:
                return raw_at + rva - start
        raise Malformed(f"RVA 0x{rva:X} is in no section's file data")

    def u16_at(self, rva):
        return self.u16(self.offset_of(rva, 2))

    def u32_at(self, rva):
        return self.u32(self.offset_of(rva, 4))

    def u64_at(self, rva):
        return int.from_bytes(self.bytes_at(self.offset_of(rva, 8), 8), "little")

    def thunk_at(self, rva):
        raw = self.bytes_at(self.offset_of(rva, self.thunk_size), self.thunk_size)
        return int.from_bytes(raw, "little")

    def string_at(self, rva):
        offset = self.offset_of(rva, 1)
        text = self.data[offset:offset + LONGEST_NAME]
        end = text.find(b"\0")
        if end < 0:
            raise Malformed(f"the name at RVA 0x{rva:X} has no terminator")
        try:
            return text[:end].decode("ascii")
        except UnicodeDecodeError as error:
            raise Malformed(f"the name at RVA 0x{rva:X} is not ASCII") from error

    def directory(self, index):
        return self.directories[index][0] if index < len(self.directories) else 0

    def imports(self):
        found = []
        at = self.directory(IMPORT_DIRECTORY)
        while at:
            lookup, name, address = self.u32_at(at), self.u32_at(at + 12), self.u32_at(at + 16)
            if not (lookup or name or address):
                break
            found.append((self.string_at(name), self.symbols(lookup or address)))
            at += IMPORT_DESCRIPTOR_SIZE
        at = self.directory(DELAY_IMPORT_DIRECTORY)
        while at:
            name = self.u32_at(at + 4)
            if not name:
                break
            found.append((self.string_at(name), self.symbols(self.u32_at(at + 16))))
            at += DELAY_DESCRIPTOR_SIZE
        return found

    def symbols(self, table):
        ordinal_flag = 1 << (8 * self.thunk_size - 1)
        found = []
        while True:
            thunk = self.thunk_at(table)
            if not thunk:
                return found
            if thunk & ordinal_flag:
                found.append(f"#{thunk & 0xFFFF}")
            else:
                found.append(self.string_at((thunk & 0x7FFFFFFF) + 2))
            table += self.thunk_size

    def relocated_pointers(self):
        at, remaining = self.directories[RELOCATION_DIRECTORY] if len(self.directories) > RELOCATION_DIRECTORY \
            else (0, 0)
        found = []
        while at and remaining >= RELOCATION_BLOCK_HEADER:
            page, size = self.u32_at(at), self.u32_at(at + 4)
            if size <= RELOCATION_BLOCK_HEADER:
                raise Malformed(f"a relocation block of {size} bytes has no entries")
            if size > remaining:
                raise Malformed(f"a relocation block of {size} bytes overruns the {remaining} bytes "
                                "of the table")
            for offset in range(RELOCATION_BLOCK_HEADER, size - 1, 2):
                entry = self.u16_at(at + offset)
                if entry >> 12 == RELOCATED_DIR64:
                    rva = page + (entry & 0xFFF)
                    found.append((rva, self.u64_at(rva)))
                elif entry >> 12 == RELOCATED_HIGHLOW:
                    rva = page + (entry & 0xFFF)
                    found.append((rva, self.u32_at(rva)))
            at, remaining = at + size, remaining - size
        return found

    def exports(self):
        directory = self.directory(EXPORT_DIRECTORY)
        if not directory:
            return [], []
        base = self.u32_at(directory + 16)
        function_count, name_count = self.u32_at(directory + 20), self.u32_at(directory + 24)
        functions, names, ordinals = (self.u32_at(directory + 28), self.u32_at(directory + 32),
                                      self.u32_at(directory + 36))
        named = []
        indices = set()
        for index in range(name_count):
            named.append(self.string_at(self.u32_at(names + 4 * index)))
            function = self.u16_at(ordinals + 2 * index)
            if function >= function_count:
                raise Malformed(f"export {named[-1]} names function {function} of {function_count}")
            indices.add(function)
        unnamed = [base + index for index in range(function_count)
                   if self.u32_at(functions + 4 * index) and index not in indices]
        return named, unnamed


def name_of(table, value):
    return table.get(value, f"0x{value:X}")


def violations_of(image, arguments):
    violations = []
    wanted_machine = MACHINE_NAMES[arguments.machine]
    if image.machine != wanted_machine:
        violations.append(f"machine {name_of(MACHINES, image.machine)}, want {MACHINES[wanted_machine]}")
    wanted = (SUBSYSTEM_WINDOWS_GUI, SUBSYSTEM_WINDOWS_CUI) if arguments.dll else (SUBSYSTEM_WINDOWS_CUI,)
    if image.subsystem not in wanted:
        violations.append(f"subsystem {name_of(SUBSYSTEMS, image.subsystem)}, want "
                          + " or ".join(SUBSYSTEMS[subsystem] for subsystem in wanted))
    if arguments.dll and not image.characteristics & FILE_DLL:
        violations.append("not a DLL: IMAGE_FILE_DLL is clear")

    violations += [f"the relocated pointer at RVA 0x{rva:X} holds the image base, so it points at nothing"
                   for rva, value in image.relocated_pointers() if value == image.image_base]

    allowed = GUEST_MODULES | {module.lower() for module in arguments.allow_import}
    imported = set()
    for module, symbols in image.imports():
        if module.lower() not in allowed:
            violations.append(f"imports {module}, not a module every guest has")
        imported.update(symbols)
    for routine in arguments.require_import:
        if routine not in imported:
            violations.append(f"does not import {routine}")

    named, unnamed = image.exports()
    for export in arguments.require_export:
        if export not in named:
            violations.append(f"does not export {export}")
    if arguments.only_exports:
        violations += [f"exports {export}, which is not a required export"
                       for export in named if export not in arguments.require_export]
        violations += [f"exports ordinal {ordinal} with no name" for ordinal in unnamed]
    return violations


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("image")
    parser.add_argument("--dll", action="store_true")
    parser.add_argument("--machine", choices=sorted(MACHINE_NAMES), default="amd64",
                        help="the architecture the image must be built for")
    parser.add_argument("--require-import", action="append", default=[])
    parser.add_argument("--allow-import", action="append", default=[],
                        help="a module beyond gdi32, kernel32 and ntdll the image may import")
    parser.add_argument("--require-export", action="append", default=[])
    parser.add_argument("--only-exports", action="store_true",
                        help="refuse any export --require-export does not name")
    arguments = parser.parse_args(argv)
    try:
        with open(arguments.image, "rb") as source:
            data = source.read()
    except OSError:
        print(f"{arguments.image}: no such image", file=sys.stderr)
        return 1
    try:
        violations = violations_of(PeImage(data), arguments)
    except Malformed as malformed:
        violations = [f"not a PE image: {malformed}"]

    for violation in violations:
        print(f"{arguments.image}: {violation}", file=sys.stderr)
    if violations:
        return 1
    print("guest_image: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
