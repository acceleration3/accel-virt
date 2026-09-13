"""C++ for the OpenCL half of the wire.

Six files, all from the same lowered registry:

  cl_protocol_ids.h    the id constants
  cl_command_names.*   an id to a name, for the log
  cl_dispatch_client.* one wrapper per command, encoding and sending
  cl_dispatch_server.* the interface the server implements, and the
                       decode-and-call table
  cl_real_dispatch.h   that interface implemented by calling the host's
                       own OpenCL, which is what the render server runs
  cl_icd_stubs.h       a refusal for every ICD slot nothing else fills

Everything goes out through emit.common.write, which is what puts a null
check behind each arena allocation and leaves an identical file alone.

The encoding is deliberately plain. Scalars are varints, handles are
eight bytes, and every pointer that may be null is preceded by a
presence byte — including the out parameters, where the byte says
whether the caller wants the value at all so the server can skip
producing it.
"""

from __future__ import annotations

import re
from pathlib import Path

from ..emit.common import write
from .model import Command, Param, Registry, Role

HEADER = ("// THIS FILE IS GENERATED. DO NOT EDIT.\n"
          "// Regenerate with: python -m rvk_codegen.opencl --xml cl.xml --out ...\n")

# Handles cross as the server's own pointer value, widened. The guest
# never dereferences one — its ICD keeps its own objects and carries
# this alongside — so the two address spaces do not have to agree on
# anything but the width.
HANDLE_T = "std::uint64_t"

# cl.h picks a version by #pragma message when nobody says, and marks
# everything 1.2 and 2.x retired with a deprecation attribute. This wire
# carries those commands deliberately - a guest calling clEnqueueTask is
# the case this exists for - so it asks for them by name.
CL_PREAMBLE = [
    "#ifndef CL_TARGET_OPENCL_VERSION",
    "#define CL_TARGET_OPENCL_VERSION 300",
    "#endif",
    "#define CL_USE_DEPRECATED_OPENCL_1_0_APIS",
    "#define CL_USE_DEPRECATED_OPENCL_1_1_APIS",
    "#define CL_USE_DEPRECATED_OPENCL_1_2_APIS",
    "#define CL_USE_DEPRECATED_OPENCL_2_0_APIS",
    "#define CL_USE_DEPRECATED_OPENCL_2_1_APIS",
    "#define CL_USE_DEPRECATED_OPENCL_2_2_APIS",
    "",
    "#include <CL/cl.h>",
    "#include <CL/cl_gl.h>",
]



def _is_handle(reg: Registry, base: str) -> bool:
    return base in reg.handles


def _client_param_decl(reg: Registry, p: Param) -> list[str]:
    """How a parameter appears in the client wrapper's signature."""
    t = p.type
    if p.role == Role.IGNORED:
        return [f"{t.cpp()} {p.name}"]
    if p.role == Role.HANDLE:
        return [f"{HANDLE_T} {p.name}"]
    if p.role in (Role.HANDLE_ARRAY,):
        return [f"const {HANDLE_T}* {p.name}"]
    if p.role in (Role.HANDLE_OUT,):
        return [f"{HANDLE_T}* {p.name}"]
    if p.role == Role.HANDLE_ARRAY_OUT:
        return [f"{HANDLE_T}* {p.name}"]
    if p.role in (Role.PROPERTIES, Role.PROPERTY_LIST):
        return [f"const {HANDLE_T}* {p.name}"]
    if p.role in (Role.STRUCT_IN,):
        return [f"const {p.element}* {p.name}"]
    if p.role in (Role.STRUCT_OUT,):
        return [f"{p.element}* {p.name}"]
    decl = [f"{t.cpp()} {p.name}"]
    if p.role in (Role.BYTES_IN, Role.BYTES_OUT) and \
            (p.len_param or "").startswith("@"):
        # OpenCL leaves this one implicit — an image transfer's size is
        # its region, its pitches and its format — so the caller, which
        # knows all three, passes it.
        decl.append(f"std::size_t {p.name}_bytes")
    return decl


def _server_param_decl(reg: Registry, p: Param) -> str:
    t = p.type
    if p.role == Role.HANDLE:
        return f"{t.base} {p.name}"
    if p.role == Role.HANDLE_ARRAY:
        return f"const {p.element}* {p.name}"
    if p.role == Role.HANDLE_OUT:
        return f"{p.element}* {p.name}"
    if p.role == Role.HANDLE_ARRAY_OUT:
        return f"{p.element}* {p.name}"
    if p.role in (Role.PROPERTIES, Role.PROPERTY_LIST):
        return f"const {t.base}* {p.name}"
    if p.role == Role.STRUCT_IN:
        return f"const {p.element}* {p.name}"
    if p.role == Role.STRUCT_OUT:
        return f"{p.element}* {p.name}"
    return f"{t.cpp()} {p.name}"


def _client_signature(reg: Registry, cmd: Command) -> str:
    ret = HANDLE_T if cmd.returns_handle else \
        ("void" if cmd.returns_void else cmd.return_type.cpp())
    params = ["::rvk::protocol::Transport& t"]
    for p in cmd.params:
        if p.role == Role.IGNORED:
            # Not on the wire, but a string array's lengths are read by
            # the encoder, so the caller still passes them.
            if _lengths_owner(cmd, p) is not None:
                params.append(f"const std::size_t* {p.name}")
            continue
        params.extend(_client_param_decl(reg, p))
    return f"{ret} rvk_{cmd.name}({', '.join(params)})"


def _server_signature(reg: Registry, cmd: Command) -> str:
    ret = cmd.return_type.cpp()
    params = [_server_param_decl(reg, p) for p in cmd.params
              if p.role != Role.IGNORED]
    return f"{ret} on_{cmd.name}({', '.join(params)})"


# ---------------------------------------------------------------------
# Request encoding, client side
# ---------------------------------------------------------------------

def _encode_param(reg: Registry, cmd: Command, p: Param, out: list[str]) -> None:
    n = p.name
    if p.role == Role.IGNORED:
        return
    if p.role == Role.SCALAR:
        out.append(f"    b.write_varint(static_cast<std::uint64_t>({n}));")
    elif p.role == Role.HANDLE:
        out.append(f"    b.write_pod<{HANDLE_T}>({n});")
    elif p.role == Role.HANDLE_ARRAY:
        count = p.len_param
        out.append(f"    b.write_pod<std::uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append(f"        b.write_varint(static_cast<std::uint64_t>({count}));")
        out.append(f"        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>({count}); ++i) {{")
        out.append(f"            b.write_pod<{HANDLE_T}>({n}[i]);")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.SCALAR_ARRAY:
        count = p.len_param
        out.append(f"    b.write_pod<std::uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append(f"        b.write_varint(static_cast<std::uint64_t>({count}));")
        out.append(f"        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>({count}); ++i) {{")
        out.append(f"            b.write_varint(static_cast<std::uint64_t>({n}[i]));")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.STRING:
        out.append(f"    b.write_pod<std::uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append(f"        const std::size_t wire_len = std::strlen({n});")
        out.append("        b.write_varint(wire_len);")
        out.append(f"        b.write({n}, wire_len);")
        out.append("    }")
    elif p.role == Role.STRING_ARRAY:
        count = p.len_param
        lengths = p.lengths_param
        out.append(f"    b.write_pod<std::uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append(f"        b.write_varint(static_cast<std::uint64_t>({count}));")
        out.append(f"        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>({count}); ++i) {{")
        if lengths:
            out.append(f"            const std::size_t wire_len = ({lengths} != nullptr && {lengths}[i] != 0)")
            out.append(f"                ? {lengths}[i]")
            out.append(f"                : ({n}[i] != nullptr ? std::strlen(reinterpret_cast<const char*>({n}[i])) : 0);")
        else:
            out.append(f"            const std::size_t wire_len = {n}[i] != nullptr ? std::strlen({n}[i]) : 0;")
        out.append("            b.write_varint(wire_len);")
        out.append(f"            if (wire_len != 0) b.write({n}[i], wire_len);")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.BYTES_IN:
        size = f"{n}_bytes" if (p.len_param or "").startswith("@") else p.len_param
        out.append(f"    b.write_pod<std::uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append(f"        b.write_varint(static_cast<std::uint64_t>({size}));")
        out.append(f"        b.write({n}, static_cast<std::size_t>({size}));")
        out.append("    }")
    elif p.role == Role.STRUCT_IN:
        out.append(f"    b.write_pod<std::uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) b.write({n}, sizeof(*{n}));")
    elif p.role == Role.PROPERTIES:
        # Zero-terminated key/value pairs. Counted here so the far side
        # does not have to trust a terminator that may not be there.
        out.append(f"    b.write_pod<std::uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append("        std::uint64_t wire_count = 0;")
        out.append(f"        while ({n}[wire_count] != 0) wire_count += 2;")
        out.append("        b.write_varint(wire_count);")
        out.append("        for (std::uint64_t i = 0; i < wire_count; ++i) {")
        out.append(f"            b.write_pod<{HANDLE_T}>({n}[i]);")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.PROPERTY_LIST:
        out.append(f"    b.write_pod<std::uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append("        std::uint64_t wire_count = 0;")
        out.append(f"        while ({n}[wire_count] != 0) ++wire_count;")
        out.append("        b.write_varint(wire_count);")
        out.append("        for (std::uint64_t i = 0; i < wire_count; ++i) {")
        out.append(f"            b.write_pod<{HANDLE_T}>(static_cast<{HANDLE_T}>({n}[i]));")
        out.append("        }")
        out.append("    }")
    elif p.role in (Role.HANDLE_OUT, Role.SCALAR_OUT, Role.STRUCT_OUT,
                    Role.HANDLE_ARRAY_OUT, Role.INFO_VALUE):
        # The byte says whether the caller wants it; the server skips
        # producing what nobody asked for.
        out.append(f"    b.write_pod<std::uint8_t>({n} != nullptr ? 1 : 0);")
    elif p.role == Role.BYTES_OUT:
        size = f"{n}_bytes" if (p.len_param or "").startswith("@") else p.len_param
        out.append(f"    b.write_pod<std::uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) b.write_varint(static_cast<std::uint64_t>({size}));")
    else:
        raise AssertionError(f"no encoder for {p.role}")


def _decode_reply(reg: Registry, cmd: Command, out: list[str]) -> None:
    """Client side: pull the return value and the out parameters back."""
    if cmd.returns_handle:
        out.append("    " + HANDLE_T + " ret = 0;")
        out.append("    if (!r.read_pod(ret)) return 0;")
    elif not cmd.returns_void:
        out.append(f"    {cmd.return_type.cpp()} ret{{}};")
        out.append("    std::int64_t ret_wire = 0;")
        out.append("    if (!r.read_varint(ret_wire)) return static_cast<"
                   f"{cmd.return_type.cpp()}>(-1);")
        out.append(f"    ret = static_cast<{cmd.return_type.cpp()}>(ret_wire);")

    fail = "return 0;" if cmd.returns_handle else (
        "return;" if cmd.returns_void else "return ret;")

    for p in cmd.out_params:
        n = p.name
        out.append(f"    if ({n} != nullptr) {{")
        if p.role == Role.HANDLE_OUT:
            out.append(f"        if (!r.read_pod(*{n})) {fail}")
        elif p.role == Role.SCALAR_OUT:
            out.append("        std::uint64_t wire_value = 0;")
            out.append(f"        if (!r.read_varint(wire_value)) {fail}")
            out.append(f"        *{n} = static_cast<{p.type.base}>(wire_value);")
        elif p.role == Role.STRUCT_OUT:
            out.append(f"        if (!r.read({n}, sizeof(*{n}))) {fail}")
        elif p.role == Role.HANDLE_ARRAY_OUT:
            out.append("        std::uint64_t wire_count = 0;")
            out.append(f"        if (!r.read_varint(wire_count)) {fail}")
            out.append("        for (std::uint64_t i = 0; i < wire_count; ++i) {")
            out.append(f"            if (!r.read_pod({n}[i])) {fail}")
            out.append("        }")
        elif p.role in (Role.BYTES_OUT, Role.INFO_VALUE):
            out.append("        std::uint64_t wire_count = 0;")
            out.append(f"        if (!r.read_varint(wire_count)) {fail}")
            out.append(f"        if (wire_count != 0 && !r.read({n}, static_cast<std::size_t>(wire_count))) {fail}")
        else:
            raise AssertionError(f"no reply decoder for {p.role}")
        out.append("    }")


def emit_client(reg: Registry, out_dir: Path) -> None:
    h = [HEADER, "#pragma once\n",
         "#include <cstddef>", "#include <cstdint>", ""] + CL_PREAMBLE + ["",
         '#include "rvk/protocol/transport.hpp"', "",
         "namespace av::clproto {", ""]
    c = [HEADER, '#include "cl_dispatch_client.h"', "",
         "#include <cstring>", "",
         '#include "proto/wire/wire_reader.hpp"',
         '#include "proto/wire/wire_writer.hpp"',
         '#include "cl_protocol_ids.h"', "",
         "namespace av::clproto {", ""]

    for cmd in reg.commands.values():
        sig = _client_signature(reg, cmd)
        h.append(f"{sig};")
        c.append(f"{sig} {{")
        c.append("    ::av::WireWriter b;")
        for p in cmd.params:
            _encode_param(reg, cmd, p, c)
        if cmd.is_async:
            c.append(f"    (void)t.send_async(kCmd_{cmd.name}, b.bytes());")
            if cmd.returns_handle:
                c.append("    return 0;")
            elif not cmd.returns_void:
                c.append("    return CL_SUCCESS;")
        else:
            c.append("    ::av::WireWriter reply;")
            c.append(f"    if (t.send_sync(kCmd_{cmd.name}, b.bytes(), &reply) !=")
            c.append("        ::rvk::protocol::TransportResult::Ok) {")
            if cmd.returns_handle:
                c.append("        return 0;")
            elif cmd.returns_void:
                c.append("        return;")
            else:
                c.append("        return CL_OUT_OF_RESOURCES;")
            c.append("    }")
            c.append("    ::av::WireReader r{reply.bytes()};")
            _decode_reply(reg, cmd, c)
            if cmd.returns_handle or not cmd.returns_void:
                c.append("    return ret;")
        c.append("}")
        c.append("")

    h += ["", "}  // namespace av::clproto", ""]
    c += ["}  // namespace av::clproto", ""]
    write(out_dir / "cl_dispatch_client.h", "\n".join(h))
    write(out_dir / "cl_dispatch_client.cpp", "\n".join(c))


# ---------------------------------------------------------------------
# Server side
# ---------------------------------------------------------------------

def _decode_param(reg: Registry, cmd: Command, p: Param, out: list[str]) -> None:
    n = p.name
    if p.role == Role.IGNORED:
        return
    if p.role == Role.SCALAR:
        out.append("    {")
        out.append("        std::uint64_t wire_value = 0;")
        out.append("        if (!r.read_varint(wire_value)) return false;")
        out.append(f"        {n} = static_cast<{p.type.base}>(wire_value);")
        out.append("    }")
    elif p.role == Role.HANDLE:
        out.append(f"    {{ {HANDLE_T} wire_handle = 0;")
        out.append("      if (!r.read_pod(wire_handle)) return false;")
        out.append(f"      {n} = reinterpret_cast<{p.type.base}>("
                   "static_cast<std::uintptr_t>(wire_handle)); }")
    elif p.role in (Role.HANDLE_ARRAY, Role.SCALAR_ARRAY):
        elem = p.element if p.role == Role.HANDLE_ARRAY else p.type.base
        out.append("    {")
        out.append("        std::uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append("            std::uint64_t wire_count = 0;")
        out.append("            if (!r.read_varint(wire_count)) return false;")
        out.append(f"            auto* wire_items = arena.allocate<{elem}>(static_cast<std::size_t>(wire_count) + 1);")
        out.append("            if (wire_items == nullptr) return false;")
        out.append("            for (std::uint64_t i = 0; i < wire_count; ++i) {")
        if p.role == Role.HANDLE_ARRAY:
            out.append(f"                {HANDLE_T} wire_handle = 0;")
            out.append("                if (!r.read_pod(wire_handle)) return false;")
            out.append(f"                wire_items[i] = reinterpret_cast<{elem}>(static_cast<std::uintptr_t>(wire_handle));")
        else:
            out.append("                std::uint64_t wire_value = 0;")
            out.append("                if (!r.read_varint(wire_value)) return false;")
            out.append(f"                wire_items[i] = static_cast<{elem}>(wire_value);")
        out.append("            }")
        out.append(f"            {n} = wire_items;")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.STRING:
        out.append("    {")
        out.append("        std::uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append("            std::uint64_t wire_len = 0;")
        out.append("            if (!r.read_varint(wire_len)) return false;")
        out.append("            char* wire_text = arena.allocate_chars(static_cast<std::size_t>(wire_len) + 1);")
        out.append("            if (wire_text == nullptr) return false;")
        out.append("            if (wire_len != 0 && !r.read(wire_text, static_cast<std::size_t>(wire_len))) return false;")
        out.append("            wire_text[wire_len] = '\\0';")
        out.append(f"            {n} = wire_text;")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.STRING_ARRAY:
        out.append("    {")
        out.append("        std::uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append("            std::uint64_t wire_count = 0;")
        out.append("            if (!r.read_varint(wire_count)) return false;")
        out.append(f"            auto** wire_items = arena.allocate<const {p.type.base}*>("
                   "static_cast<std::size_t>(wire_count) + 1);")
        out.append("            if (wire_items == nullptr) return false;")
        keeps_lengths = _lengths_consumer(cmd, p) is not None
        if keeps_lengths:
            out.append("            auto* wire_lengths = arena.allocate<std::size_t>("
                       "static_cast<std::size_t>(wire_count) + 1);")
            out.append("            if (wire_lengths == nullptr) return false;")
        out.append("            for (std::uint64_t i = 0; i < wire_count; ++i) {")
        out.append("                std::uint64_t wire_len = 0;")
        out.append("                if (!r.read_varint(wire_len)) return false;")
        out.append("                char* wire_text = arena.allocate_chars(static_cast<std::size_t>(wire_len) + 1);")
        out.append("                if (wire_text == nullptr) return false;")
        out.append("                if (wire_len != 0 && !r.read(wire_text, static_cast<std::size_t>(wire_len))) return false;")
        out.append("                wire_text[wire_len] = '\\0';")
        out.append(f"                wire_items[i] = reinterpret_cast<const {p.type.base}*>(wire_text);")
        if keeps_lengths:
            out.append("                wire_lengths[i] = static_cast<std::size_t>(wire_len);")
        out.append("            }")
        out.append(f"            {n} = wire_items;")
        if keeps_lengths:
            out.append(f"            {n}_lengths = wire_lengths;")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.BYTES_IN:
        out.append("    {")
        out.append("        std::uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append("            std::uint64_t wire_count = 0;")
        out.append("            if (!r.read_varint(wire_count)) return false;")
        # The reader hands out no view of its own buffer, so the bytes are
        # copied into the caller's block like everything else pointed at.
        out.append("            auto* wire_bytes = arena.allocate<std::uint8_t>("
                   "static_cast<std::size_t>(wire_count) + 1);")
        out.append("            if (wire_bytes == nullptr) return false;")
        out.append("            if (wire_count != 0 && !r.read(wire_bytes, "
                   "static_cast<std::size_t>(wire_count))) return false;")
        out.append(f"            {n} = reinterpret_cast<{p.type.cpp()}>(wire_bytes);")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.STRUCT_IN:
        out.append("    {")
        out.append("        std::uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append(f"            auto* wire_item = arena.allocate<{p.element}>(1);")
        out.append("            if (wire_item == nullptr) return false;")
        out.append("            if (!r.read(wire_item, sizeof(*wire_item))) return false;")
        out.append(f"            {n} = wire_item;")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.PROPERTIES:
        out.append("    {")
        out.append("        std::uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append("            std::uint64_t wire_count = 0;")
        out.append("            if (!r.read_varint(wire_count)) return false;")
        out.append(f"            auto* wire_items = arena.allocate<{p.type.base}>(static_cast<std::size_t>(wire_count) + 1);")
        out.append("            if (wire_items == nullptr) return false;")
        out.append("            for (std::uint64_t i = 0; i < wire_count; ++i) {")
        out.append(f"                {HANDLE_T} wire_value = 0;")
        out.append("                if (!r.read_pod(wire_value)) return false;")
        out.append(f"                wire_items[i] = static_cast<{p.type.base}>(wire_value);")
        out.append("            }")
        out.append("            wire_items[wire_count] = 0;")
        out.append(f"            {n} = wire_items;")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.PROPERTY_LIST:
        out.append("    {")
        out.append("        std::uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append("            std::uint64_t wire_count = 0;")
        out.append("            if (!r.read_varint(wire_count)) return false;")
        out.append(f"            auto* wire_items = arena.allocate<{p.type.base}>(static_cast<std::size_t>(wire_count) + 1);")
        out.append("            if (wire_items == nullptr) return false;")
        out.append("            for (std::uint64_t i = 0; i < wire_count; ++i) {")
        out.append(f"                {HANDLE_T} wire_value = 0;")
        out.append("                if (!r.read_pod(wire_value)) return false;")
        out.append(f"                wire_items[i] = static_cast<{p.type.base}>(wire_value);")
        out.append("            }")
        out.append("            wire_items[wire_count] = 0;")
        out.append(f"            {n} = wire_items;")
        out.append("        }")
        out.append("    }")
    elif p.role in (Role.HANDLE_OUT, Role.SCALAR_OUT, Role.STRUCT_OUT,
                    Role.HANDLE_ARRAY_OUT, Role.INFO_VALUE, Role.BYTES_OUT):
        out.append("    {")
        out.append("        std::uint8_t wire_wanted = 0;")
        out.append("        if (!r.read_pod(wire_wanted)) return false;")
        out.append(f"        {n}_wanted = wire_wanted != 0;")
        if p.role == Role.BYTES_OUT:
            out.append(f"        if ({n}_wanted) {{")
            out.append("            std::uint64_t cap = 0;")
            out.append("            if (!r.read_varint(cap)) return false;")
            out.append(f"            {n}_capacity = static_cast<std::size_t>(cap);")
            out.append("        }")
        out.append("    }")
    else:
        raise AssertionError(f"no server decoder for {p.role}")


def _lengths_consumer(cmd: Command, array: Param) -> Param | None:
    """The parameter this string array's per-string lengths are handed to."""
    if not array.lengths_param:
        return None
    for other in cmd.params:
        if other.name == array.lengths_param and other.role == Role.IGNORED:
            return other
    return None


def _lengths_owner(cmd: Command, p: Param) -> Param | None:
    """The string array, if this ignored parameter is its lengths."""
    for other in cmd.params:
        if other.role == Role.STRING_ARRAY and other.lengths_param == p.name:
            return other
    return None


def _server_params(reg: Registry, cmd: Command) -> list[tuple[Param, str]]:
    """(param, declaration) for everything the implementation is handed."""
    result: list[tuple[Param, str]] = []
    for p in cmd.params:
        if p.role == Role.IGNORED:
            owner = _lengths_owner(cmd, p)
            if owner is not None:
                # Not on the wire — the lengths travel inside the strings
                # — but the call underneath still takes them.
                result.append((p, f"const std::size_t* {p.name}"))
            continue
        result.append((p, _server_param_decl(reg, p)))
    return result


def _server_signature2(reg: Registry, cmd: Command) -> str:
    decls = ", ".join(d for _, d in _server_params(reg, cmd))
    return f"{cmd.return_type.cpp()} on_{cmd.name}({decls})"


def _capacity_expr(cmd: Command, p: Param) -> str:
    if p.role == Role.HANDLE_ARRAY_OUT:
        return p.len_param or "0"
    if p.role == Role.INFO_VALUE:
        return p.size_param or "0"
    if p.role == Role.BYTES_OUT:
        return f"{p.name}_capacity"
    return "0"


def emit_server(reg: Registry, out_dir: Path) -> None:
    h = [HEADER, "#pragma once\n",
         "#include <cstddef>", "#include <cstdint>", ""] + CL_PREAMBLE + ["",
         '#include "proto/wire/wire_arena.hpp"',
         '#include "proto/wire/wire_reader.hpp"',
         '#include "proto/wire/wire_writer.hpp"', "",
         "namespace av::clproto {", "",
         "// Implemented by the server; the table below calls into it.",
         "class ServerDispatch {", "public:",
         "    virtual ~ServerDispatch() = default;", ""]
    c = [HEADER, '#include "cl_dispatch_server.h"', "",
         "#include <cstddef>", "#include <cstdint>", "",
         '#include "cl_protocol_ids.h"', "",
         "namespace av::clproto {", ""]

    for cmd in reg.commands.values():
        h.append(f"    virtual {_server_signature2(reg, cmd)} = 0;")

        c.append(f"static bool decode_{cmd.name}(ServerDispatch& s, "
                 "::av::WireReader& r,")
        c.append(f"{' ' * (len(cmd.name) + 19)}::av::WireWriter& reply, "
                 "::av::WireArena& arena) {")
        c.append("    (void)r;")
        c.append("    (void)reply;")
        c.append("    (void)arena;")
        # Locals, in declaration order.
        for p in cmd.params:
            if p.role == Role.IGNORED:
                owner = _lengths_owner(cmd, p)
                if owner is not None:
                    c.append(f"    const std::size_t* {p.name} = nullptr;")
                continue
            if p.role == Role.SCALAR:
                c.append(f"    {p.type.base} {p.name}{{}};")
            elif p.role == Role.HANDLE:
                c.append(f"    {p.type.base} {p.name} = nullptr;")
            elif p.role == Role.HANDLE_ARRAY:
                c.append(f"    const {p.element}* {p.name} = nullptr;")
            elif p.role == Role.SCALAR_ARRAY:
                c.append(f"    const {p.type.base}* {p.name} = nullptr;")
            elif p.role == Role.STRING:
                c.append(f"    const char* {p.name} = nullptr;")
            elif p.role == Role.STRING_ARRAY:
                c.append(f"    const {p.type.base}** {p.name} = nullptr;")
                if _lengths_consumer(cmd, p) is not None:
                    c.append(f"    const std::size_t* {p.name}_lengths = nullptr;")
            elif p.role == Role.BYTES_IN:
                c.append(f"    {p.type.cpp()} {p.name} = nullptr;")
            elif p.role == Role.STRUCT_IN:
                c.append(f"    const {p.element}* {p.name} = nullptr;")
            elif p.role in (Role.PROPERTIES, Role.PROPERTY_LIST):
                c.append(f"    const {p.type.base}* {p.name} = nullptr;")
            else:
                c.append(f"    bool {p.name}_wanted = false;")
                if p.role == Role.BYTES_OUT:
                    c.append(f"    std::size_t {p.name}_capacity = 0;")

        for p in cmd.params:
            _decode_param(reg, cmd, p, c)

        # Storage for whatever the caller asked for.
        for p in cmd.out_params:
            n = p.name
            if p.role == Role.HANDLE_OUT:
                c.append(f"    {p.element} {n}_storage = nullptr;")
                c.append(f"    {p.element}* {n} = {n}_wanted ? &{n}_storage : nullptr;")
            elif p.role == Role.SCALAR_OUT:
                c.append(f"    {p.type.base} {n}_storage{{}};")
                c.append(f"    {p.type.base}* {n} = {n}_wanted ? &{n}_storage : nullptr;")
            elif p.role == Role.STRUCT_OUT:
                c.append(f"    {p.element} {n}_storage{{}};")
                c.append(f"    {p.element}* {n} = {n}_wanted ? &{n}_storage : nullptr;")
            elif p.role == Role.HANDLE_ARRAY_OUT:
                cap = _capacity_expr(cmd, p)
                c.append(f"    {p.element}* {n} = nullptr;")
                c.append(f"    if ({n}_wanted) {{")
                c.append(f"        {n} = arena.allocate<{p.element}>(static_cast<std::size_t>({cap}) + 1);")
                c.append(f"        if ({n} == nullptr) return false;")
                c.append("    }")
            elif p.role in (Role.INFO_VALUE, Role.BYTES_OUT):
                cap = _capacity_expr(cmd, p)
                ptr_t = "void*" if p.type.base == "void" else f"{p.type.base}*"
                c.append(f"    {ptr_t} {n} = nullptr;")
                c.append(f"    if ({n}_wanted) {{")
                c.append(f"        {n} = reinterpret_cast<{ptr_t}>("
                         f"arena.allocate<std::uint8_t>(static_cast<std::size_t>({cap}) + 1));")
                c.append(f"        if ({n} == nullptr) return false;")
                c.append("    }")

        # The lengths a string array carried inside itself.
        for p in cmd.params:
            if p.role == Role.IGNORED and _lengths_owner(cmd, p) is not None:
                owner = _lengths_owner(cmd, p)
                c.append(f"    {p.name} = {owner.name}_lengths;")

        args = ", ".join(p.name for p, _ in _server_params(reg, cmd))
        if cmd.returns_void:
            c.append(f"    s.on_{cmd.name}({args});")
        else:
            c.append(f"    const {cmd.return_type.cpp()} ret = s.on_{cmd.name}({args});")

        # Reply: the return value, then what was asked for.
        if cmd.returns_handle:
            c.append("    reply.write_pod<" + HANDLE_T +
                     ">(static_cast<" + HANDLE_T + ">(reinterpret_cast<std::uintptr_t>(ret)));")
        elif not cmd.returns_void:
            c.append("    reply.write_varint(static_cast<std::uint64_t>("
                     "static_cast<std::int64_t>(ret)));")

        for p in cmd.out_params:
            n = p.name
            c.append(f"    if ({n}_wanted) {{")
            if p.role == Role.HANDLE_OUT:
                c.append(f"        reply.write_pod<{HANDLE_T}>(static_cast<{HANDLE_T}>("
                         f"reinterpret_cast<std::uintptr_t>({n}_storage)));")
            elif p.role == Role.SCALAR_OUT:
                c.append(f"        reply.write_varint(static_cast<std::uint64_t>({n}_storage));")
            elif p.role == Role.STRUCT_OUT:
                c.append(f"        reply.write(&{n}_storage, sizeof({n}_storage));")
            elif p.role == Role.HANDLE_ARRAY_OUT:
                # However many the call actually produced, never more
                # than the caller's room for them.
                count = p.size_ret_param
                cap = _capacity_expr(cmd, p)
                if count:
                    c.append(f"        const std::uint64_t wire_produced = {count} != nullptr")
                    c.append(f"            ? static_cast<std::uint64_t>(*{count})")
                    c.append(f"            : static_cast<std::uint64_t>({cap});")
                else:
                    c.append(f"        const std::uint64_t wire_produced = static_cast<std::uint64_t>({cap});")
                c.append(f"        const std::uint64_t wire_sent = wire_produced < static_cast<std::uint64_t>({cap})")
                c.append(f"            ? wire_produced : static_cast<std::uint64_t>({cap});")
                c.append("        reply.write_varint(wire_sent);")
                c.append("        for (std::uint64_t i = 0; i < wire_sent; ++i) {")
                c.append(f"            reply.write_pod<{HANDLE_T}>(static_cast<{HANDLE_T}>("
                         f"reinterpret_cast<std::uintptr_t>({n}[i])));")
                c.append("        }")
            elif p.role == Role.INFO_VALUE:
                size_ret = p.size_ret_param
                cap = _capacity_expr(cmd, p)
                if size_ret:
                    c.append(f"        std::uint64_t wire_written = {size_ret} != nullptr")
                    c.append(f"            ? static_cast<std::uint64_t>(*{size_ret})")
                    c.append(f"            : static_cast<std::uint64_t>({cap});")
                else:
                    c.append(f"        std::uint64_t wire_written = static_cast<std::uint64_t>({cap});")
                c.append(f"        if (wire_written > static_cast<std::uint64_t>({cap}))")
                c.append(f"            wire_written = static_cast<std::uint64_t>({cap});")
                c.append("        reply.write_varint(wire_written);")
                c.append(f"        if (wire_written != 0) reply.write({n}, static_cast<std::size_t>(wire_written));")
            elif p.role == Role.BYTES_OUT:
                cap = _capacity_expr(cmd, p)
                c.append(f"        reply.write_varint(static_cast<std::uint64_t>({cap}));")
                c.append(f"        if ({cap} != 0) reply.write({n}, static_cast<std::size_t>({cap}));")
            c.append("    }")

        c.append("    return true;")
        c.append("}")
        c.append("")

    c.append("bool dispatch_command(ServerDispatch& s, std::uint32_t cmd_id,")
    c.append("                      ::av::WireReader& r, ::av::WireWriter& reply,")
    c.append("                      ::av::WireArena& arena) {")
    c.append("    switch (cmd_id) {")
    for cmd in reg.commands.values():
        c.append(f"        case kCmd_{cmd.name}: "
                 f"return decode_{cmd.name}(s, r, reply, arena);")
    c.append("        default: return false;")
    c.append("    }")
    c.append("}")
    c.append("")

    h += ["};", "",
          "// `arena` is where everything a decoded command points at lives,",
          "// for the length of the call; it is the caller's block, because it",
          "// has to outlive the decode that filled it.",
          "[[nodiscard]] bool dispatch_command(ServerDispatch& s, std::uint32_t cmd_id,",
          "                                    ::av::WireReader& r, ::av::WireWriter& reply,",
          "                                    ::av::WireArena& arena);", "",
          "}  // namespace av::clproto", ""]
    c += ["}  // namespace av::clproto", ""]
    write(out_dir / "cl_dispatch_server.h", "\n".join(h))
    write(out_dir / "cl_dispatch_server.cpp", "\n".join(c))


def emit_real_dispatch(reg: Registry, out_dir: Path) -> None:
    """The interface implemented by calling the host's own OpenCL."""
    out = [HEADER, "#pragma once\n"] + CL_PREAMBLE + ["",
           '#include "cl_dispatch_server.h"', "",
           "namespace av::clproto {", "",
           "// Every command forwarded to whatever OpenCL this process",
           "// loaded. The handles the guest sent are this process's own",
           "// pointers, so there is nothing to translate.",
           "class RealDispatch : public ServerDispatch {", "public:"]
    for cmd in reg.commands.values():
        decls = _server_params(reg, cmd)
        sig = _server_signature2(reg, cmd)
        args = []
        for p in cmd.params:
            if p.role == Role.IGNORED:
                if _lengths_owner(cmd, p) is not None:
                    args.append(p.name)
                else:
                    # A callback and its user data cannot cross a
                    # process boundary; the guest keeps both and calls
                    # the callback itself once the reply lands.
                    args.append("nullptr")
                continue
            args.append(p.name)
        call = f"::{cmd.name}({', '.join(args)})"
        out.append(f"    {sig} override {{")
        if cmd.returns_void:
            out.append(f"        {call};")
        else:
            out.append(f"        return {call};")
        out.append("    }")
    out += ["};", "", "}  // namespace av::clproto", ""]
    write(out_dir / "cl_real_dispatch.h", "\n".join(out))


def emit_ids(reg: Registry, out_dir: Path) -> None:
    out = [HEADER, "#pragma once\n", "#include <cstdint>", "",
           "namespace av::clproto {", ""]
    for cmd in reg.commands.values():
        out.append(f"inline constexpr std::uint32_t kCmd_{cmd.name} = "
                   f"0x{cmd.cmd_id:08X}U;")
    out += ["", "}  // namespace av::clproto", ""]
    write(out_dir / "cl_protocol_ids.h", "\n".join(out))


def emit_names(reg: Registry, out_dir: Path) -> None:
    h = [HEADER, "#pragma once\n", "#include <cstdint>", "",
         "namespace av::clproto {", "",
         "// A static name for a wire id; \"unknown\" for anything else,",
         "// never null.",
         "[[nodiscard]] const char* cmd_id_to_string(std::uint32_t cmd_id) noexcept;",
         "", "}  // namespace av::clproto", ""]
    c = [HEADER, '#include "cl_command_names.h"', "",
         '#include "cl_protocol_ids.h"', "",
         "namespace av::clproto {", "",
         "const char* cmd_id_to_string(std::uint32_t cmd_id) noexcept {",
         "    switch (cmd_id) {"]
    for cmd in reg.commands.values():
        c.append(f'        case kCmd_{cmd.name}: return "{cmd.name}";')
    c += ["        default: return \"unknown\";", "    }", "}", "",
          "}  // namespace av::clproto", ""]
    write(out_dir / "cl_command_names.h", "\n".join(h))
    write(out_dir / "cl_command_names.cpp", "\n".join(c))


def emit_all(reg: Registry, out_dir: Path, raw=None) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    emit_ids(reg, out_dir)
    emit_names(reg, out_dir)
    emit_client(reg, out_dir)
    emit_server(reg, out_dir)
    emit_real_dispatch(reg, out_dir)
    if raw is not None:
        emit_icd_stubs(raw, reg, out_dir)


# ---------------------------------------------------------------------
# Stubs for everything not implemented
#
# A null slot in the ICD dispatch table is worse than a missing entry
# point: the loader calls through it. And a stub cannot be generic,
# because a command returning cl_int must answer CL_INVALID_OPERATION
# while one returning an object must answer null — the same bit pattern
# means "fine" in one and "failed" in the other.
#
# So every command in the registry gets a stub with its own signature,
# whether or not the wire carries it, and the table is swept once at
# load: any slot still null after the real entry points are in gets its
# stub. What an application then sees is a clean CL_INVALID_OPERATION
# rather than a jump to address zero.
# ---------------------------------------------------------------------

def _stub_signature(raw, name: str) -> tuple[str, str, str]:
    """(return type, parameter list, what the stub answers)."""
    ret = raw.return_type.cpp()
    params = []
    has_errcode = False
    for i, p in enumerate(raw.params):
        if p.is_callback:
            # A function pointer's declaration cannot be rebuilt from a
            # base type and a pointer count, so the registry's own text is
            # used as written - minus the name, which a stub never reads
            # and -Wunused-parameter would refuse. The first occurrence is
            # the pointer's own name; later ones name the callback's
            # parameters, where a name costs nothing.
            params.append(re.sub(rf"\b{p.name}\b", f"/*{p.name}*/", p.text,
                                 count=1))
            continue
        # A create-shaped command reports through errcode_ret, and an
        # application that reads it deserves the real reason rather
        # than whatever was in that variable before the call.
        if p.name == "errcode_ret" and p.type.cpp() == "cl_int*":
            has_errcode = True
            params.append("cl_int* errcode_ret")
            continue
        params.append(f"{p.type.cpp()} /*{p.name}*/")
    prologue = ("if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_OPERATION; "
                if has_errcode else "")
    if ret == "cl_int":
        answer = prologue + "return CL_INVALID_OPERATION;"
    elif raw.return_type.pointer_depth > 0 or ret.startswith("cl_") and \
            ret not in ("cl_int", "cl_uint", "cl_ulong", "cl_bool"):
        answer = prologue + "return nullptr;"
    elif ret == "void":
        answer = ""
    else:
        answer = "return 0;"
    return ret, ", ".join(params) if params else "", answer


def emit_icd_stubs(raw, reg: Registry, out_dir: Path) -> None:
    out = [HEADER, "#pragma once\n",
           "#include <type_traits>", ""] + CL_PREAMBLE + [
           "#include <CL/cl_icd.h>", "",
           '#include "rvk/client/stub_report.hpp"', "",
           "namespace av::clproto {", "",
           "// Every one of these answers CL_INVALID_OPERATION, and says so",
           "// once: an application that asks for something this driver does",
           "// not carry gets a clean refusal, and whoever is reading the log",
           "// gets the name of what it asked for. See",
           "// rvk/client/stub_report.hpp.",
           "namespace stubs {", ""]

    # A stub can only be written for a command whose types this
    # translation unit has. The sharing extensions name D3D interfaces,
    # EGL displays and GL objects, which live in headers an ICD that
    # does not implement them has no business including — so those
    # slots are left exactly as the loader found them.
    def declarable(t) -> bool:
        base = t.base
        if base.startswith("cl_GL") or base.startswith("cl_gl"):
            return False
        if base.startswith("cl_") and not any(
                token in base.lower()
                for token in ("d3d", "dx9", "egl", "va_api", "gl_")):
            return True
        return base in ("size_t", "void", "char", "unsigned char", "int",
                        "unsigned int", "float", "double", "intptr_t")

    names: list[str] = []
    everything: list[str] = list(raw.commands)
    for name, cmd in raw.commands.items():
        # Core commands only. Their types are always declared, and they
        # are the ones that matter: the loader exports every core entry
        # point and dispatches it straight through this table, so a null
        # core slot is a crash waiting for the first application that
        # calls it. An extension slot is only reached through
        # clGetExtensionFunctionAddress, which this driver answers with
        # null — the documented way to say "not here".
        if not cmd.since.startswith("CL_VERSION_"):
            continue
        if not declarable(cmd.return_type):
            continue
        if not all(p.is_callback or declarable(p.type) for p in cmd.params):
            continue
        ret, params, answer = _stub_signature(cmd, name)
        out.append(f"inline {ret} CL_API_CALL {name}({params}) {{")
        out.append(f'    ::rvk::client::stub_called("cl", "{name}", '
                   '"this driver does not carry it");')
        if answer:
            out.append(f"    {answer}")
        out.append("}")
        names.append(name)
    out.append("")
    out.append("}  // namespace stubs")
    out.append("")
    out.append("// A refusal built from the table's own slot type.")
    out.append("//")
    out.append("// The named stubs above are wire_written from the registry, and")
    out.append("// for a handful of commands the registry and cl_icd.h")
    out.append("// disagree about a parameter — void* against void*[], and")
    out.append("// the like. Those slots cannot take their named stub, and")
    out.append("// leaving them as they were found means leaving them null,")
    out.append("// which is the one outcome worth avoiding: the loader does")
    out.append("// not check, and calls through.")
    out.append("//")
    out.append("// So a second refusal is generated from the slot's own")
    out.append("// type. It cannot report through errcode_ret, because it")
    out.append("// does not know which parameter that is, but it returns")
    out.append("// what the return type means by failure and it is exactly")
    out.append("// the type the table wants.")
    out.append("template <typename Fn>")
    out.append("struct Refuse;")
    out.append("")
    out.append("template <typename R, typename... A>")
    out.append("struct Refuse<R(CL_API_CALL*)(A...)> {")
    out.append("    static R CL_API_CALL call(A...) {")
    out.append("        if constexpr (std::is_void_v<R>) {")
    out.append("            return;")
    out.append("        } else if constexpr (std::is_pointer_v<R>) {")
    out.append("            return nullptr;")
    out.append("        } else {")
    out.append("            return static_cast<R>(CL_INVALID_OPERATION);")
    out.append("        }")
    out.append("    }")
    out.append("};")
    out.append("")
    out.append("// Whether a slot is a function pointer at all. cl_icd.h")
    out.append("// declares the platform-tied ones as void*, and nothing")
    out.append("// can be put in those.")
    out.append("template <typename T>")
    out.append("struct IsRefusable : std::false_type {};")
    out.append("")
    out.append("template <typename R, typename... A>")
    out.append("struct IsRefusable<R(CL_API_CALL*)(A...)> : std::true_type {};")
    out.append("")
    out.append("// Fill every slot the driver did not claim.")
    out.append("//")
    out.append("// The named stub goes in when its type matches the slot")
    out.append("// exactly; the generic refusal goes in when the slot is a")
    out.append("// function pointer of some other shape; and where the")
    out.append("// header declares the slot as a bare void* — which is what")
    out.append("// it does for a version this translation unit was not")
    out.append("// built for — the named stub goes in through a cast,")
    out.append("// because a loader built for that version will call it")
    out.append("// with the signature the registry gave. Either way the")
    out.append("// slot is not left null.")
    out.append("//")
    out.append("// A template because `if constexpr` only discards what it")
    out.append("// does not take in a template: in an ordinary function")
    out.append("// the branch not taken is still type-checked, and the")
    out.append("// whole point here is that some of those branches do not")
    out.append("// type-check.")
    out.append("template <typename Table>")
    out.append("inline void fill_unimplemented(Table& table) {")
    named = set(names)
    for name in everything:
        out.append(f"    if constexpr (requires {{ table.{name}; }}) {{")
        if name in named:
            out.append(f"        if constexpr (std::is_same_v<decltype(table.{name}),")
            out.append(f"                                     decltype(&stubs::{name})>) {{")
            out.append(f"            if (table.{name} == nullptr) table.{name} = &stubs::{name};")
            out.append(f"        }} else if constexpr (IsRefusable<decltype(table.{name})>::value) {{")
            out.append(f"            if (table.{name} == nullptr)")
            out.append(f"                table.{name} = &Refuse<decltype(table.{name})>::call;")
            out.append(f"        }} else if constexpr (std::is_same_v<decltype(table.{name}), void*>) {{")
            out.append(f"            if (table.{name} == nullptr)")
            out.append(f"                table.{name} = reinterpret_cast<void*>(&stubs::{name});")
            out.append("        }")
        else:
            out.append(f"        if constexpr (IsRefusable<decltype(table.{name})>::value) {{")
            out.append(f"            if (table.{name} == nullptr)")
            out.append(f"                table.{name} = &Refuse<decltype(table.{name})>::call;")
            out.append("        }")
        out.append("    }")
    out.append("}")
    out += ["", "}  // namespace av::clproto", ""]
    write(out_dir / "cl_icd_stubs.h", "\n".join(out))
