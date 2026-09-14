"""Reads a paused Windows guest's bugcheck from QEMU's gdbstub, for a panic the
crash MSRs carried no parameters for (docs/testing.md, "A bugcheck with no dump").

  gdb -q -batch -ex 'target remote 127.0.0.1:1234' -x tools/guest/read_bugcheck.py

ntoskrnl's base is found by walking down from the stopped CPU's instruction
pointer to the page holding its PE image; KiBugCheckData is read at the RVA
the guest kernel's own PDB gives (re/guest-26200/publics.txt, section .data),
and every loaded module is named from PsLoadedModuleList so the stack's
return addresses can be attributed. The guest is left stopped.
"""
import os
import struct

import gdb

# RVAs in the 26200 guest's ntoskrnl, from its PDB: section 26 (.data at
# 0xE00000) plus the publics' offsets. Override with the environment when the
# guest's build changes.
KI_BUGCHECK_DATA = int(os.environ.get("AV_KI_BUGCHECK_DATA", "0xf22740"), 16)
PS_LOADED_MODULE_LIST = int(os.environ.get("AV_PS_LOADED_MODULE_LIST", "0xef50c0"), 16)
PAGE = 0x1000
SEARCH_PAGES = 0x4000


def read(address, size):
    return bytes(gdb.selected_inferior().read_memory(address, size))


def u64(address):
    return struct.unpack("<Q", read(address, 8))[0]


def u16(address):
    return struct.unpack("<H", read(address, 2))[0]


def image_base(address):
    page = address & ~(PAGE - 1)
    for _ in range(SEARCH_PAGES):
        try:
            if read(page, 2) == b"MZ":
                offset = struct.unpack("<I", read(page + 0x3C, 4))[0]
                if offset < PAGE and read(page + offset, 4) == b"PE\0\0":
                    return page
        except gdb.MemoryError:
            pass
        page -= PAGE
    return None


def unicode(address):
    length = u16(address)
    buffer = u64(address + 8)
    return read(buffer, length).decode("utf-16-le", errors="replace") if length else ""


def modules(base):
    head = base + PS_LOADED_MODULE_LIST
    entry = u64(head)
    found = []
    while entry != head and len(found) < 512:
        start = u64(entry + 0x30)
        size = struct.unpack("<I", read(entry + 0x40, 4))[0]
        found.append((start, size, unicode(entry + 0x58)))
        entry = u64(entry)
    return found


def owner(address, loaded):
    for start, size, name in loaded:
        if start <= address < start + size:
            return f"{name}+{address - start:#x}"
    return "?"


rip = int(gdb.parse_and_eval("$pc"))
rsp = int(gdb.parse_and_eval("$sp"))
nt = image_base(rip)
if nt is None:
    print(f"read_bugcheck: no PE image below rip {rip:#x}")
else:
    code, *arguments = struct.unpack("<5Q", read(nt + KI_BUGCHECK_DATA, 40))
    print(f"read_bugcheck: nt {nt:#x} rip {rip:#x}")
    print(f"read_bugcheck: code {code:#x} arguments " + " ".join(f"{a:#x}" for a in arguments))
    loaded = modules(nt)
    for argument in arguments:
        print(f"read_bugcheck:   {argument:#x} {owner(argument, loaded)}")
    for slot in range(64):
        value = u64(rsp + slot * 8)
        name = owner(value, loaded)
        if name != "?":
            print(f"read_bugcheck: stack[{slot}] {value:#x} {name}")
gdb.execute("monitor stop")
