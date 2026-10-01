"""The generated OpenGL half of the wire.

Six files come out of here.

``gl_protocol_ids.h``    one id per command, never moved once assigned
``gl_command_names``     id back to name, for logs
``gl_sizes.h``           how many values an enum answers with, and how
                         wide an index type is: the two questions the
                         registry poses as COMPSIZE and does not answer
``gl_dispatch_client``   what the guest driver calls
``gl_dispatch_server``   what the host reads, and what it calls
``gl_real_dispatch.h``   the host entry points, loaded by name

The shape follows the OpenCL generator, with one difference that runs
through all of it: a command with nothing to answer does not wait. It
goes on the ring and the caller returns. Ordering is the ring's, so a
later command that does wait sees everything before it.
"""

from __future__ import annotations

import re
from pathlib import Path

from ..emit.common import write
from .model import Command, LengthKind, Param, Registry, Role

HEADER = """// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.opengl --xml gl.xml --wgl wgl.xml
//                  --profile tools/codegen/profiles/opengl.toml --out ...
"""

# Types written as they are rather than as a varint: a float that goes
# through a varint is a float that has been rounded.
POD_SCALARS = {"GLfloat", "GLclampf", "GLdouble", "GLclampd", "GLhalf"}


def _is_pod_scalar(base: str) -> bool:
    return base in POD_SCALARS


def _ret_kind(cmd: Command) -> str:
    ret = cmd.return_type
    if ret.base == "void" and ret.pointer_depth == 0:
        return "void"
    if ret.base == "GLsync":
        return "sync"
    if ret.pointer_depth > 0:
        # const GLubyte* from glGetString and friends. There is no
        # pointer to hand over, so the text comes back as bytes and the
        # guest keeps its own copy.
        if ret.base in ("GLubyte", "GLchar", "char"):
            return "string"
        return "pointer"
    return "scalar"


# ---------------------------------------------------------------------
# Signatures
# ---------------------------------------------------------------------

# A read states the room it has: bufSize is the caller's answer to how
# many bytes there are, and asking again is a second answer to the same
# question and somewhere for the two to disagree.
#
# A compressed upload's imageSize is not that answer. It says how long
# the image is, not where in the caller's memory it begins: with the
# unpack skips set the host starts whole rows of blocks in, and those
# rows are the caller's bytes as much as the image is. So the driver
# works the length out — see compressed_bytes — and imageSize crosses
# as what it is, an argument of the call.
STATED_SIZE_PARAMS = ("bufSize",)


def _stated_size(cmd: Command) -> str | None:
    names = {p.name for p in cmd.params}
    for stated in STATED_SIZE_PARAMS:
        if stated in names:
            return stated
    return None


def _pixel_bytes_expr(cmd: Command, p: Param) -> str:
    """The byte count of a pixel parameter, in the client's scope."""
    stated = _stated_size(cmd)
    if stated is not None:
        return f"static_cast<size_t>({stated})"
    return f"{p.name}_bytes"


def fail_for(cmd: Command) -> str:
    """What a client entry point returns when it cannot send."""
    return {"void": "return;", "scalar": "return 0;", "sync": "return 0;",
            "string": "return false;", "pointer": "return nullptr;"}[_ret_kind(cmd)]


def _client_params(cmd: Command, defaults: bool = False) -> list[str]:
    out = []
    for p in cmd.params:
        if p.role is Role.IGNORED:
            continue
        if p.role is Role.SYNC:
            out.append(f"uint64_t {p.name}")
        elif p.role is Role.BUFFER_OFFSET:
            out.append(f"uint64_t {p.name}")
        elif p.role is Role.OFFSET_ARRAY:
            out.append(f"const uint64_t* {p.name}")
        elif p.role in (Role.PIXELS_IN, Role.PIXELS_OUT):
            const = "const " if p.role is Role.PIXELS_IN else ""
            out.append(f"{const}void* {p.name}")
            if _stated_size(cmd) is None:
                out.append(f"size_t {p.name}_bytes")
            # With a pixel buffer bound the pointer is an offset into
            # it, not an address in the caller — the same argument
            # meaning two different things is what the specification
            # says, so the wire has to be told which one it got. A
            # caller with no pixel buffer says nothing and gets the
            # address it always got.
            out.append(f"bool {p.name}_is_offset" +
                       (" = false" if defaults else ""))
        else:
            out.append(f"{p.type.cpp()} {p.name}")
    return out


def _client_return(cmd: Command) -> str:
    kind = _ret_kind(cmd)
    if kind == "void":
        return "void"
    if kind == "sync":
        return "uint64_t"
    if kind == "string":
        return "bool"
    return cmd.return_type.cpp()


def _client_signature(cmd: Command, defaults: bool = False) -> str:
    # The transport is av_t rather than t because GL has parameters
    # called t: glTexCoord2f(s, t) is one, and a transport named the
    # same thing shadows it in the very expression that needs both.
    params = ["::av::ClientTransport& av_t"] + _client_params(cmd, defaults)
    if _ret_kind(cmd) == "string":
        # GL answers with a pointer to storage the driver owns, and there
        # is no growable string here to put it in. So the caller lends a
        # buffer and is told how long the answer really was: a driver that
        # was handed too little sees the truncation and asks again with
        # room, rather than the wire deciding for it.
        params += ["char* out", "size_t out_capacity",
                   "size_t* out_length"]
    return f"{_client_return(cmd)} av_{cmd.name}({', '.join(params)})"


# ---------------------------------------------------------------------
# How long is it
# ---------------------------------------------------------------------

def _count_expr(p: Param) -> str:
    """C for how many elements the parameter has, in the caller's scope."""
    length = p.length
    if length is None:
        return "0"
    if length.kind is LengthKind.FIXED:
        return str(length.count)
    if length.kind in (LengthKind.PARAM, LengthKind.EXPR):
        return f"static_cast<size_t>({length.text})"
    if length.kind is LengthKind.STRING:
        return f"({p.name} != nullptr ? __builtin_strlen(reinterpret_cast<const char*>({p.name})) : 0)"
    if length.kind is LengthKind.PNAME:
        # The transport-taking overload: a few of these are lists whose
        # length only the driver knows, and a table that guessed too
        # high would write past the end of the caller's array.
        return f"::av::glproto::pname_count(av_t, {length.reads[0]})"
    if length.kind is LengthKind.TYPED_COUNT:
        count, type_name = length.reads
        return (f"(static_cast<size_t>({count}) * "
                f"::av::glproto::type_size({type_name}))")
    raise AssertionError(f"no count for {p.name}: {length.kind}")


def _storage_type(p: Param) -> str:
    """What a run of the parameter is measured and stored as.

    ``void`` is neither: a void* run is a count of bytes, which is what
    ``sizeof(void)`` means where a compiler allows it at all.
    """
    return "uint8_t" if p.type.base == "void" else p.type.base


def _bytes_expr(p: Param) -> str:
    if p.length is not None and p.length.kind is LengthKind.TYPED_COUNT:
        # Already a byte count: an index array is counted in elements of
        # a type the registry names rather than in elements of its own.
        return _count_expr(p)
    return f"({_count_expr(p)} * sizeof({_storage_type(p)}))"


# ---------------------------------------------------------------------
# ids and names
# ---------------------------------------------------------------------

def emit_ids(reg: Registry, out_dir: Path) -> None:
    out = [HEADER, "#pragma once\n", "#include <stdint.h>\n",
           "namespace av::glproto {\n"]
    for cmd in reg.commands.values():
        out.append(f"inline constexpr uint32_t kCmd_{cmd.name} = "
                   f"0x{cmd.command_id:08x};")
    out += ["", "}  // namespace av::glproto", ""]
    write(out_dir / "gl_protocol_ids.h", "\n".join(out))


# The registry writes its sized types in the names KHR/khrplatform.h
# gives them, and that header is not fetched: the registries are, and
# nothing else. Each name means one width, and a width is what the wire
# needs it for.
KHRONOS_WIDTHS = {
    "khronos_int8_t": "int8_t",
    "khronos_uint8_t": "uint8_t",
    "khronos_int16_t": "int16_t",
    "khronos_uint16_t": "uint16_t",
    "khronos_int32_t": "int32_t",
    "khronos_uint32_t": "uint32_t",
    "khronos_int64_t": "int64_t",
    "khronos_uint64_t": "uint64_t",
    "khronos_float_t": "float",
    "khronos_intptr_t": "intptr_t",
    "khronos_uintptr_t": "uintptr_t",
    "khronos_ssize_t": "ptrdiff_t",
    "khronos_usize_t": "size_t",
}


def emit_types(reg: Registry, out_dir: Path) -> None:
    """The registry's own types, rather than a GL header.

    There is no OpenGL SDK on either side of this wire - the guest is a
    Windows driver and the host is a QEMU device - and every GL header
    that exists brings a window system in with it. The registry
    declares each type itself, in dependency order, so the declarations
    are what is written out.
    """
    out = [HEADER, "#pragma once\n",
           "#include <stddef.h>",
           "#include <stdint.h>\n",
           "// The calling convention every GL entry point is declared",
           "// with, taken the way the Khronos headers take it: from the",
           "// platform where the platform has one, and nothing at all",
           "// where it does not.",
           "#if defined(_WIN32) && !defined(APIENTRY)",
           "#ifndef WIN32_LEAN_AND_MEAN",
           "#define WIN32_LEAN_AND_MEAN 1",
           "#endif",
           "#include <windows.h>",
           "#endif",
           "#ifndef APIENTRY",
           "#define APIENTRY",
           "#endif\n"]
    for text in reg.types.values():
        declaration = text.strip()
        for khronos, width in KHRONOS_WIDTHS.items():
            declaration = declaration.replace(khronos, width)
        out.append(declaration)
    out.append("")
    write(out_dir / "gl_types.h", "\n".join(out))


def emit_names(reg: Registry, out_dir: Path) -> None:
    h = [HEADER, "#pragma once\n", "#include <stdint.h>\n",
         "namespace av::glproto {\n",
         "const char* cmd_id_to_string(uint32_t cmd_id) noexcept;\n",
         "}  // namespace av::glproto", ""]
    c = [HEADER, '#include "gl_command_names.h"\n',
         '#include "gl_protocol_ids.h"\n',
         "namespace av::glproto {\n",
         "const char* cmd_id_to_string(uint32_t cmd_id) noexcept {",
         "    switch (cmd_id) {"]
    for cmd in reg.commands.values():
        c.append(f'        case kCmd_{cmd.name}: return "{cmd.name}";')
    c += ['        default: return "unknown";', "    }", "}", "",
          "}  // namespace av::glproto", ""]
    write(out_dir / "gl_command_names.h", "\n".join(h))
    write(out_dir / "gl_command_names.cpp", "\n".join(c))


# ---------------------------------------------------------------------
# The two questions COMPSIZE poses
# ---------------------------------------------------------------------

def emit_sizes(reg: Registry, out_dir: Path) -> None:
    out = [HEADER, "#pragma once\n",
           "#include <stddef.h>",
           "#include <stdint.h>\n",
           "namespace av::glproto {\n",
           "// How many values a query with this enum answers with.",
           "//",
           "// The registry says COMPSIZE(pname) and stops. Almost every",
           "// enum answers with one value; the ones that do not are",
           "// listed in the profile, and both sides read this table, so",
           "// they agree about how much came back.",
           "[[nodiscard]] inline size_t pname_count(uint32_t pname) noexcept {",
           "    switch (pname) {"]
    written: dict[int, tuple[str, int]] = {}
    for name, count in sorted(reg.pname_counts.items()):
        value = _enum_value(name)
        if value in written:
            # Two spellings of one enum. They must agree about the
            # count, because the wire only sees the number.
            first, first_count = written[value]
            if first_count != count:
                raise AssertionError(
                    f"{name} and {first} are the same enum with different "
                    f"counts ({count} and {first_count})")
            out[-1] += f", {name}"
            continue
        written[value] = (name, count)
        out.append(f"        case 0x{value:04X}: return {count};  // {name}")
    out += ["        default: return 1;",
            "    }",
            "}",
            "",
            "// How wide one element of an index or vertex array is.",
            "[[nodiscard]] inline size_t type_size(uint32_t type) noexcept {",
            "    switch (type) {",
            "        case 0x1400: return 1;  // GL_BYTE",
            "        case 0x1401: return 1;  // GL_UNSIGNED_BYTE",
            "        case 0x1402: return 2;  // GL_SHORT",
            "        case 0x1403: return 2;  // GL_UNSIGNED_SHORT",
            "        case 0x1404: return 4;  // GL_INT",
            "        case 0x1405: return 4;  // GL_UNSIGNED_INT",
            "        case 0x1406: return 4;  // GL_FLOAT",
            "        case 0x140A: return 8;  // GL_DOUBLE",
            "        case 0x140B: return 2;  // GL_HALF_FLOAT",
            "        default: return 4;",
            "    }",
            "}",
            "",
            "}  // namespace av::glproto", ""]
    write(out_dir / "gl_sizes.h", "\n".join(out))


_ENUM_VALUES: dict[str, int] = {}


def _enum_value(name: str) -> int:
    return _ENUM_VALUES.get(name, 0)


# ---------------------------------------------------------------------
# The guest's side
# ---------------------------------------------------------------------

def _lease_for_write(name: str, out: list[str], guard: str) -> None:
    out.append(f"    ::av::ClientBlobLease {name}_lease{{}};")
    out.append(f"    bool {name}_via_blob = false;")
    out.append(f"    if ({guard} &&")
    out.append(f"        {name}_want >= ::av::glproto::kPixelBlobFloor) {{")
    out.append("        auto& av_blobs = av_t.blobs();")
    out.append("        if (av_blobs.ready()) {")
    out.append(f"            {name}_lease = av_blobs.take({name}_want);")
    out.append(f"            if ({name}_lease.taken()) {{")
    out.append(f"                __builtin_memcpy({name}_lease.data(), {name}, {name}_want);")
    out.append(f"                {name}_via_blob = true;")
    out.append("            }")
    out.append("        }")
    out.append("    }")


def _write_leases(cmd: Command) -> list[str]:
    return [p.name for p in cmd.params
            if p.role is Role.PIXELS_IN or p.role is Role.ARRAY_IN]


def _read_leases(cmd: Command) -> list[str]:
    """Read-back (PIXELS_OUT) leases: given back in the reply decode on success,
    but a send that never happened must give them back too, or they leak."""
    return [p.name for p in cmd.params if p.role is Role.PIXELS_OUT]


def _hand_back_leases(cmd: Command, out: list[str], indent: str = "    ") -> None:
    """Rewinds the write leases of a command the host will not read, or has
    read already because it answered.

    A command that was sent keeps its leases: the host reads them when it
    runs the command, which for a one-way call is later than the send. They
    are retired together once an answer shows the host has run everything
    before it (KmtTransport::end_call)."""
    for name in _write_leases(cmd):
        out.append(f"{indent}if ({name}_via_blob) {{")
        out.append(f"{indent}    av_t.blobs()")
        out.append(f"{indent}        .give_back({name}_lease);")
        out.append(f"{indent}}}")


def _encode_param(cmd: Command, p: Param, out: list[str]) -> None:
    name = p.name
    if p.role is Role.IGNORED:
        return
    if p.role is Role.SCALAR:
        if _is_pod_scalar(p.type.base):
            out.append(f"    av_b.write_pod<{p.type.base}>({name});")
        else:
            out.append(f"    av_b.write_varint(static_cast<uint64_t>({name}));")
        return
    if p.role in (Role.SYNC, Role.BUFFER_OFFSET):
        out.append(f"    av_b.write_pod<uint64_t>({name});")
        return
    if p.role is Role.OFFSET_ARRAY:
        out.append(f"    av_b.write_pod<uint8_t>({name} != nullptr ? 1 : 0);")
        out.append(f"    if ({name} != nullptr) {{")
        out.append(f"        const size_t av_n = {_count_expr(p)};")
        out.append("        av_b.write_varint(static_cast<uint64_t>(av_n));")
        out.append("        for (size_t av_i = 0; av_i < av_n; ++av_i) {")
        out.append(f"            av_b.write_pod<uint64_t>({name}[av_i]);")
        out.append("        }")
        out.append("    }")
        return
    if p.role is Role.PIXELS_IN:
        size = _pixel_bytes_expr(cmd, p)
        out.append(f"    const size_t {name}_want = static_cast<size_t>({size});")
        _lease_for_write(name, out, guard=f"!{name}_is_offset && {name} != nullptr")
        out.append(f"    av_b.write_pod<uint8_t>("
                   f"{name}_is_offset ? 2 : ({name}_via_blob ? 3 : "
                   f"({name} != nullptr ? 1 : 0)));")
        out.append(f"    if ({name}_is_offset) {{")
        out.append(f"        av_b.write_varint(static_cast<uint64_t>("
                   f"reinterpret_cast<uintptr_t>({name})));")
        out.append(f"    }} else if ({name}_via_blob) {{")
        out.append(f"        av_b.write_varint(static_cast<uint64_t>({name}_want));")
        out.append(f"        av_b.write_varint(static_cast<uint64_t>({name}_lease.offset()));")
        out.append(f"    }} else if ({name} != nullptr) {{")
        out.append(f"        av_b.write_varint(static_cast<uint64_t>({name}_want));")
        out.append(f"        if ({name}_want != 0) av_b.write({name}, {name}_want);")
        out.append("    }")
        return
    if p.role is Role.PIXELS_OUT:
        size = _pixel_bytes_expr(cmd, p)
        # A read big enough to be worth a lease goes through the blob.
        #
        # The alternative is the reply slot, and the reply slot is
        # 64 KiB: a 640x480 frame is nineteen synchronous fetches, each
        # one a command out and an answer back, for bytes that both
        # sides already map. Through the blob the host writes the
        # pixels where this side can read them and the answer carries
        # nothing at all.
        #
        # Below a page it is not worth it — the allocator rounds to
        # one, so a four-byte read would take 4096 and a round trip to
        # give it back, which is worse than the reply slot it replaced.
        out.append(f"    const size_t {name}_want = static_cast<size_t>({size});")
        out.append(f"    ::av::ClientBlobLease {name}_lease{{}};")
        out.append(f"    bool {name}_via_blob = false;")
        if not cmd.is_async:
            out.append(f"    if (!{name}_is_offset && {name} != nullptr &&")
            out.append(f"        {name}_want >= ::av::glproto::kPixelBlobFloor) {{")
            out.append("        auto& av_blobs = av_t.blobs();")
            out.append("        if (av_blobs.ready()) {")
            out.append(f"            {name}_lease = av_blobs.take({name}_want);")
            out.append(f"            {name}_via_blob = {name}_lease.taken();")
            # Cleared before the host writes into it.
            #
            # A read is allowed to ask for more than there is: a caller
            # may read a region larger than the framebuffer, and what
            # lands outside it is undefined. Undefined has to mean
            # zero here rather than whatever the arena last held,
            # because the arena last held somebody else's pixels — so
            # the alternative is handing an application another
            # program's frame, and a picture that changes with what ran
            # before it. The host writes zeros there, and this is how
            # it looks like the host.
            out.append(f"            if ({name}_via_blob) "
                       f"__builtin_memset({name}_lease.data(), 0, {name}_want);")
            out.append("        }")
            out.append("    }")
        out.append(f"    av_b.write_pod<uint8_t>(")
        out.append(f"        {name}_is_offset ? 2 : ({name}_via_blob ? 3 : "
                   f"({name} != nullptr ? 1 : 0)));")
        out.append(f"    if ({name}_is_offset) {{")
        out.append(f"        av_b.write_varint(static_cast<uint64_t>("
                   f"reinterpret_cast<uintptr_t>({name})));")
        out.append(f"    }} else if ({name}_via_blob) {{")
        out.append(f"        av_b.write_varint(static_cast<uint64_t>({name}_want));")
        out.append(f"        av_b.write_varint(static_cast<uint64_t>({name}_lease.offset()));")
        out.append(f"    }} else if ({name} != nullptr) {{")
        out.append(f"        av_b.write_varint(static_cast<uint64_t>({name}_want));")
        out.append("    }")
        return
    if p.role is Role.STRING_IN:
        out.append(f"    av_b.write_pod<uint8_t>({name} != nullptr ? 1 : 0);")
        out.append(f"    if ({name} != nullptr) {{")
        out.append(f"        const size_t av_n = {_count_expr(p)};")
        out.append("        av_b.write_varint(static_cast<uint64_t>(av_n));")
        out.append(f"        if (av_n != 0) av_b.write({name}, av_n);")
        out.append("    }")
        return
    if p.role is Role.STRING_ARRAY_IN:
        count = _count_expr(p)
        out.append(f"    av_b.write_pod<uint8_t>({name} != nullptr ? 1 : 0);")
        out.append(f"    if ({name} != nullptr) {{")
        out.append(f"        const size_t av_n = {count};")
        out.append("        av_b.write_varint(static_cast<uint64_t>(av_n));")
        out.append("        for (size_t av_i = 0; av_i < av_n; ++av_i) {")
        out.append(f"            const auto* av_s = {name}[av_i];")
        if p.has_lengths:
            # glShaderSource: a negative length, or none at all, means
            # the string is terminated rather than counted.
            out.append("            const size_t av_len = av_s == nullptr ? 0")
            out.append("                : (length != nullptr && length[av_i] >= 0)")
            out.append("                    ? static_cast<size_t>(length[av_i])")
            out.append("                    : __builtin_strlen(reinterpret_cast<const char*>(av_s));")
        else:
            out.append("            const size_t av_len = av_s == nullptr ? 0")
            out.append("                : __builtin_strlen(reinterpret_cast<const char*>(av_s));")
        out.append("            av_b.write_varint(static_cast<uint64_t>(av_len));")
        out.append("            if (av_len != 0) av_b.write(av_s, av_len);")
        out.append("        }")
        out.append("    }")
        return
    if p.role is Role.ARRAY_IN:
        out.append(f"    size_t {name}_want = 0;")
        out.append(f"    if ({name} != nullptr) {name}_want = {_bytes_expr(p)};")
        _lease_for_write(name, out, guard=f"{name} != nullptr")
        out.append(f"    av_b.write_pod<uint8_t>("
                   f"{name} == nullptr ? 0 : ({name}_via_blob ? 3 : 1));")
        out.append(f"    if ({name}_via_blob) {{")
        out.append(f"        av_b.write_varint(static_cast<uint64_t>({name}_want));")
        out.append(f"        av_b.write_varint(static_cast<uint64_t>({name}_lease.offset()));")
        out.append(f"    }} else if ({name} != nullptr) {{")
        out.append(f"        av_b.write_varint(static_cast<uint64_t>({name}_want));")
        out.append(f"        if ({name}_want != 0) av_b.write({name}, {name}_want);")
        out.append("    }")
        return
    if p.role in (Role.ARRAY_OUT, Role.STRING_OUT):
        # Worked out once, on the way out, and remembered: the reply
        # is measured against it, and a length that reads the driver's
        # own state - how many values this enum answers with - would
        # be a second question over the wire to bound the first one's
        # answer.
        out.append(f"    const size_t {name}_want = "
                   f"{name} != nullptr ? {_bytes_expr(p)} : 0u;")
        out.append(f"    av_b.write_pod<uint8_t>({name} != nullptr ? 1 : 0);")
        out.append(f"    if ({name} != nullptr) {{")
        out.append(f"        av_b.write_varint(static_cast<uint64_t>({name}_want));")
        out.append("    }")
        return
    raise AssertionError(f"no encoder for {p.role}")


def _decode_reply(cmd: Command, out: list[str]) -> None:
    kind = _ret_kind(cmd)
    fail = {"void": "return;", "scalar": "return 0;", "sync": "return 0;",
            "string": "return false;", "pointer": "return nullptr;"}[kind]
    if kind == "scalar":
        out.append("    uint64_t av_ret = 0;")
        out.append(f"    if (!av_r.read_varint(av_ret)) {fail}")
    elif kind == "sync":
        out.append("    uint64_t av_ret = 0;")
        out.append(f"    if (!av_r.read_pod(av_ret)) {fail}")
    elif kind == "string":
        out.append("    uint64_t av_present = 0;")
        out.append(f"    if (!av_r.read_varint(av_present)) {fail}")
        out.append("    uint64_t length = 0;")
        out.append(f"    if (!av_r.read_varint(length)) {fail}")
        # The length is the host's word for it, and the reply is the
        # only thing that bounds it.
        out.append(f"    if (length > av_r.remaining()) {fail}")
        # The length the host has, whatever the caller had room for, so a
        # caller holding nothing can ask with a null buffer and then ask
        # again with the right one.
        out.append("    if (out_length != nullptr) "
                   "*out_length = static_cast<size_t>(length);")
        out.append("    size_t av_kept = 0;")
        out.append("    if (out != nullptr && out_capacity != 0) {")
        out.append("        av_kept = static_cast<size_t>(length) < out_capacity")
        out.append("            ? static_cast<size_t>(length) : out_capacity - 1;")
        out.append(f"        if (av_kept != 0 && !av_r.read(out, av_kept)) {fail}")
        # Terminated, because what the caller hands its own caller back is
        # a const GLubyte* and nothing carries the length with it.
        out.append("        out[av_kept] = '\\0';")
        out.append("    }")
        # Whatever did not fit is still on the reply, and anything the
        # command answers with follows it.
        out.append("    for (uint64_t av_i = av_kept; av_i < length; ++av_i) {")
        out.append("        uint8_t av_byte = 0;")
        out.append(f"        if (!av_r.read_pod(av_byte)) {fail}")
        out.append("    }")
    for p in cmd.params:
        if p.role in (Role.ARRAY_OUT, Role.STRING_OUT):
            # The caller's array is the bound, not the reply's own
            # count: a host that answers with more than was asked for
            # is answering past the end of somebody's buffer.
            out.append(f"    if ({p.name} != nullptr) {{")
            out.append("        uint64_t av_bytes = 0;")
            out.append(f"        if (!av_r.read_varint(av_bytes)) {fail}")
            out.append(f"        if (av_bytes > static_cast<uint64_t>({p.name}_want)) {fail}")
            out.append("        if (av_bytes != 0 &&")
            out.append(f"            !av_r.read({p.name}, static_cast<size_t>(av_bytes))) {fail}")
            out.append("    }")
        elif p.role is Role.PIXELS_OUT:
            # Through the blob the pixels are already here: the host
            # wrote them into memory this side lent it, so all that is
            # left is to hand them to the caller and give the lease
            # back.
            out.append(f"    if ({p.name}_via_blob) {{")
            out.append(f"        __builtin_memcpy({p.name}, {p.name}_lease.data(), {p.name}_want);")
            out.append("        av_t.blobs()")
            out.append(f"            .give_back({p.name}_lease);")
            out.append("    }")
            # A read into a pixel buffer lands on the host and stays
            # there; nothing comes back for the caller to be given.
            out.append(f"    if ({p.name} != nullptr && !{p.name}_is_offset && "
                       f"!{p.name}_via_blob) {{")
            out.append("        uint64_t av_bytes = 0;")
            out.append(f"        if (!av_r.read_varint(av_bytes)) {fail}")
            out.append(f"        if (av_bytes > static_cast<uint64_t>({p.name}_want)) {fail}")
            out.append("        if (av_bytes != 0 &&")
            out.append(f"            !av_r.read({p.name}, static_cast<size_t>(av_bytes))) {fail}")
            out.append("    }")
    if kind == "scalar":
        out.append(f"    return static_cast<{cmd.return_type.cpp()}>(av_ret);")
    elif kind == "sync":
        out.append("    return av_ret;")
    elif kind == "string":
        out.append("    return av_present != 0;")


def _into_blob_variant(cmd: Command, h: list[str], c: list[str]) -> None:
    """av_<name>_into_blob: the read sent one-way, its pixels written by
    the host at `av_blob_at` of the arena. See Registry.read_into_blob."""
    pixels = [p for p in cmd.params if p.role is Role.PIXELS_OUT]
    size = _stated_size(cmd)
    if len(pixels) != 1 or size is None or _ret_kind(cmd) != "void":
        raise GenerationError(f"read_into_blob: {cmd.name} is not a void read of one stated-size pixel array")
    out = pixels[0]
    params = ["::av::ClientTransport& av_t"]
    for p in cmd.params:
        if p.role is Role.IGNORED or p is out:
            continue
        params.append(f"{p.type.cpp()} {p.name}")
    params.append("uint64_t av_blob_at")
    signature = f"void av_{cmd.name}_into_blob({', '.join(params)})"
    h.append(f"{signature};")
    c.append(f"{signature} {{")
    c.append(f'    AV_SCOPE("av_{cmd.name}_into_blob");')
    c.append("    ::av::WireWriter av_b = av_t.request_writer();")
    for p in cmd.params:
        if p is out:
            # Mode 3, as the synchronous read spells a blob lease: the
            # server cannot tell the two apart, and answers neither
            # through the reply slot.
            c.append("    av_b.write_pod<uint8_t>(3);")
            c.append(f"    av_b.write_varint(static_cast<uint64_t>({size}));")
            c.append("    av_b.write_varint(av_blob_at);")
        else:
            _encode_param(cmd, p, c)
    c.append("    if (av_b.status() != ::av::Status::ok) {")
    c.append(f'        ::av::icd::payload_dropped("gl", "{cmd.name}",')
    c.append("                                       av_b.offset());")
    c.append("        av_t.end_call();")
    c.append("        return;")
    c.append("    }")
    c.append(f"    (void)av_t.send_async(kCmd_{cmd.name}, av_b);")
    c.append("}")
    c.append("")


def emit_client(reg: Registry, out_dir: Path) -> None:
    h = [HEADER, "#pragma once\n",
         "#include <stddef.h>", "#include <stdint.h>\n",
         '#include "proto/client_transport.hpp"',
         '#include "gl_types.h"\n',
         "namespace av::glproto {\n"]
    c = [HEADER, '#include "gl_dispatch_client.h"\n',
         '#include "core/support/status.hpp"',
         '#include "core/trace/function_scope.hpp"',
         '#include "proto/wire/wire_writer.hpp"',
         '#include "proto/wire/wire_reader.hpp"',
         '#include "proto/icd_stub_report.hpp"',
         '#include "gl_helpers.h"',
         '#include "gl_protocol_ids.h"',
         '#include "gl_sizes.h"',
         '#include "proto/client_blob_arena.hpp"',
         '#include "proto/sync_call_scope.hpp"',
         '',
         "namespace av::glproto {\n",
         "// Below one page a blob lease costs more than the reply slot",
         "// it would replace: the allocator rounds to a page either way.",
         "inline constexpr size_t kPixelBlobFloor = 4096;\n"]
    for cmd in reg.commands.values():
        h.append(f"{_client_signature(cmd, defaults=True)};")
        if cmd.name in reg.answered_by_guest:
            # Declared and called like any other, defined by hand: the
            # answer is in the guest already and asking for it would be
            # the round trip this exists to remove. See
            # `answered_by_guest` in gl_profile.toml.
            continue
        c.append(f"{_client_signature(cmd)} {{")
        # One trace scope per command sent (docs/trace.md).
        c.append(f'    AV_SCOPE("av_{cmd.name}");')
        c.append("    ::av::WireWriter av_b = av_t.request_writer();")
        if not cmd.is_async:
            # Ends the call on every path from here, a payload dropped
            # before the send included: a transport shared between threads
            # holds its lock from the writer to this (feature 050).
            c.append("    const ::av::SyncCallScope av_call{av_t};")
        if cmd.mints_handle:
            # The handle the guest is about to answer with, written
            # first so the server binds it before it decodes anything
            # that could fail. See the av_sync_* fields on RealGl for
            # why the guest gets to choose it at all.
            c.append("    const uint64_t av_token = "
                     "::av::glproto::mint_sync_token();")
            c.append("    av_b.write_pod<uint64_t>(av_token);")
        for p in cmd.params:
            _encode_param(cmd, p, c)
        # A buffer that dropped a write must not be sent.
        #
        # Buffer records a failed write rather than throwing — it sits
        # under noexcept generated code — and nothing read that flag.
        # What went out instead was a header saying "four thousand
        # bytes of pixels follow" with no pixels behind it, and the
        # server answered "undecodable command" and dropped the whole
        # thing. That is a tile of an image that silently never
        # arrived. Sending nothing is no better for the picture, but it
        # is honest, and it says so once.
        c.append("    if (av_b.status() != ::av::Status::ok) {")
        c.append(f'        ::av::icd::payload_dropped("gl", "{cmd.name}",')
        c.append("                                       av_b.offset());")
        _hand_back_leases(cmd, c, indent="        ")
        if cmd.is_async:
            # No send will let the writer's lock go, so the call ends here.
            c.append("        av_t.end_call();")
        c.append(f"        {'return;' if _ret_kind(cmd) == 'void' else fail_for(cmd)}")
        c.append("    }")
        if cmd.is_async:
            c.append(f"    (void)av_t.send_async(kCmd_{cmd.name}, av_b);")
            if cmd.mints_handle:
                c.append("    return av_token;")
            c.append("}")
        else:
            kind = _ret_kind(cmd)
            fail = fail_for(cmd)
            c.append("    ::av::WireReader av_r{nullptr, 0u};")
            c.append(f"    const bool av_sent = av_t.send_sync(kCmd_{cmd.name}, av_b, av_r) ==")
            c.append("        ::av::TransportResult::ok;")
            _hand_back_leases(cmd, c)
            c.append("    if (!av_sent) {")
            for rname in _read_leases(cmd):
                c.append(f"        if ({rname}_via_blob) av_t.blobs().give_back({rname}_lease);")
            c.append(f"        {fail}")
            c.append("    }")
            _decode_reply(cmd, c)
            c.append("}")
        c.append("")
        if cmd.name in reg.read_into_blob:
            _into_blob_variant(cmd, h, c)
    h += ["", "}  // namespace av::glproto", ""]
    c += ["}  // namespace av::glproto", ""]
    write(out_dir / "gl_dispatch_client.h", "\n".join(h))
    write(out_dir / "gl_dispatch_client.cpp", "\n".join(c))


# ---------------------------------------------------------------------
# The host's side
# ---------------------------------------------------------------------

def _decode_param(p: Param, out: list[str]) -> None:
    name = p.name
    if p.role is Role.IGNORED:
        out.append(f"    {p.type.cpp()} {name} = nullptr;")
        return
    if p.role is Role.SCALAR:
        if _is_pod_scalar(p.type.base):
            out.append(f"    {p.type.base} {name}{{}};")
            out.append(f"    if (!av_r.read_pod({name})) return false;")
        else:
            out.append(f"    {p.type.cpp()} {name}{{}};")
            out.append("    { uint64_t av_v = 0; if (!av_r.read_varint(av_v)) return false;")
            out.append(f"      {name} = static_cast<{p.type.cpp()}>(av_v); }}")
        return
    if p.role is Role.SYNC:
        # The guest's token, not the host's pointer. The table says
        # which of the host's fences it stands for; an unknown token
        # resolves to null, which is what GL does with a handle that was
        # never a handle. See the av_sync_* fields on RealGl.
        out.append(f"    uint64_t {name}_token = 0;")
        out.append(f"    if (!av_r.read_pod({name}_token)) return false;")
        out.append(f"    GLsync {name} = static_cast<GLsync>("
                   f"(av_gl.av_sync_for == nullptr ? nullptr : av_gl.av_sync_for(av_gl.av_sync_user, {name}_token)));")
        return
    if p.role is Role.OFFSET_ARRAY:
        out.append(f"    const void** {name} = nullptr;")
        out.append(f"    uint64_t {name}_count = 0;")
        out.append("    {")
        out.append("        uint8_t av_present = 0;")
        out.append("        if (!av_r.read_pod(av_present)) return false;")
        out.append("        if (av_present != 0) {")
        out.append("            uint64_t av_n = 0;")
        out.append("            if (!av_r.read_varint(av_n)) return false;")
        out.append("            auto** av_offsets = av_arena.allocate<const void*>(static_cast<size_t>(av_n) + 1);")
        out.append("            if (av_offsets == nullptr) return false;")
        out.append("            for (uint64_t av_i = 0; av_i < av_n; ++av_i) {")
        out.append("                uint64_t av_v = 0;")
        out.append("                if (!av_r.read_pod(av_v)) return false;")
        out.append("                av_offsets[av_i] = reinterpret_cast<const void*>(static_cast<uintptr_t>(av_v));")
        out.append("            }")
        out.append(f"            {name} = av_offsets;")
        out.append(f"            {name}_count = av_n;")
        out.append("        }")
        out.append("    }")
        return
    if p.role is Role.BUFFER_OFFSET:
        out.append(f"    const void* {name} = nullptr;")
        out.append("    { uint64_t av_v = 0; if (!av_r.read_pod(av_v)) return false;")
        out.append(f"      {name} = reinterpret_cast<const void*>(static_cast<uintptr_t>(av_v)); }}")
        return
    if p.role is Role.PIXELS_IN:
        # Out of the caller's block, like everything else a decoded
        # command points at: there is no heap on this side. An upload
        # larger than the block is refused here rather than truncated,
        # because a texture that arrives short is a texture that is
        # wrong for the rest of the run. A guest with a level that
        # large sends it through the blob, which is what the 3 case
        # above is.
        out.append(f"    uint8_t* {name}_storage = nullptr;")
        out.append(f"    size_t {name}_storage_count = 0;")
        out.append(f"    const void* {name} = nullptr;")
        out.append(f"    uint64_t {name}_room = 0;")
        out.append(f"    bool {name}_is_offset = false;")
        out.append("    {")
        out.append("        uint8_t av_present = 0;")
        out.append("        if (!av_r.read_pod(av_present)) return false;")
        out.append("        if (av_present == 2) {")
        out.append("            uint64_t av_offset = 0;")
        out.append("            if (!av_r.read_varint(av_offset)) return false;")
        out.append(f"            {name} = reinterpret_cast<const void*>("
                   "static_cast<uintptr_t>(av_offset));")
        out.append(f"            {name}_is_offset = true;")
        out.append("        } else if (av_present == 3) {")
        out.append("            uint64_t av_bytes = 0;")
        out.append("            uint64_t av_at = 0;")
        out.append("            if (!av_r.read_varint(av_bytes)) return false;")
        out.append("            if (!av_r.read_varint(av_at)) return false;")
        out.append("            if (av_gl.av_blob == nullptr) return false;")
        out.append(f"            {name} = av_gl.av_blob(av_gl.av_blob_user, av_at, av_bytes);")
        out.append(f"            if ({name} == nullptr) return false;")
        out.append(f"            {name}_room = av_bytes;")
        out.append("        } else if (av_present != 0) {")
        out.append("            uint64_t av_bytes = 0;")
        out.append("            if (!av_r.read_varint(av_bytes)) return false;")
        out.append("            if (av_bytes > av_r.remaining()) return false;")
        out.append(f"            {name}_room = av_bytes;")
        out.append(f"            {name}_storage_count = static_cast<size_t>(av_bytes);")
        out.append(f"            if ({name}_storage_count != 0) {{")
        out.append(f"                {name}_storage = av_arena.allocate<uint8_t>("
                   f"{name}_storage_count);")
        out.append(f"                if ({name}_storage == nullptr) return false;")
        # Every byte of it is about to be overwritten by the read, so it
        # is the one block that is not cleared first.
        out.append(f"                if (!av_r.read({name}_storage, "
                   f"{name}_storage_count)) return false;")
        out.append("            }")
        out.append(f"            {name} = {name}_storage;")
        out.append("        }")
        out.append("    }")
        return
    if p.role is Role.PIXELS_OUT:
        # Out of the caller's block. A read too large for it is refused
        # rather than answered short: the guest sizes its own buffer
        # from the same arguments, so a truncated answer would be read
        # back as pixels. A frame-sized read goes through the blob and
        # never lands here at all — that is the 3 case below, and the
        # guest takes it for anything above a page.
        out.append(f"    uint8_t* {name}_storage = nullptr;")
        out.append(f"    size_t {name}_storage_count = 0;")
        out.append(f"    void* {name} = nullptr;")
        out.append(f"    size_t {name}_bytes = 0;")
        out.append(f"    bool {name}_is_offset = false;")
        out.append(f"    bool {name}_via_blob = false;")
        out.append("    {")
        out.append("        uint8_t av_present = 0;")
        out.append("        if (!av_r.read_pod(av_present)) return false;")
        out.append("        if (av_present == 2) {")
        out.append("            uint64_t av_offset = 0;")
        out.append("            if (!av_r.read_varint(av_offset)) return false;")
        out.append(f"            {name} = reinterpret_cast<void*>("
                   "static_cast<uintptr_t>(av_offset));")
        out.append(f"            {name}_is_offset = true;")
        out.append("        } else if (av_present == 3) {")
        # The guest lent a range of the region; GL writes into it
        # directly and nothing goes back through the reply.
        out.append("            uint64_t av_bytes = 0;")
        out.append("            uint64_t av_at = 0;")
        out.append("            if (!av_r.read_varint(av_bytes)) return false;")
        out.append("            if (!av_r.read_varint(av_at)) return false;")
        out.append("            if (av_gl.av_blob == nullptr) return false;")
        # Bounds-checked against the whole mapping by the resolver,
        # which is the server's only defence against a guest that lies
        # about where its memory is.
        out.append(f"            {name} = av_gl.av_blob("
                   "av_gl.av_blob_user, av_at, av_bytes);")
        out.append(f"            if ({name} == nullptr) return false;")
        out.append(f"            {name}_bytes = static_cast<size_t>(av_bytes);")
        out.append(f"            {name}_via_blob = true;")
        out.append("        } else if (av_present != 0) {")
        out.append("            uint64_t av_bytes = 0;")
        out.append("            if (!av_r.read_varint(av_bytes)) return false;")
        out.append(f"            {name}_bytes = static_cast<size_t>(av_bytes);")
        # One past what was asked for, which also makes the overflow
        # harmless: a length of SIZE_MAX wraps the count to zero, the
        # arena answers null for that, and the command is refused
        # rather than handed a block smaller than it was told.
        out.append(f"            {name}_storage_count = {name}_bytes + 1;")
        out.append(f"            {name}_storage = av_arena.allocate<uint8_t>("
                   f"{name}_storage_count);")
        out.append(f"            if ({name}_storage == nullptr) return false;")
        # Cleared, because the driver may write fewer bytes than were
        # asked for and the reply sends back what is there: whatever
        # the block last held is another guest's picture.
        out.append(f"            for (size_t av_z = 0; "
                   f"av_z < {name}_storage_count; ++av_z) {{ "
                   f"{name}_storage[av_z] = 0; }}")
        out.append(f"            {name} = {name}_storage;")
        out.append("        }")
        out.append("    }")
        return
    if p.role is Role.STRING_IN:
        out.append(f"    const {p.type.base}* {name} = nullptr;")
        out.append(f"    uint64_t {name}_length = 0;")
        out.append("    {")
        out.append("        uint8_t av_present = 0;")
        out.append("        if (!av_r.read_pod(av_present)) return false;")
        out.append("        if (av_present != 0) {")
        out.append("            uint64_t av_n = 0;")
        out.append("            if (!av_r.read_varint(av_n)) return false;")
        # A length out of the frame buys room in the arena, so what is
        # left of the frame bounds it before it is spent.
        out.append("            if (av_n > av_r.remaining()) return false;")
        out.append(f"            auto* av_text = av_arena.allocate<{p.type.base}>(static_cast<size_t>(av_n) + 1);")
        out.append("            if (av_text == nullptr) return false;")
        out.append("            if (av_n != 0 &&")
        out.append("                !av_r.read(av_text, static_cast<size_t>(av_n))) return false;")
        out.append("            av_text[av_n] = 0;")
        out.append(f"            {name} = av_text;")
        out.append(f"            {name}_length = av_n;")
        out.append("        }")
        out.append("    }")
        return
    if p.role is Role.STRING_ARRAY_IN:
        base = p.type.base
        out.append(f"    const {base}** {name} = nullptr;")
        out.append(f"    uint64_t {name}_count = 0;")
        if p.has_lengths:
            out.append(f"    uint64_t* {name}_lengths = nullptr;")
        out.append("    {")
        out.append("        uint8_t av_present = 0;")
        out.append("        if (!av_r.read_pod(av_present)) return false;")
        out.append("        if (av_present != 0) {")
        out.append("            uint64_t av_n = 0;")
        out.append("            if (!av_r.read_varint(av_n)) return false;")
        out.append("            if (av_n > av_r.remaining()) return false;")
        out.append(f"            auto** av_strings = av_arena.allocate<{base}*>(static_cast<size_t>(av_n) + 1);")
        out.append("            if (av_strings == nullptr) return false;")
        if p.has_lengths:
            # What each string holds, for the lengths the call is also
            # handed: the driver reads as far as a length says.
            out.append(f"            {name}_lengths = av_arena.allocate<uint64_t>(static_cast<size_t>(av_n) + 1);")
            out.append(f"            if ({name}_lengths == nullptr) return false;")
        out.append("            for (uint64_t av_i = 0; av_i < av_n; ++av_i) {")
        out.append("                uint64_t av_len = 0;")
        out.append("                if (!av_r.read_varint(av_len)) return false;")
        out.append("                if (av_len > av_r.remaining()) return false;")
        out.append(f"                auto* av_text = av_arena.allocate<{base}>(static_cast<size_t>(av_len) + 1);")
        out.append("                if (av_text == nullptr) return false;")
        out.append("                if (av_len != 0 &&")
        out.append("                    !av_r.read(av_text, static_cast<size_t>(av_len))) return false;")
        out.append("                av_text[av_len] = 0;")
        out.append("                av_strings[av_i] = av_text;")
        if p.has_lengths:
            out.append(f"                {name}_lengths[av_i] = av_len;")
        out.append("            }")
        out.append(f"            {name} = const_cast<const {base}**>(av_strings);")
        out.append(f"            {name}_count = av_n;")
        out.append("        }")
        out.append("    }")
        return
    if p.role is Role.ARRAY_IN:
        storage = _storage_type(p)
        queries = p.length is not None and p.length.kind is LengthKind.PNAME
        out.append(f"    const {p.type.base}* {name} = nullptr;")
        out.append(f"    uint64_t {name}_room = 0;")
        out.append("    {")
        out.append("        uint8_t av_present = 0;")
        out.append("        if (!av_r.read_pod(av_present)) return false;")
        out.append("        if (av_present == 3) {")
        if queries:
            # A handful of values, which only ever cross inline: see
            # below for why they have to be copied.
            out.append("            return false;")
        else:
            out.append("            uint64_t av_bytes = 0;")
            out.append("            uint64_t av_at = 0;")
            out.append("            if (!av_r.read_varint(av_bytes)) return false;")
            out.append("            if (!av_r.read_varint(av_at)) return false;")
            out.append("            if (av_gl.av_blob == nullptr) return false;")
            out.append("            const void* av_from ="
                       " av_gl.av_blob(av_gl.av_blob_user, av_at, av_bytes);")
            out.append("            if (av_from == nullptr) return false;")
            out.append(f"            {name} = reinterpret_cast<const {p.type.base}*>(av_from);")
            out.append(f"            {name}_room = av_bytes;")
        out.append("        } else if (av_present != 0) {")
        out.append("            uint64_t av_bytes = 0;")
        out.append("            if (!av_r.read_varint(av_bytes)) return false;")
        out.append("            if (av_bytes > av_r.remaining()) return false;")
        out.append(f"            {name}_room = av_bytes;")
        # Room for the run as the element type, so that it is aligned
        # for the one the call reads it as. A block of bytes is aligned
        # for nothing.
        out.append(f"            size_t av_elements = static_cast<size_t>(av_bytes) / sizeof({storage}) + 1;")
        if queries:
            # How many values a parameter enum takes comes from the same
            # table a query's answer does, and it can be short the same
            # way: the driver reads as many as the enum really has. So
            # the run is never shorter than the largest, and zero past
            # what the guest sent.
            out.append("            if (av_elements < kQuerySlack + 1) av_elements = kQuerySlack + 1;")
        out.append(f"            auto* av_run = av_arena.allocate<{storage}>(av_elements);")
        out.append("            if (av_run == nullptr) return false;")
        if queries:
            out.append("            for (size_t av_z = 0; av_z < av_elements; ++av_z) { av_run[av_z] = {}; }")
        out.append("            if (av_bytes != 0 &&")
        out.append("                !av_r.read(av_run, static_cast<size_t>(av_bytes))) return false;")
        out.append(f"            {name} = reinterpret_cast<const {p.type.base}*>(av_run);")
        out.append("        }")
        out.append("    }")
        return
    if p.role in (Role.ARRAY_OUT, Role.STRING_OUT):
        # Only what the guest asked for is read here. The room the call
        # writes into is made once every argument is known - see
        # _output_room - because how much the driver writes is a
        # question of those arguments and not of the guest's count.
        storage = _storage_type(p)
        out.append(f"    {storage}* {name}_storage = nullptr;")
        out.append(f"    size_t {name}_storage_count = 0;")
        out.append(f"    {p.type.base}* {name} = nullptr;")
        out.append(f"    size_t {name}_bytes = 0;")
        out.append(f"    bool {name}_answered = false;")
        out.append("    {")
        out.append("        uint8_t av_present = 0;")
        out.append("        if (!av_r.read_pod(av_present)) return false;")
        out.append("        if (av_present != 0) {")
        out.append(f"            {name}_answered = true;")
        out.append("            uint64_t av_bytes = 0;")
        out.append("            if (!av_r.read_varint(av_bytes)) return false;")
        out.append(f"            {name}_bytes = static_cast<size_t>(av_bytes);")
        out.append("        }")
        out.append("    }")
        return
    raise AssertionError(f"no decoder for {p.role}")


# ---------------------------------------------------------------------
# What a read-back may write
#
# A pixel read writes as many bytes as GL works out - the format, the
# type, the extent and the pack state - at whatever pointer it is given.
# The guest's own count of them is a number off the wire, so before any
# read the server works the count out again and refuses a destination
# smaller than it (feature 044, research section 2). An offset is only
# an offset while a buffer is bound; without one GL writes at the
# guest's number as an address, so that is refused too.
# ---------------------------------------------------------------------

GL_PIXEL_PACK_BUFFER_BINDING = "0x88ED"
GL_QUERY_BUFFER_BINDING = "0x9193"
GL_TEXTURE_WIDTH, GL_TEXTURE_HEIGHT, GL_TEXTURE_DEPTH = "0x1000", "0x1001", "0x8071"

PACK_HELPERS = [
    "// The bytes one pixel of this format and type packs to, or 0 for a",
    "// pairing this server does not size, which it then does not read into.",
    "static size_t pixel_bytes(GLenum format, GLenum type) noexcept {",
    "    switch (type) {",
    "        case 0x8032: case 0x8362: return 1;  // UNSIGNED_BYTE_3_3_2, _2_3_3_REV",
    "        case 0x8363: case 0x8364: case 0x8033: case 0x8365:",
    "        case 0x8034: case 0x8366: return 2;  // the 5_6_5, 4_4_4_4 and 5_5_5_1 packings",
    "        case 0x8035: case 0x8367: case 0x8036: case 0x8368:",
    "        case 0x84FA: case 0x8C3B: case 0x8C3E: return 4;  // 8_8_8_8, 10_10_10_2, 24_8, 10F_11F_11F, 5_9_9_9",
    "        case 0x8DAD: return 8;  // FLOAT_32_UNSIGNED_INT_24_8_REV",
    "        default: break;",
    "    }",
    "    size_t component = 0;",
    "    switch (type) {",
    "        case 0x1400: case 0x1401: component = 1; break;  // BYTE, UNSIGNED_BYTE",
    "        case 0x1402: case 0x1403: case 0x140B: component = 2; break;  // SHORT, UNSIGNED_SHORT, HALF_FLOAT",
    "        case 0x1404: case 0x1405: case 0x1406: component = 4; break;  // INT, UNSIGNED_INT, FLOAT",
    "        default: return 0;",
    "    }",
    "    switch (format) {",
    "        case 0x1900: case 0x1901: case 0x1902: case 0x1903: case 0x1904:",
    "        case 0x1905: case 0x1906: case 0x1909: case 0x8D94: case 0x8D95:",
    "        case 0x8D96: case 0x8D97: return component;  // one channel, or a depth or stencil index",
    "        case 0x8227: case 0x8228: case 0x190A: return 2 * component;  // RG, RG_INTEGER, LUMINANCE_ALPHA",
    "        case 0x1907: case 0x80E0: case 0x8D98: case 0x8D9A: return 3 * component;  // RGB, BGR and integers",
    "        case 0x1908: case 0x80E1: case 0x8D99: case 0x8D9B: return 4 * component;  // RGBA, BGRA and integers",
    "        default: return 0;",
    "    }",
    "}",
    "",
    "static bool pack_state(const RealGl& av_gl, GLenum pname, uint64_t& value) noexcept {",
    "    GLint answer = -1;",
    "    av_gl.glGetIntegerv(pname, &answer);",
    "    if (answer < 0) return false;",
    "    value = static_cast<uint64_t>(answer);",
    "    return true;",
    "}",
    "",
    "// Where a transfer's pixel store lives: alignment, skip pixels, skip rows,",
    "// skip images, row length and image height, for packing and unpacking.",
    "static constexpr GLenum kPackStore[6] = {0x0D05, 0x0D04, 0x0D03, 0x806B, 0x0D02, 0x806C};",
    "static constexpr GLenum kUnpackStore[6] = {0x0CF5, 0x0CF4, 0x0CF3, 0x806D, 0x0CF2, 0x806E};",
    "",
    "struct StoreLayout {",
    "    uint64_t alignment, skip_pixels, skip_rows, skip_images, row_length, image_height;",
    "};",
    "",
    "static bool store_layout(const RealGl& av_gl, const GLenum* store, StoreLayout& layout) noexcept {",
    "    if (av_gl.glGetIntegerv == nullptr) return false;",
    "    if (!pack_state(av_gl, store[0], layout.alignment) || !pack_state(av_gl, store[1], layout.skip_pixels) ||",
    "        !pack_state(av_gl, store[2], layout.skip_rows) || !pack_state(av_gl, store[3], layout.skip_images) ||",
    "        !pack_state(av_gl, store[4], layout.row_length) || !pack_state(av_gl, store[5], layout.image_height)) return false;",
    "    const uint64_t alignment = layout.alignment;",
    "    return alignment != 0 && alignment <= 8 && (alignment & (alignment - 1)) == 0;",
    "}",
    "",
    "// The bytes a transfer reaches at its pointer: every image before the last",
    "// whole, every row before the last padded to the alignment, the last row",
    "// only as far as the extent reaches, all of it after the skips. Skipped",
    "// images count even for a flat image, which GL ignores: a bound a little",
    "// high refuses nothing a real caller sends.",
    "static bool store_bytes(const RealGl& av_gl, const GLenum* store, int64_t width, int64_t height,",
    "                        int64_t depth, GLenum format, GLenum type, uint64_t& bytes) noexcept {",
    "    bytes = 0;",
    "    if (width < 0 || height < 0 || depth < 0) return false;",
    "    if (width == 0 || height == 0 || depth == 0) return true;",
    "    const uint64_t pixel = pixel_bytes(format, type);",
    "    StoreLayout layout{};",
    "    if (pixel == 0 || !store_layout(av_gl, store, layout)) return false;",
    "    const uint64_t alignment = layout.alignment;",
    "    const uint64_t columns = layout.row_length != 0 ? layout.row_length : static_cast<uint64_t>(width);",
    "    const uint64_t rows = layout.image_height != 0 ? layout.image_height : static_cast<uint64_t>(height);",
    "    uint64_t stride = 0, image = 0, images = 0, lines = 0, last = 0;",
    "    if (__builtin_mul_overflow(columns, pixel, &stride) ||",
    "        __builtin_add_overflow(stride, alignment - 1, &stride)) return false;",
    "    stride -= stride % alignment;",
    "    return !__builtin_mul_overflow(rows, stride, &image) &&",
    "           !__builtin_mul_overflow(layout.skip_images + static_cast<uint64_t>(depth - 1), image, &images) &&",
    "           !__builtin_mul_overflow(layout.skip_rows + static_cast<uint64_t>(height - 1), stride, &lines) &&",
    "           !__builtin_mul_overflow(layout.skip_pixels + static_cast<uint64_t>(width), pixel, &last) &&",
    "           !__builtin_add_overflow(images, lines, &bytes) &&",
    "           !__builtin_add_overflow(bytes, last, &bytes);",
    "}",
    "",
    "static bool pack_bytes(const RealGl& av_gl, int64_t width, int64_t height, int64_t depth,",
    "                       GLenum format, GLenum type, uint64_t& bytes) noexcept {",
    "    return store_bytes(av_gl, kPackStore, width, height, depth, format, type, bytes);",
    "}",
    "",
    "// A bitmap - the stipple, glBitmap's - at a bit a pixel, eight to a byte,",
    "// rows padded to the alignment, after the skips.",
    "static bool bitmap_bytes(const RealGl& av_gl, const GLenum* store, int64_t width, int64_t height,",
    "                         uint64_t& bytes) noexcept {",
    "    bytes = 0;",
    "    if (width < 0 || height < 0) return false;",
    "    if (width == 0 || height == 0) return true;",
    "    StoreLayout layout{};",
    "    if (!store_layout(av_gl, store, layout)) return false;",
    "    const uint64_t columns = layout.row_length != 0 ? layout.row_length : static_cast<uint64_t>(width);",
    "    uint64_t stride = columns / 8 + (columns % 8 != 0 ? 1 : 0);",
    "    stride += layout.alignment - 1;",
    "    stride -= stride % layout.alignment;",
    "    uint64_t lines = 0, last = layout.skip_pixels + static_cast<uint64_t>(width);",
    "    last = last / 8 + (last % 8 != 0 ? 1 : 0);",
    "    return !__builtin_mul_overflow(layout.skip_rows + static_cast<uint64_t>(height - 1), stride, &lines) &&",
    "           !__builtin_add_overflow(lines, last, &bytes);",
    "}",
    "",
    "// Whether a buffer is bound where `binding` asks. An offset with none",
    "// bound is a number GL follows as an address in this process.",
    "static bool buffer_bound(const RealGl& av_gl, GLenum binding) noexcept {",
    "    if (av_gl.glGetIntegerv == nullptr) return false;",
    "    GLint name = 0;",
    "    av_gl.glGetIntegerv(binding, &name);",
    "    return name != 0;",
    "}",
    "",
    "// Whether a compressed upload starts at its pointer. With the compressed",
    "// block parameters set, the unpack skips move it whole blocks in, by a",
    "// size the driver works out from state this server does not size.",
    "static bool compressed_starts_at_pointer(const RealGl& av_gl) noexcept {",
    "    uint64_t skip_pixels = 0, skip_rows = 0, skip_images = 0;",
    "    if (av_gl.glGetIntegerv == nullptr || !pack_state(av_gl, 0x0CF4, skip_pixels) ||",
    "        !pack_state(av_gl, 0x0CF3, skip_rows) || !pack_state(av_gl, 0x806D, skip_images)) return false;",
    "    if (skip_pixels == 0 && skip_rows == 0 && skip_images == 0) return true;",
    "    uint64_t width = 0, height = 0, depth = 0, size = 0;",
    "    return pack_state(av_gl, 0x9127, width) && pack_state(av_gl, 0x9128, height) &&",
    "           pack_state(av_gl, 0x9129, depth) && pack_state(av_gl, 0x912A, size) &&",
    "           width == 0 && height == 0 && depth == 0 && size == 0;",
    "}",
    "",
    "// Every array a draw reads from is in a buffer. An enabled array with no",
    "// buffer is a client array, read at its pointer - an address in this",
    "// process that the guest chose - and a buffer deleted from under an array",
    "// leaves it one too, so this is asked at the draw rather than when the",
    "// pointer was set.",
    "static bool fixed_arrays_in_buffers(const RealGl& av_gl) noexcept {",
    "    static constexpr GLenum kArrays[][2] = {",
    "        {0x8074, 0x8896}, {0x8075, 0x8897}, {0x8076, 0x8898}, {0x8077, 0x8899},",
    "        {0x8079, 0x889B}, {0x8457, 0x889D}, {0x845E, 0x889C}};",
    "    for (const auto& array : kArrays) {",
    "        if (av_gl.glIsEnabled(array[0]) && !buffer_bound(av_gl, array[1])) return false;",
    "    }",
    "    GLint units = 0;",
    "    av_gl.glGetIntegerv(0x8871, &units);",
    "    if (units <= 0) return true;",
    "    if (av_gl.glClientActiveTexture == nullptr) return false;",
    "    GLint active = 0x84C0;",
    "    av_gl.glGetIntegerv(0x84E1, &active);",
    "    bool bound = true;",
    "    for (GLint unit = 0; bound && unit < units && unit < kMaxArrays; ++unit) {",
    "        av_gl.glClientActiveTexture(static_cast<GLenum>(0x84C0 + unit));",
    "        bound = !av_gl.glIsEnabled(0x8078) || buffer_bound(av_gl, 0x889A);",
    "    }",
    "    av_gl.glClientActiveTexture(static_cast<GLenum>(active));",
    "    return bound;",
    "}",
    "",
    "// The fixed-function arrays are the compatibility profile's: a core",
    "// context has none, and asking it of them is an error the application",
    "// would read back from its own glGetError.",
    "static bool arrays_in_buffers(const RealGl& av_gl) noexcept {",
    "    if (av_gl.glGetIntegerv == nullptr || av_gl.glIsEnabled == nullptr ||",
    "        av_gl.glGetVertexAttribiv == nullptr) return false;",
    "    if (av_gl.av_fixed_function && !fixed_arrays_in_buffers(av_gl)) return false;",
    "    GLint attributes = 0;",
    "    av_gl.glGetIntegerv(0x8869, &attributes);",
    "    for (GLint index = 0; index < attributes && index < kMaxArrays; ++index) {",
    "        GLint enabled = 0;",
    "        GLint buffer = 0;",
    "        av_gl.glGetVertexAttribiv(static_cast<GLuint>(index), 0x8622, &enabled);",
    "        if (enabled == 0) continue;",
    "        av_gl.glGetVertexAttribiv(static_cast<GLuint>(index), 0x889F, &buffer);",
    "        if (buffer == 0) return false;",
    "    }",
    "    return true;",
    "}",
    "",
    "// arrays_in_buffers, asked once per vertex array object rather than once",
    "// per draw: it is a query per attribute, dozens a draw, and the answer",
    "// changes only when a command that is not array_neutral runs, which",
    "// forgets every verdict. Keyed by the binding GL reports rather than one",
    "// the server tracks, so a bind GL refused cannot stand in for a check.",
    "static bool arrays_verified(const RealGl& av_gl) noexcept {",
    "    if (av_gl.glGetIntegerv == nullptr) return false;",
    "    GLint bound = 0;",
    "    av_gl.glGetIntegerv(0x85B5, &bound);",
    "    const uint32_t vao = static_cast<uint32_t>(bound);",
    "    for (uint32_t i = 0; i < av_gl.av_verified_count; ++i)",
    "        if (av_gl.av_verified_arrays[i] == vao) return true;",
    "    if (!arrays_in_buffers(av_gl)) return false;",
    "    if (av_gl.av_verified_count < RealGl::av_verified_capacity)",
    "        av_gl.av_verified_arrays[av_gl.av_verified_count++] = vao;",
    "    return true;",
    "}",
    "",
    "// How many values an evaluator map's points hold per control point, by",
    "// target, or 0 for a target that is not a map.",
    "static uint64_t map_dimensions(GLenum target) noexcept {",
    "    switch (target) {",
    "        case 0x0D91: case 0x0DB1: case 0x0D93: case 0x0DB3: return 1;  // INDEX, TEXTURE_COORD_1",
    "        case 0x0D94: case 0x0DB4: return 2;  // TEXTURE_COORD_2",
    "        case 0x0D92: case 0x0DB2: case 0x0D95: case 0x0DB5: case 0x0D97: case 0x0DB7: return 3;",
    "        case 0x0D90: case 0x0DB0: case 0x0D96: case 0x0DB6: case 0x0D98: case 0x0DB8: return 4;",
    "        default: return 0;",
    "    }",
    "}",
    "",
    "// The values a one- or two-dimensional map reads: the last control point",
    "// starts a stride per order past the first. A stride below the point's",
    "// own size is one GL refuses, and is counted as that size.",
    "static bool map_points(GLenum target, int64_t uorder, int64_t ustride, int64_t vorder,",
    "                       int64_t vstride, uint64_t& values) noexcept {",
    "    const uint64_t dimensions = map_dimensions(target);",
    "    if (dimensions == 0 || uorder < 1 || vorder < 1 || ustride < 0 || vstride < 0) return false;",
    "    const uint64_t ustep = static_cast<uint64_t>(ustride) > dimensions ? static_cast<uint64_t>(ustride) : dimensions;",
    "    const uint64_t vstep = static_cast<uint64_t>(vstride) > dimensions ? static_cast<uint64_t>(vstride) : dimensions;",
    "    uint64_t across = 0, down = 0;",
    "    return !__builtin_mul_overflow(static_cast<uint64_t>(uorder - 1), ustep, &across) &&",
    "           !__builtin_mul_overflow(static_cast<uint64_t>(vorder - 1), vstep, &down) &&",
    "           !__builtin_add_overflow(across, down, &values) &&",
    "           !__builtin_add_overflow(values, dimensions, &values);",
    "}",
    "",
    "// What glGetMap answers with: the coefficients are every control point,",
    "// which the map's order says how many of; the rest fit in the slack.",
    "static bool map_values(const RealGl& av_gl, GLenum target, GLenum query, uint64_t& values) noexcept {",
    "    values = kQuerySlack;",
    "    const uint64_t dimensions = map_dimensions(target);",
    "    if (query != 0x0A00 || dimensions == 0) return true;",
    "    if (av_gl.glGetMapiv == nullptr) return false;",
    "    GLint order[2] = {0, 0};",
    "    av_gl.glGetMapiv(target, 0x0A01, order);",
    "    const uint64_t u = order[0] > 0 ? static_cast<uint64_t>(order[0]) : 0;",
    "    const uint64_t v = target >= 0x0DB0 ? (order[1] > 0 ? static_cast<uint64_t>(order[1]) : 0) : 1;",
    "    uint64_t points = 0;",
    "    if (__builtin_mul_overflow(u, v, &points) ||",
    "        __builtin_mul_overflow(points, dimensions, &points)) return false;",
    "    if (points > values) values = points;",
    "    return true;",
    "}",
    "",
    "// A pixel map's size is state: GL_PIXEL_MAP_x_TO_y_SIZE sits 0x40 above",
    "// the map it counts.",
    "static bool pixel_map_values(const RealGl& av_gl, GLenum map, uint64_t& values) noexcept {",
    "    values = kQuerySlack;",
    "    if (map < 0x0C70 || map > 0x0C79) return true;",
    "    uint64_t size = 0;",
    "    if (av_gl.glGetIntegerv == nullptr || !pack_state(av_gl, map + 0x40, size)) return false;",
    "    if (size > values) values = size;",
    "    return true;",
    "}",
    "",
]


def _pixels_out_checks(cmd: Command, p: Param) -> list[str]:
    """Between decoding a read-back and calling it: a bound buffer behind an
    offset, and room for what GL will write behind a pointer."""
    names = {q.name for q in cmd.params}
    n = p.name
    binding = GL_PIXEL_PACK_BUFFER_BINDING if p.type.base == "void" else GL_QUERY_BUFFER_BINDING
    # A null destination is an offset too: with no buffer bound GL writes
    # at address zero.
    out = [f"    if ({n}_is_offset || {n} == nullptr) {{",
           "        GLint av_bound = 0;",
           "        if (av_gl.glGetIntegerv == nullptr) return false;",
           f"        av_gl.glGetIntegerv({binding}, &av_bound);",
           "        if (av_bound == 0) return false;",
           "    } else {"]
    room = f"static_cast<uint64_t>({n}_bytes)"
    if "bufSize" in names:
        # GL writes no more than the caller says it has, so the saying is
        # what has to be true.
        out.append(f"        if (bufSize < 0 || static_cast<uint64_t>(bufSize) > {room}) return false;")
    elif {"width", "height", "format", "type"} <= names:
        out.append("        uint64_t av_needed = 0;")
        out.append("        if (!pack_bytes(av_gl, width, height, 1, format, type, av_needed) ||")
        out.append(f"            av_needed > {room}) return false;")
    elif "pname" in names and p.type.base != "void":
        out.append(f"        if (::av::glproto::pname_count(pname) * sizeof({p.type.base}) > {room}) return false;")
    elif {"target", "level", "format", "type"} <= names and not names & {"texture", "texunit"}:
        # The extent is the level's, which only the driver knows.
        out.append("        GLint av_width = 0, av_height = 0, av_depth = 0;")
        out.append("        uint64_t av_needed = 0;")
        out.append("        if (av_gl.glGetTexLevelParameteriv == nullptr) return false;")
        out.append(f"        av_gl.glGetTexLevelParameteriv(target, level, {GL_TEXTURE_WIDTH}, &av_width);")
        out.append(f"        av_gl.glGetTexLevelParameteriv(target, level, {GL_TEXTURE_HEIGHT}, &av_height);")
        out.append(f"        av_gl.glGetTexLevelParameteriv(target, level, {GL_TEXTURE_DEPTH}, &av_depth);")
        out.append("        if (!pack_bytes(av_gl, av_width, av_height, av_depth, format, type, av_needed) ||")
        out.append(f"            av_needed > {room}) return false;")
    else:
        # A compressed level, or a texture named rather than bound: sized
        # by state this server does not ask for yet, so read only into a
        # bound pack buffer, which GL bounds itself.
        out.append("        return false;")
    out.append("    }")
    return out


# ---------------------------------------------------------------------
# What every other pointer the driver is handed may reach
#
# The same rule as the reads above, for the rest of the pointers: the
# driver reads or writes as far as its own arguments and state say, so
# the server works that out again and holds what crossed to it - an
# input no shorter, an output never handed less room, no pointer null
# where GL would follow it, and no offset followed as an address.
# ---------------------------------------------------------------------

GL_ARRAY_BUFFER_BINDING = "0x8894"
GL_ELEMENT_ARRAY_BUFFER_BINDING = "0x8895"
GL_PIXEL_UNPACK_BUFFER_BINDING = "0x88EF"
GL_DRAW_INDIRECT_BUFFER_BINDING = "0x8F43"
GL_PARAMETER_BUFFER_BINDING = "0x80EF"

SIGNED_SCALARS = {"GLint", "GLsizei", "GLsizeiptr", "GLintptr", "GLshort", "GLbyte",
                  "GLint64", "GLint64EXT", "GLsizeiptrARB", "GLintptrARB"}

# Refused whatever their arguments, and why. The driver keeps the pointer
# past the call, and what it points at is a block the next command reuses.
REFUSED_COMMANDS = {
    "glFeedbackBuffer": "feedback mode writes into the buffer at every later draw",
    "glSelectBuffer": "selection mode writes its hits into the buffer at every later draw",
}

# The draws that read every enabled vertex array at its pointer.
# What array_neutral lets past without forgetting arrays_verified's
# verdicts: draws, and state no vertex array holds. glEnable and glDisable
# are not here - a compatibility context takes GL_VERTEX_ARRAY through them.
ARRAY_NEUTRAL_PREFIXES = ("glUniform", "glProgramUniform", "glClearBuffer", "glClearNamedFramebuffer")
ARRAY_NEUTRAL = frozenset((
    "glBindVertexArray", "glUseProgram", "glBindBuffer", "glBindBufferBase", "glBindBufferRange",
    "glBufferData", "glBufferSubData", "glNamedBufferData", "glNamedBufferSubData",
    "glBindTexture", "glBindTextures", "glBindTextureUnit", "glActiveTexture", "glBindSampler", "glBindSamplers",
    "glBindFramebuffer", "glClear", "glClearColor", "glClearDepth", "glClearDepthf", "glClearStencil",
    "glViewport", "glViewportIndexedf", "glViewportIndexedfv", "glScissor", "glScissorIndexed",
    "glDepthMask", "glDepthFunc", "glDepthRange", "glDepthRangef", "glColorMask", "glColorMaski",
    "glStencilMask", "glStencilMaskSeparate", "glStencilFunc", "glStencilFuncSeparate", "glStencilOp",
    "glStencilOpSeparate", "glBlendFunc", "glBlendFuncSeparate", "glBlendEquation", "glBlendEquationSeparate",
    "glBlendColor", "glCullFace", "glFrontFace", "glPolygonMode", "glPolygonOffset", "glLineWidth",
    "glPointSize", "glFlush", "glMemoryBarrier", "glDispatchCompute", "glBindImageTexture",
    "glTexSubImage2D", "glTexSubImage3D", "glTextureSubImage2D", "glTextureSubImage3D", "glGenerateMipmap",
    "glGenerateTextureMipmap", "glBeginQuery", "glEndQuery", "glQueryCounter", "glPixelStorei",
    "glDrawBuffer", "glDrawBuffers", "glSampleMaski", "glPatchParameteri", "glPrimitiveRestartIndex",
    "glProvokingVertex", "glMinSampleShading", "glBeginConditionalRender", "glEndConditionalRender",
    "glUniformBlockBinding", "glShaderStorageBlockBinding",
))


def _array_neutral(name):
    return name in ARRAY_NEUTRAL or name.startswith(ARRAY_NEUTRAL_PREFIXES) or name.startswith(DRAWS_FROM_ARRAYS)


DRAWS_FROM_ARRAYS = ("glDrawArrays", "glDrawElements", "glDrawRangeElements", "glMultiDrawArrays",
                     "glMultiDrawElements", "glArrayElement", "glDrawTransformFeedback")

# The fixed-function arrays arrays_in_buffers asks about. An array it does
# not know of - a vendor's weights or palette indices, a program's NV
# attributes - is one a draw would read without that check, so it may not
# be enabled at all.
KNOWN_ARRAYS = "array == 0x8074 || array == 0x8075 || array == 0x8076 || array == 0x8077 || " \
               "array == 0x8078 || array == 0x8079 || array == 0x8457 || array == 0x845E"
ENABLED_ARRAYS = {
    "glEnableClientState": KNOWN_ARRAYS,
    "glEnableClientStateIndexedEXT": "array == 0x8078",
    "glEnableClientStateiEXT": "array == 0x8078",
    # Direct state access also names a unit's texture coordinates by unit.
    "glEnableVertexArrayEXT": KNOWN_ARRAYS + " || (array >= 0x84C0 && array < 0x84C0 + kMaxArrays)",
}

# Which buffer a void* offset is an offset into, by the parameter's name.
OFFSET_BINDINGS = {
    "indices": (GL_ELEMENT_ARRAY_BUFFER_BINDING,),
    "indirect": (GL_DRAW_INDIRECT_BUFFER_BINDING,),
    "pointer": (GL_ARRAY_BUFFER_BINDING,),
}

# An input GL permits to be null, with the condition under which it does.
NULLABLE_INPUTS = {
    "glBufferData.data": "true",
    "glBufferStorage.data": "true",
    "glNamedBufferData.data": "true",
    "glNamedBufferStorage.data": "true",
    "glNamedBufferDataEXT.data": "true",
    "glNamedBufferStorageEXT.data": "true",
    "glBindBuffersBase.buffers": "true",
    "glBindBuffersRange.buffers": "true",
    "glBindBuffersRange.offsets": "buffers == nullptr",
    "glBindBuffersRange.sizes": "buffers == nullptr",
    "glBindTextures.textures": "true",
    "glBindSamplers.samplers": "true",
    "glBindImageTextures.textures": "true",
    "glBindVertexBuffers.buffers": "true",
    "glBindVertexBuffers.offsets": "buffers == nullptr",
    "glBindVertexBuffers.strides": "buffers == nullptr",
    "glVertexArrayVertexBuffers.buffers": "true",
    "glVertexArrayVertexBuffers.offsets": "buffers == nullptr",
    "glVertexArrayVertexBuffers.strides": "buffers == nullptr",
    "glShaderSource.length": "true",
    "glObjectLabel.label": "true",
    "glObjectPtrLabel.label": "true",
}

# Inputs a bound pixel unpack buffer turns into offsets, where null is the
# start of that buffer rather than nothing.
UNPACKED_INPUTS = ("glBitmap.bitmap", "glPolygonStipple.mask", "glPixelMapfv.values",
                   "glPixelMapuiv.values", "glPixelMapusv.values")

# Outputs GL permits to be null, which stay null: a length a string query
# reports through, and the debug log's columns, whose being null changes
# what the call does.
NULLABLE_OUTPUTS = {
    "glGetAttachedShaders.count", "glGetAttachedObjectsARB.count",
    "glGetDebugMessageLog.sources", "glGetDebugMessageLog.types", "glGetDebugMessageLog.ids",
    "glGetDebugMessageLog.severities", "glGetDebugMessageLog.lengths",
    "glGetDebugMessageLog.messageLog", "glGetObjectLabel.label", "glGetObjectPtrLabel.label",
}

# An output the driver writes more of than the length the guest sizes it
# by: its own count of values, which is the one the room must hold.
SERVER_BOUNDS = {
    # Every property's values, up to `count`, where the guest counts one
    # a property.
    "glGetProgramResourceiv.params": "count",
    # One value for every sync pname there is; the slack, in case of one
    # more.
    "glGetSynciv.values": "kQuerySlack",
}

# The state queries whose answer is a list as long as another query says.
LIST_QUERIES = {
    **{name: (("0x86A3", "av_gl.glGetIntegerv(0x86A2, &av_listed)"),
              ("0x87FF", "av_gl.glGetIntegerv(0x87FE, &av_listed)"),
              ("0x8DF8", "av_gl.glGetIntegerv(0x8DF9, &av_listed)"))
       for name in ("glGetBooleanv", "glGetIntegerv", "glGetInteger64v", "glGetFloatv",
                    "glGetDoublev")},
    "glGetActiveSubroutineUniformiv": (
        ("0x8E4B", "av_gl.glGetActiveSubroutineUniformiv(program, shadertype, index, 0x8E4A, &av_listed)"),),
    "glGetActiveAtomicCounterBufferiv": (
        ("0x92C6", "av_gl.glGetActiveAtomicCounterBufferiv(program, bufferIndex, 0x92C5, &av_listed)"),),
}


def _scalar(cmd: Command, name: str) -> Param:
    for q in cmd.params:
        if q.name == name and q.role is Role.SCALAR:
            return q
    raise ValueError(f"{cmd.name}: {name} sizes a pointer but is not a scalar argument")


def _not_negative(cmd: Command, names: list[str]) -> list[str]:
    """A count GL refuses when negative, refused here before it is one."""
    return [f"    if ({n} < 0) return false;" for n in names
            if _scalar(cmd, n).type.base in SIGNED_SCALARS]


def _listed_values(cmd: Command, p: Param) -> list[str]:
    n = p.name
    lines = [f"    uint64_t {n}_elements = kQuerySlack;"]
    pname = p.length.reads[0]
    for listed, query in LIST_QUERIES.get(cmd.name, ()):
        function = query.split("(")[0]
        lines += [f"    if ({pname} == {listed}) {{",
                  f"        if ({function} == nullptr) return false;",
                  "        GLint av_listed = 0;",
                  f"        {query};",
                  f"        if (av_listed > 0 && static_cast<uint64_t>(av_listed) > {n}_elements)",
                  f"            {n}_elements = static_cast<uint64_t>(av_listed);",
                  "    }"]
    return lines


def _helper_elements(cmd: Command, p: Param, text: str) -> list[str]:
    """A length only the driver's state can say, asked of the driver."""
    n = p.name
    if "pname_count(av_t," in text or "uniform_value_count(" in text:
        return [f"    const uint64_t {n}_elements = kQuerySlack;"]
    if "uniform_block_value_count(" in text:
        return [f"    uint64_t {n}_elements = kQuerySlack;",
                "    if (pname == 0x8A43) {",
                "        if (av_gl.glGetActiveUniformBlockiv == nullptr) return false;",
                "        GLint av_listed = 0;",
                "        av_gl.glGetActiveUniformBlockiv(program, uniformBlockIndex, 0x8A42, &av_listed);",
                f"        if (av_listed > 0 && static_cast<uint64_t>(av_listed) > {n}_elements)",
                f"            {n}_elements = static_cast<uint64_t>(av_listed);",
                "    }"]
    if "map_value_count(" in text:
        return [f"    uint64_t {n}_elements = 0;",
                f"    if (!map_values(av_gl, target, query, {n}_elements)) return false;"]
    if "pixel_map_size(" in text:
        return [f"    uint64_t {n}_elements = 0;",
                f"    if (!pixel_map_values(av_gl, map, {n}_elements)) return false;"]
    if "stipple_bytes(" in text:
        store = "kPackStore" if "true" in text else "kUnpackStore"
        return [f"    uint64_t {n}_elements = 0;",
                f"    if (!bitmap_bytes(av_gl, {store}, 32, 32, {n}_elements)) return false;"]
    if "bitmap_bytes(" in text:
        return [f"    uint64_t {n}_elements = 0;",
                f"    if (!bitmap_bytes(av_gl, kUnpackStore, width, height, {n}_elements)) return false;"]
    if "named_program_string_bytes(" in text:
        return ["    if (av_gl.glGetNamedProgramivEXT == nullptr) return false;",
                "    GLint av_program_length = 0;",
                "    av_gl.glGetNamedProgramivEXT(program, target, 0x8627, &av_program_length);",
                f"    const uint64_t {n}_elements = av_program_length > 0 ? static_cast<uint64_t>(av_program_length) : 0;"]
    if "map_point_values(" in text and cmd.name.startswith("glMap1"):
        return [f"    uint64_t {n}_elements = 0;",
                f"    if (!map_points(target, order, stride, 1, 0, {n}_elements)) return false;"]
    if "map_point_values(" in text and cmd.name.startswith("glMap2"):
        return [f"    uint64_t {n}_elements = 0;",
                f"    if (!map_points(target, uorder, ustride, vorder, vstride, {n}_elements)) return false;"]
    raise ValueError(f"{cmd.name}.{n}: no server-side length for {text}")


def _server_elements(cmd: Command, p: Param) -> list[str]:
    """Lines defining `<name>_elements`: how many elements the driver reads
    or writes through the parameter, worked out from its arguments."""
    n = p.name
    length = p.length
    bound = SERVER_BOUNDS.get(f"{cmd.name}.{n}")
    if bound == "kQuerySlack":
        return [f"    const uint64_t {n}_elements = kQuerySlack;"]
    if bound is not None:
        return _not_negative(cmd, [bound]) + [f"    const uint64_t {n}_elements = static_cast<uint64_t>({bound});"]
    if length is None:
        raise ValueError(f"{cmd.name}.{n}: no length")
    if length.kind is LengthKind.FIXED:
        return [f"    const uint64_t {n}_elements = {length.count};"]
    if length.kind is LengthKind.PARAM:
        return (_not_negative(cmd, [length.text]) +
                [f"    const uint64_t {n}_elements = static_cast<uint64_t>({length.text});"])
    if length.kind is LengthKind.PNAME:
        if p.role is Role.ARRAY_OUT:
            return _listed_values(cmd, p)
        return [f"    const uint64_t {n}_elements = ::av::glproto::pname_count({length.reads[0]});"]
    if length.kind is LengthKind.TYPED_COUNT:
        count, type_name = length.reads
        return (_not_negative(cmd, [count]) +
                [f"    const uint64_t {n}_elements = static_cast<uint64_t>({count}) * "
                 f"::av::glproto::type_size({type_name});"])
    if length.kind is LengthKind.EXPR:
        text = length.text
        if "av_t" in text or "::av::glproto::" in text:
            return _helper_elements(cmd, p, text)
        # C over the call's own arguments, evaluated in 64 bits once none
        # of them is negative.
        scalars = [q.name for q in cmd.params if q.role is Role.SCALAR]
        named = [s for s in scalars if re.search(rf"\b{re.escape(s)}\b", text)]
        for s in named:
            text = re.sub(rf"\b{re.escape(s)}\b", f"static_cast<int64_t>({s})", text)
        return (_not_negative(cmd, named) +
                [f"    const uint64_t {n}_elements = static_cast<uint64_t>({text});"])
    raise ValueError(f"{cmd.name}.{n}: no server-side length for {length.kind}")


def _element_bytes(p: Param) -> str:
    if p.length is not None and p.length.kind is LengthKind.TYPED_COUNT:
        return "1"
    return f"sizeof({_storage_type(p)})"


def _array_in_checks(cmd: Command, p: Param) -> list[str]:
    n = p.name
    key = f"{cmd.name}.{n}"
    nullable = NULLABLE_INPUTS.get(key, "false")
    if key in UNPACKED_INPUTS or "Compressed" in cmd.name:
        nullable = f"buffer_bound(av_gl, {GL_PIXEL_UNPACK_BUFFER_BINDING})"
        if "Compressed" in cmd.name and "Sub" not in cmd.name:
            nullable = "true"
    out = _server_elements(cmd, p)
    out += [f"    uint64_t {n}_needed = 0;",
            f"    if (__builtin_mul_overflow({n}_elements, static_cast<uint64_t>({_element_bytes(p)}), &{n}_needed)) return false;",
            f"    if ({n} == nullptr ? ({n}_needed != 0 && !({nullable})) : {n}_room < {n}_needed) return false;"]
    if "Compressed" in cmd.name:
        out.append("    if (!compressed_starts_at_pointer(av_gl)) return false;")
    return out


def _pixels_in_checks(cmd: Command, p: Param) -> list[str]:
    n = p.name
    names = {q.name for q in cmd.params}
    unpack = f"buffer_bound(av_gl, {GL_PIXEL_UNPACK_BUFFER_BINDING})"
    clears = cmd.name.startswith("glClear")
    specifies = re.match(r"^gl(Compressed)?(Tex|Texture|MultiTex)Image[123]D(EXT)?$", cmd.name)
    null_ok = "true" if clears or specifies else unpack
    out = [f"    if ({n}_is_offset) {{",
           f"        if ({'true' if clears else '!' + unpack}) return false;",
           f"    }} else if ({n} == nullptr) {{",
           f"        if (!({null_ok})) return false;",
           "    } else {",
           f"        uint64_t {n}_needed = 0;"]
    if clears:
        # One element, of the format and type given: the pixel store
        # does not apply to a clear.
        out.append(f"        if (({n}_needed = pixel_bytes(format, type)) == 0 || {n}_room < {n}_needed) return false;")
    elif p.length is not None and p.length.kind is LengthKind.PARAM:
        size = p.length.text
        out.append(f"        if ({size} < 0 || !compressed_starts_at_pointer(av_gl)) return false;")
        out.append(f"        {n}_needed = static_cast<uint64_t>({size});")
        out.append(f"        if ({n}_room < {n}_needed) return false;")
    elif {"format", "type", "width"} <= names:
        height = "height" if "height" in names else "1"
        depth = "depth" if "depth" in names else "1"
        out.append(f"        if (!store_bytes(av_gl, kUnpackStore, width, {height}, {depth}, format, type, {n}_needed) ||")
        out.append(f"            {n}_room < {n}_needed) return false;")
    else:
        raise ValueError(f"{cmd.name}.{n}: pixels in with no size the server can check")
    out.append("    }")
    return out


def _refusals(cmd: Command) -> list[str]:
    """Between decoding a command and making room for its answers: every
    pointer it carries held to what the driver will do with it."""
    out: list[str] = []
    names = {q.name for q in cmd.params}
    late: list[str] = []
    for p in cmd.params:
        n = p.name
        if p.role is Role.ARRAY_IN:
            out += _array_in_checks(cmd, p)
        elif p.role is Role.PIXELS_IN:
            out += _pixels_in_checks(cmd, p)
        elif p.role is Role.STRING_IN:
            if NULLABLE_INPUTS.get(f"{cmd.name}.{n}") is None:
                out.append(f"    if ({n} == nullptr) return false;")
            if any(q.name == "length" and q.role is Role.SCALAR and q.type.base in SIGNED_SCALARS
                   for q in cmd.params):
                # A length that is not negative is how far the driver
                # reads, terminator or not.
                out.append(f"    if ({n} != nullptr && length >= 0 && static_cast<uint64_t>(length) > {n}_length) return false;")
            else:
                out.append(f"    (void){n}_length;")
        elif p.role is Role.STRING_ARRAY_IN:
            count = p.length.text
            out += _not_negative(cmd, [count])
            out.append(f"    if ({n} == nullptr ? {count} != 0 : {n}_count < static_cast<uint64_t>({count})) return false;")
            if p.has_lengths:
                late += ["    if (length != nullptr) {",
                         f"        for (uint64_t av_i = 0; av_i < static_cast<uint64_t>({count}); ++av_i) {{",
                         f"            if (length[av_i] >= 0 && static_cast<uint64_t>(length[av_i]) > {n}_lengths[av_i]) return false;",
                         "        }",
                         "    }"]
        elif p.role is Role.OFFSET_ARRAY:
            count = p.length.text
            out += _not_negative(cmd, [count])
            late += [f"    if ({n} == nullptr ? {count} != 0 : {n}_count < static_cast<uint64_t>({count})) return false;",
                     f"    if ({count} != 0 && !buffer_bound(av_gl, {GL_ELEMENT_ARRAY_BUFFER_BINDING})) return false;"]
        elif p.role is Role.BUFFER_OFFSET:
            if n not in OFFSET_BINDINGS:
                raise ValueError(f"{cmd.name}.{n}: an offset into no buffer this server knows")
            # A null array pointer with no buffer is an array with nowhere to
            # read from, which GL takes, and which arrays_in_buffers refuses
            # to draw from; a null index or indirect pointer is read at once.
            unbound = "pointer != nullptr && " if n == "pointer" else ""
            for binding in OFFSET_BINDINGS[n]:
                out.append(f"    if ({unbound}!buffer_bound(av_gl, {binding})) return false;")
            if cmd.name.endswith("IndirectCount"):
                out.append(f"    if (!buffer_bound(av_gl, {GL_PARAMETER_BUFFER_BINDING})) return false;")
    if cmd.name.startswith(DRAWS_FROM_ARRAYS):
        late.append("    if (!arrays_verified(av_gl)) return false;")
    if cmd.name in ENABLED_ARRAYS:
        _scalar(cmd, "array")
        late.append(f"    if (!({ENABLED_ARRAYS[cmd.name]})) return false;")
    return out + late


def _output_room(cmd: Command, p: Param) -> list[str]:
    """The room an output is written into: as much as the guest asked for,
    and never less than the driver will write. A guest that asked for none
    of an output GL will not take null for has it written into the block
    and not sent back."""
    n = p.name
    storage = _storage_type(p)
    nullable = f"{cmd.name}.{n}" in NULLABLE_OUTPUTS or (
        n == "length" and p.role is Role.ARRAY_OUT and p.length is not None
        and p.length.kind is LengthKind.FIXED)
    out = _server_elements(cmd, p)
    guard = f"{n}_answered" if nullable else "true"
    out += [f"    if ({guard}) {{",
            f"        uint64_t av_room = static_cast<uint64_t>({n}_bytes / sizeof({storage}));",
            f"        if (av_room < {n}_elements) av_room = {n}_elements;",
            f"        if (av_room >= SIZE_MAX / sizeof({storage})) return false;",
            f"        {n}_storage_count = static_cast<size_t>(av_room) + 1;",
            f"        {n}_storage = av_arena.allocate<{storage}>({n}_storage_count);",
            f"        if ({n}_storage == nullptr) return false;",
            # Cleared, because the driver may answer with fewer values than
            # were asked for and the reply sends back as many as were: the
            # difference would be whatever the block last held.
            f"        for (size_t av_z = 0; av_z < {n}_storage_count; ++av_z) {{ {n}_storage[av_z] = {{}}; }}"]
    if storage == p.type.base:
        out.append(f"        {n} = {n}_storage;")
    else:
        out.append(f"        {n} = reinterpret_cast<{p.type.base}*>({n}_storage);")
    out.append("    }")
    return out


def emit_server(reg: Registry, out_dir: Path) -> None:
    h = [HEADER, "#pragma once\n",
         "#include <stddef.h>", "#include <stdint.h>\n",
         '#include "proto/wire/wire_arena.hpp"',
         '#include "proto/wire/wire_writer.hpp"',
         '#include "proto/wire/wire_reader.hpp"',
         '#include "gl_real_dispatch.h"\n',
         "namespace av::glproto {\n",
         "// Reads one command and calls the host's entry point for it.",
         "//",
         "// `arena` is where everything a decoded command points at lives,",
         "// for the length of the call; it is the caller's block, because",
         "// it has to outlive the decode that filled it.",
         "[[nodiscard]] bool dispatch_command(const RealGl& gl, uint32_t cmd_id,",
         "                                    ::av::WireReader& reader,",
         "                                    ::av::WireWriter& reply,",
         "                                    ::av::WireArena& arena) noexcept;\n",
         "}  // namespace av::glproto", ""]
    c = [HEADER, '#include "gl_dispatch_server.h"\n',
         "#include <stddef.h>\n",
         '#include "core/trace/function_scope.hpp"',
         '#include "gl_protocol_ids.h"',
         '#include "gl_sizes.h"',
         '',
         "namespace av::glproto {\n",
         "// How many values the largest state query GL has answers",
         "// with: a four-by-four matrix, and room to spare. A query's",
         "// buffer is never smaller than this, whatever the guest asked",
         "// for, because the host writes as many values as the enum",
         "// really has and a short buffer is a heap the server owns.",
         "static constexpr size_t kQuerySlack = 32;\n",
         "// More texture units or vertex attributes than any driver has: a",
         "// bound on the loops that ask about each, whatever a driver answers.",
         "static constexpr GLint kMaxArrays = 64;\n"] + PACK_HELPERS
    for cmd in reg.commands.values():
        c.append(f"static bool decode_{cmd.name}(const RealGl& av_gl, "
                 "::av::WireReader& av_r, "
                 "::av::WireWriter& av_reply, "
                 "::av::WireArena& av_arena) {")
        # One trace scope per decoded command (docs/trace.md).
        c.append(f'    AV_SCOPE("server:{cmd.name}");')
        c.append("    (void)av_arena; (void)av_reply; (void)av_r;")
        if cmd.name in REFUSED_COMMANDS:
            c.append(f"    (void)av_gl;  // Refused: {REFUSED_COMMANDS[cmd.name]}.")
            c.append("    return false;")
            c.append("}")
            c.append("")
            continue
        if cmd.mints_handle:
            c.append("    uint64_t av_token = 0;")
            c.append("    if (!av_r.read_pod(av_token)) return false;")
        for p in cmd.params:
            _decode_param(p, c)
        c.extend(_refusals(cmd))
        for p in cmd.params:
            if p.role in (Role.ARRAY_OUT, Role.STRING_OUT):
                c.extend(_output_room(cmd, p))
        for p in cmd.params:
            if p.role is Role.PIXELS_OUT:
                c.extend(_pixels_out_checks(cmd, p))
        # A pixel parameter is decoded as void*, which is what every
        # pixel transfer's own signature says. A query result is not:
        # glGetQueryObjectuiv wants GLuint*, and it is carried the same
        # way because the question — is this an address or an offset
        # into a bound buffer — is the same one.
        call_args = ", ".join(
            (f"static_cast<{p.type.cpp()}>({p.name})"
             if p.role is Role.PIXELS_OUT and p.type.base != "void"
             else p.name)
            for p in cmd.params)
        c.append(f"    if (av_gl.{cmd.name} == nullptr) return false;")
        kind = _ret_kind(cmd)
        if kind == "void":
            c.append(f"    av_gl.{cmd.name}({call_args});")
        else:
            c.append(f"    const auto av_ret = av_gl.{cmd.name}({call_args});")
        if cmd.mints_handle:
            # The guest already answered with the token; all that is
            # owed here is the note of which fence it meant.
            c.append("    if (av_gl.av_bind_sync != nullptr) { av_gl.av_bind_sync(av_gl.av_sync_user, av_token, av_ret); }")
        if kind == "scalar":
            c.append("    av_reply.write_varint(static_cast<uint64_t>(av_ret));")
        elif kind == "sync" and not cmd.mints_handle:
            c.append("    av_reply.write_pod<uint64_t>(")
            c.append("        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(av_ret)));")
        elif kind == "string":
            c.append("    const auto* av_text = reinterpret_cast<const char*>(av_ret);")
            c.append("    av_reply.write_varint(av_text != nullptr ? 1 : 0);")
            c.append("    const size_t av_length = av_text != nullptr ? __builtin_strlen(av_text) : 0;")
            c.append("    av_reply.write_varint(static_cast<uint64_t>(av_length));")
            c.append("    if (av_length != 0) av_reply.write(av_text, av_length);")
        elif kind == "pointer":
            c.append("    av_reply.write_pod<uint64_t>(")
            c.append("        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(av_ret)));")
        for p in cmd.params:
            if p.role in (Role.ARRAY_OUT, Role.STRING_OUT, Role.PIXELS_OUT):
                guard = (f"{p.name} != nullptr && !{p.name}_is_offset"
                         f" && !{p.name}_via_blob"
                         if p.role is Role.PIXELS_OUT else f"{p.name}_answered")
                c.append(f"    if ({guard}) {{")
                c.append(f"        av_reply.write_varint(static_cast<uint64_t>({p.name}_bytes));")
                c.append(f"        if ({p.name}_bytes != 0) av_reply.write({p.name}, {p.name}_bytes);")
                c.append("    }")
        if cmd.name == "glDeleteSync":
            # The host's object is gone; without this the table keeps a
            # dead entry per fence, and a game makes one a frame.
            c.append("    if (av_gl.av_forget_sync != nullptr) { av_gl.av_forget_sync(av_gl.av_sync_user, sync_token); }")
        c.append("    return true;")
        c.append("}")
        c.append("")
    neutral = [name for name in reg.commands if _array_neutral(name)]
    c.append("// Commands that cannot change what any vertex array reads from, so a")
    c.append("// verdict of arrays_verified outlives them. Anything else - a pointer,")
    c.append("// an enable, a deleted buffer or array, a client state - forgets them.")
    c.append("static bool array_neutral(uint32_t cmd_id) noexcept {")
    c.append("    switch (cmd_id) {")
    for name in neutral:
        c.append(f"        case kCmd_{name}:")
    c += ["            return true;", "        default: return false;", "    }", "}", ""]
    c.append("bool dispatch_command(const RealGl& av_gl, uint32_t cmd_id,")
    c.append("                      ::av::WireReader& av_r,")
    c.append("                      ::av::WireWriter& av_reply,")
    c.append("                      ::av::WireArena& av_arena) noexcept {")
    c.append("    if (!array_neutral(cmd_id)) av_gl.av_verified_count = 0;")
    c.append("    switch (cmd_id) {")
    for cmd in reg.commands.values():
        c.append(f"        case kCmd_{cmd.name}: "
                  f"return decode_{cmd.name}(av_gl, av_r, av_reply, av_arena);")
    c += ["        default: return false;", "    }", "}", "",
          "}  // namespace av::glproto", ""]
    write(out_dir / "gl_dispatch_server.h", "\n".join(h))
    write(out_dir / "gl_dispatch_server.cpp", "\n".join(c))


# ---------------------------------------------------------------------
# The host's entry points
# ---------------------------------------------------------------------

def emit_real_dispatch(reg: Registry, out_dir: Path) -> None:
    out = [HEADER, "#pragma once\n",
           "#include <stddef.h>", "#include <stdint.h>\n",
           '#include "gl_types.h"\n',
           "namespace av::glproto {\n",
           "// The host's OpenGL, by name.",
           "//",
           "// There is no OpenGL ABI past 1.1 on any platform worth the",
           "// name: everything else is resolved through the window",
           "// system's own getProcAddress. So the server holds a table",
           "// and fills it once per context.",
           "struct RealGl {",
           "    // Turns a region offset from the guest into a pointer,",
           "    // bounds-checked against the mapping. Set by the server;",
           "    // null when there is no region, in which case a command",
           "    // that names one is refused rather than guessed at.",
           "    uint8_t* (*av_blob)(void*, uint64_t, uint64_t)"
           " = nullptr;",
           "    void* av_blob_user = nullptr;",
           "",
           "    // A guest mints a sync token; the host binds it to whatever",
           "    // glFenceSync made and looks it up again by the token. Fields",
           "    // rather than free functions for the reason av_blob is one:",
           "    // a parameterless hook cannot reach one context's state, and",
           "    // this project has no file-scope place to put it (042).",
           "    void (*av_bind_sync)(void*, uint64_t, void*) = nullptr;",
           "    void* (*av_sync_for)(void*, uint64_t) = nullptr;",
           "    void (*av_forget_sync)(void*, uint64_t) = nullptr;",
           "    void* av_sync_user = nullptr;",
           "",
           "    // Whether the context has the compatibility profile's",
           "    // fixed-function vertex arrays, which a draw's check of its",
           "    // arrays asks after. Set by the server from what the host",
           "    // granted; a core context has none to ask of.",
           "    bool av_fixed_function = true;",
           "",
           "    // The vertex array objects a draw found reading only from",
           "    // buffers (arrays_verified). Mutable because dispatch is handed",
           "    // the table const: these are a cache of what the context would",
           "    // answer, not entry points.",
           "    static constexpr uint32_t av_verified_capacity = 8;",
           "    mutable uint32_t av_verified_arrays[av_verified_capacity] = {};",
           "    mutable uint32_t av_verified_count = 0;"]
    for cmd in reg.commands.values():
        params = ", ".join(p.type.cpp() for p in cmd.params) or "void"
        out.append(f"    {cmd.return_type.cpp()} (*{cmd.name})({params}) = nullptr;")
    out += ["};", "",
            "// Fills the table. `load` is the platform's resolver:",
            "// eglGetProcAddress, glXGetProcAddress or wglGetProcAddress.",
            "// Returns how many entry points were found.",
            "using GlLoader = void* (*)(const char* name, void* user);",
            "",
            "inline size_t load_real_gl(RealGl& gl, GlLoader load, void* user) {",
            "    size_t found = 0;"]
    for cmd in reg.commands.values():
        params = ", ".join(p.type.cpp() for p in cmd.params) or "void"
        out.append(f'    gl.{cmd.name} = reinterpret_cast<{cmd.return_type.cpp()} (*)({params})>('
                   f'load("{cmd.name}", user));')
        out.append(f"    if (gl.{cmd.name} != nullptr) ++found;")
    out += ["    return found;", "}", "",
            "}  // namespace av::glproto", ""]
    write(out_dir / "gl_real_dispatch.h", "\n".join(out))


# ---------------------------------------------------------------------
# What this driver may claim
# ---------------------------------------------------------------------

def emit_tier(reg: Registry, out_dir: Path) -> None:
    out = [HEADER, "#pragma once\n",
           "#include <stdint.h>\n",
           "namespace av::glproto {\n",
           "// The version this driver carries, and may therefore claim.",
           "//",
           "// The host's own GL is newer than the tier — it is whatever",
           "// the machine has — and forwarding its version string would",
           "// promise entry points the wire does not carry. So the",
           "// version is capped to this and the extension list is",
           "// filtered to the table below.",
           f"inline constexpr uint32_t kTierMajor = {reg.tier_major};",
           f"inline constexpr uint32_t kTierMinor = {reg.tier_minor};",
           "",
           "// Every extension whose commands are all generated. An",
           "// extension needing an entry point that does not cross is",
           "// not one this driver can offer, whatever the host reports.",
           "inline constexpr const char* kCarriedExtensions[] = {"]
    for name in reg.carried_extensions:
        out.append(f'    "{name}",')
    out += ["};", "",
            "inline constexpr size_t kCarriedExtensionCount =",
            "    sizeof(kCarriedExtensions) / sizeof(kCarriedExtensions[0]);",
            "",
            "[[nodiscard]] inline bool extension_is_carried(const char* name) noexcept {",
            "    if (name == nullptr) return false;",
            "    for (size_t i = 0; i < kCarriedExtensionCount; ++i) {",
            "        if (__builtin_strcmp(name, kCarriedExtensions[i]) == 0) return true;",
            "    }",
            "    return false;",
            "}",
            "",
            "}  // namespace av::glproto", ""]
    write(out_dir / "gl_tier.h", "\n".join(out))


# ---------------------------------------------------------------------
# The guest's own entry points
# ---------------------------------------------------------------------
#
# What an application links against is the platform's OpenGL library,
# and every one of its entry points has to exist. Nearly all of them
# are the same shape — take the arguments, hand them to the generated
# client — so they are written here rather than by hand, and only the
# ones that need something of the guest's own are left out for the
# driver to write itself.
#
# Three shapes need that something:
#
#   pixels    how many bytes a transfer is depends on the format, the
#             type, the extent and the pixel store state, and the
#             guest is the side that knows the store state
#   strings   glGetString answers with a pointer the caller keeps, so
#             the guest has to own the storage
#   mapping   a pointer into a buffer has to be guest memory, filled
#             at map and written back at unmap
#
# Everything else is forwarding.

HANDWRITTEN_PREFIXES = ("glMapBuffer", "glMapNamedBuffer", "glUnmapBuffer",
                        "glUnmapNamedBuffer", "glFlushMappedBufferRange",
                        "glFlushMappedNamedBufferRange")

HANDWRITTEN = {
    "glGetString", "glGetStringi",
    # Answered from the region when the server has already written the
    # answer there. An application polls a fence rather than waiting on
    # it, and a poll that crosses the wire cannot be answered until the
    # server has drained everything queued in front of it.
    "glClientWaitSync",
    # Created out of a blob where the host will allow it, so that a
    # coherent mapping of it needs no write-backs at all. Immutable
    # storage cannot be respecified, so this is the only moment it can
    # be decided. See glBufferStorage in entries_by_hand.cpp.
    "glBufferStorage",
    "glGetBufferPointerv", "glGetNamedBufferPointerv",
    "glGetVertexAttribPointerv",
    "glDebugMessageCallback",
    # Forwarded like the rest, and noted on the way past: the driver
    # needs the pixel store state to work out how long a transfer is,
    # and the buffer bindings to know whether a pointer argument is an
    # address or an offset.
    "glPixelStorei", "glPixelStoref",
    "glBindBuffer", "glDeleteBuffers",
    # A location is fixed when a program is linked and stays fixed
    # until it is linked again, so the answer is the driver's to keep.
    # Applications ask for them per draw — one measured example asked
    # two thousand times a frame — and each one was a round trip.
    "glGetUniformLocation", "glGetAttribLocation",
    "glLinkProgram", "glDeleteProgram",
    # Names are answers too, and an application that builds geometry
    # every frame asks for them constantly. The driver takes them from
    # a block the host reserved.
    "glGenBuffers", "glGenVertexArrays", "glGenTextures",
    # "Start working on what I have given you" — which means the
    # commands gathered here go out, not just the ones already sent.
    "glFlush",
    # The window. Framebuffer zero is a framebuffer object on this host,
    # and the buffers an application names for it - GL_BACK, GL_FRONT -
    # are that object's colour attachment; only the guest knows which
    # window the calling context is current on (back_buffer_presenter).
    # GL_FRONT has no host buffer at all: what it names is the frame the
    # swap already read back and put on the window, which the guest still
    # holds, so a read of it is answered here (feature 051, FrontBuffer).
    "glBindFramebuffer", "glDrawBuffer", "glReadBuffer", "glReadPixels",

    # The array pointers and the draws that read them. A pointer taken
    # with no buffer bound is an address in the guest, and the rows it
    # names have to be copied across before the draw goes — which is
    # something only the guest can do. See client_arrays.cpp.
    "glVertexPointer", "glNormalPointer", "glColorPointer",
    "glSecondaryColorPointer", "glIndexPointer", "glEdgeFlagPointer",
    "glFogCoordPointer", "glTexCoordPointer", "glVertexAttribPointer",
    "glInterleavedArrays",
    "glEnableClientState", "glDisableClientState",
    "glEnableVertexAttribArray", "glDisableVertexAttribArray",
    "glClientActiveTexture",
    # Which vertex array object those arrays belong to. They are the
    # object's state and not the context's, so the driver has to see
    # the binding change and the deletion.
    "glBindVertexArray", "glDeleteVertexArrays",
    "glDrawArrays", "glDrawArraysInstanced", "glDrawElements",
    "glDrawElementsInstanced", "glDrawRangeElements", "glArrayElement",

    # The calls that give a buffer its storage. What they carry is the
    # size, and the size is what glMapBuffer would otherwise ask the
    # host for — a round trip in front of every map.
    "glBufferData", "glBufferStorage", "glNamedBufferData",
    "glNamedBufferStorage",
}


# Binds whose effect can already be in place.
#
# Half of what SuperTuxKart sends a frame is these three, and 42% of
# those ask for a binding the driver has already made. The expression
# answers "this call cannot change anything", and a call that cannot
# change anything is not worth a ring record. See support.hpp.
BIND_SHADOW = {
    "glActiveTexture": "::av::glicd::skip_active_texture(texture)",
    "glBindTexture": "::av::glicd::skip_bind_texture(target, texture)",
    "glBindSampler": "::av::glicd::skip_bind_sampler(unit, sampler)",
}

# Queries some of whose names only the guest can answer.
#
# A buffer's map state - GL_BUFFER_MAPPED, _ACCESS, _ACCESS_FLAGS,
# _MAP_OFFSET and _MAP_LENGTH - describes a map the host never saw: a map is
# a copy in the guest (av::glicd::MappedBuffers), so the host always says
# unmapped. The expression answers those names and says so; every other name
# still goes to the host. A tracer's glUnmapBuffer asks them to learn what
# the application wrote, and recorded nothing while the host answered.
ANSWERED_BY_GUEST_NAMES = {
    "glGetBufferParameteriv": "::av::glicd::answer_buffer_map_state(target, pname, params)",
    "glGetBufferParameteri64v": "::av::glicd::answer_buffer_map_state(target, pname, params)",
    "glGetNamedBufferParameteriv": "::av::glicd::answer_named_buffer_map_state(buffer, pname, params)",
    "glGetNamedBufferParameteri64v": "::av::glicd::answer_named_buffer_map_state(buffer, pname, params)",
    "glGetNamedBufferParameterivEXT": "::av::glicd::answer_named_buffer_map_state(buffer, pname, params)",
}

# Commands that change a buffer's bytes on the host, told to the guest's
# copies of them before they go.
#
# A map is a copy in the guest, and OpenGL keeps every byte a map does not
# write, so a map that only writes starts from the guest's copy of what the
# host holds and sends back only what differs (av::glicd::BufferShadows).
# The copy is only as good as the guest's knowledge: a command the host runs
# that writes a buffer drops that buffer's copy, glBufferSubData updates it,
# and anything that lets a shader write buffers the guest cannot follow ends
# the copies for the context. A download into a pack or query buffer is
# every command with pixels out, which _icd_buffer_writes adds itself.
BUFFER_WRITES = {
    "glBufferSubData": "::av::glicd::wrote_buffer(target, offset, size, data)",
    "glNamedBufferSubData": "::av::glicd::wrote_named_buffer(buffer, offset, size, data)",
    "glNamedBufferSubDataEXT": "::av::glicd::wrote_named_buffer(buffer, offset, size, data)",
    "glCopyBufferSubData": "::av::glicd::host_writes_target(writeTarget)",
    "glCopyNamedBufferSubData": "::av::glicd::host_writes_buffer(writeBuffer)",
    "glNamedCopyBufferSubDataEXT": "::av::glicd::host_writes_buffer(writeBuffer)",
    "glClearBufferData": "::av::glicd::host_writes_target(target)",
    "glClearBufferSubData": "::av::glicd::host_writes_target(target)",
    "glClearNamedBufferData": "::av::glicd::host_writes_buffer(buffer)",
    "glClearNamedBufferSubData": "::av::glicd::host_writes_buffer(buffer)",
    "glClearNamedBufferDataEXT": "::av::glicd::host_writes_buffer(buffer)",
    "glClearNamedBufferSubDataEXT": "::av::glicd::host_writes_buffer(buffer)",
    "glInvalidateBufferData": "::av::glicd::host_writes_buffer(buffer)",
    "glInvalidateBufferSubData": "::av::glicd::host_writes_buffer(buffer)",
    "glNamedBufferDataEXT": "::av::glicd::host_writes_buffer(buffer)",
    "glNamedBufferStorageEXT": "::av::glicd::host_writes_buffer(buffer)",
    "glBufferPageCommitmentARB": "::av::glicd::host_writes_target(target)",
    "glNamedBufferPageCommitmentARB": "::av::glicd::host_writes_buffer(buffer)",
    "glNamedBufferPageCommitmentEXT": "::av::glicd::host_writes_buffer(buffer)",
    "glGetQueryBufferObjecti64v": "::av::glicd::host_writes_buffer(buffer)",
    "glGetQueryBufferObjectiv": "::av::glicd::host_writes_buffer(buffer)",
    "glGetQueryBufferObjectui64v": "::av::glicd::host_writes_buffer(buffer)",
    "glGetQueryBufferObjectuiv": "::av::glicd::host_writes_buffer(buffer)",
    # These bind the target's own binding as well as the index, which the
    # guest follows to know what a target names.
    "glBindBufferBase": "::av::glicd::bind_buffer_indexed(target, buffer)",
    "glBindBufferRange": "::av::glicd::bind_buffer_indexed(target, buffer)",
    "glBindBuffersBase": "::av::glicd::bind_buffers_indexed(target)",
    "glBindBuffersRange": "::av::glicd::bind_buffers_indexed(target)",
    "glTransformFeedbackBufferBase": "::av::glicd::shaders_write_buffers()",
    "glTransformFeedbackBufferRange": "::av::glicd::shaders_write_buffers()",
    "glBeginTransformFeedback": "::av::glicd::shaders_write_buffers()",
    # A transform feedback object brings its own buffer bindings.
    "glBindTransformFeedback": "::av::glicd::bind_buffers_indexed(0x8C8Eu)",
    "glBindImageTexture": "::av::glicd::shaders_write_buffers()",
    "glBindImageTextures": "::av::glicd::shaders_write_buffers()",
    "glMakeBufferResidentNV": "::av::glicd::shaders_write_buffers()",
    "glMakeNamedBufferResidentNV": "::av::glicd::shaders_write_buffers()",
    "glMakeImageHandleResidentARB": "::av::glicd::shaders_write_buffers()",
    "glMakeImageHandleResidentNV": "::av::glicd::shaders_write_buffers()",
    # Restores the array and pixel buffer bindings out of sight.
    "glPopClientAttrib": "::av::glicd::bindings_restored()",
}

# The binding a download writes through when its pointer is an offset.
PACK_TARGET = "0x88EBu"
QUERY_TARGET = "0x9192u"


def _icd_buffer_writes(cmd: Command) -> list[str]:
    """The guest's copies told what this command writes (BUFFER_WRITES)."""
    lines = []
    for p in cmd.params:
        if p.role is Role.PIXELS_OUT:
            target = (QUERY_TARGET if cmd.name.startswith("glGetQueryObject")
                      else PACK_TARGET)
            lines.append(f"    if (av_{p.name}_offset) "
                         f"::av::glicd::host_writes_target({target});")
    if cmd.name in BUFFER_WRITES:
        lines.append(f"    {BUFFER_WRITES[cmd.name]};")
    return lines


# Everything else that can move one of those bindings.
#
# Skipping a bind is only safe while the driver knows what is bound, so
# a command that can change a binding without naming it in the way the
# three above do drops the shadow entirely. They are rare — one
# measured run deleted textures 489 times against seven million binds —
# so forgetting everything costs nothing and needs no bookkeeping to be
# right.
#
# glBindVertexArray is deliberately absent: vertex array objects hold
# buffer bindings, not texture units. wglMakeCurrent is not here
# because it is not a GL command; it forgets them itself.
BIND_SHADOW_FORGETS = (
    # Bind several at once, or by unit, without going through the three.
    "glBindTextures", "glBindSamplers", "glBindTextureUnit",
    "glBindMultiTextureEXT",
    # A deleted name is unbound from every unit it was bound to.
    "glDeleteTextures", "glDeleteSamplers",
    # Restores bindings saved by glPushAttrib(GL_TEXTURE_BIT).
    "glPopAttrib",
    # A list can contain binds, and executing it does not call them.
    "glCallList", "glCallLists",
)


def _icd_is_handwritten(cmd: Command) -> bool:
    if cmd.name in HANDWRITTEN:
        return True
    if any(cmd.name.startswith(prefix) for prefix in HANDWRITTEN_PREFIXES):
        return True
    return _ret_kind(cmd) in ("string", "pointer")


def _icd_signature(cmd: Command) -> str:
    params = []
    for p in cmd.params:
        if p.role is Role.IGNORED:
            params.append(f"{p.type.cpp()} {p.name}")
        elif p.role is Role.SYNC:
            params.append(f"GLsync {p.name}")
        elif p.role is Role.BUFFER_OFFSET:
            params.append(f"const void* {p.name}")
        elif p.role is Role.OFFSET_ARRAY:
            params.append(f"const void* const* {p.name}")
        else:
            params.append(f"{p.type.cpp()} {p.name}")
    ret = cmd.return_type.cpp()
    return f"{ret} APIENTRY {cmd.name}({', '.join(params) if params else 'void'})"


def _icd_prologue(cmd: Command) -> list[str]:
    """What the driver has to look up before it can forward the call."""
    lines = []
    for p in cmd.params:
        if p.role is Role.PIXELS_IN or p.role is Role.PIXELS_OUT:
            # Which binding decides whether the pointer is an address.
            # For a pixel transfer it is the pixel buffer for the
            # direction the transfer goes; for a query result it is the
            # query buffer, which is the same rule about a different
            # binding.
            if cmd.name.startswith("glGetQueryObject"):
                bound = "::av::glicd::query_buffer_bound()"
            else:
                packing = "true" if p.role is Role.PIXELS_OUT else "false"
                bound = f"::av::glicd::pixel_buffer_bound({packing})"
            lines.append(f"    const bool av_{p.name}_offset = {bound};")
    return lines


def _icd_forward_args(cmd: Command) -> list[str]:
    args = []
    for p in cmd.params:
        if p.role is Role.IGNORED:
            continue
        if p.role is Role.SYNC:
            args.append(f"reinterpret_cast<uint64_t>({p.name})")
        elif p.role is Role.BUFFER_OFFSET:
            args.append(f"static_cast<uint64_t>("
                        f"reinterpret_cast<uintptr_t>({p.name}))")
        elif p.role is Role.OFFSET_ARRAY:
            args.append(f"::av::glicd::offsets_of({p.name}, "
                        f"static_cast<size_t>({p.length.text}))")
        elif p.role is Role.PIXELS_IN or p.role is Role.PIXELS_OUT:
            # An offset needs no length, and working one out would ask
            # the host how big a texture level is for an answer that
            # is then thrown away.
            args.append(p.name)
            if _stated_size(cmd) is None:
                args.append(
                    f"av_{p.name}_offset ? 0 : {_icd_pixel_bytes(cmd, p)}")
            args.append(f"av_{p.name}_offset")
        else:
            args.append(p.name)
    return args


def _icd_pixel_bytes(cmd: Command, p: Param) -> str:
    """How many bytes of pixels this call carries, in the guest's terms."""
    names = {q.name for q in cmd.params}
    # Where the caller states the room there is nothing to work out,
    # and working it out anyway would be a second answer to a question
    # already answered.
    if "bufSize" in names:
        return "static_cast<size_t>(bufSize)"
    reads = p.length.reads if p.length is not None else ()
    width = "width" if "width" in names else "1"
    height = "height" if "height" in names else "1"
    depth = "depth" if "depth" in names else "1"
    fmt = "format" if "format" in names else "0"
    typ = "type" if "type" in names else "0"
    packing = "true" if p.role is Role.PIXELS_OUT else "false"
    if "imageSize" in names:
        # A compressed image: its length is stated, its beginning is
        # not. What crosses starts at the caller's pointer and runs far
        # enough that the host, applying the same skips, lands inside
        # what it was given.
        return (f"::av::glicd::compressed_bytes({width}, {height}, {depth}, "
                f"imageSize, {packing})")
    if "Compressed" in cmd.name:
        # glGetCompressedTexImage: neither the extent nor the format
        # says how many bytes a compressed level is. The host is asked
        # for that, and the pack state says where in the caller's
        # memory it lands.
        #
        # The direct-state-access spellings ask the same question and
        # call the level "lod", so the level is taken by whichever name
        # this command gave it.
        level = "level" if "level" in names else "lod"
        return (f"::av::glicd::compressed_level_bytes(target, {level}, "
                f"{packing})")
    if "pname" in names and "format" not in names and "type" not in names:
        # A query answered by enum: how many values it is worth is the
        # same table every other such query uses, and how wide each one
        # is the parameter's own type says.
        return (f"::av::glproto::pname_count(*av_t, pname) * "
                f"sizeof({p.type.base})")
    if "target" in reads and "level" in reads:
        # glGetTexImage: the extent is the level's, which only the
        # driver knows.
        return (f"::av::glicd::level_bytes(target, level, {fmt}, {typ})")
    return (f"::av::glicd::pixel_bytes({fmt}, {typ}, {width}, {height}, "
            f"{depth}, {packing})")


def _framebuffer_names(cmd) -> list[str]:
    """Parameters that name a framebuffer the application may call zero.

    The direct-state-access family says which framebuffer it means
    instead of binding it, so zero appears as an argument rather than as
    a binding — and zero is the window, which on this host is a
    framebuffer object with a name of its own.

    glIsFramebuffer is left alone on purpose: an application asking
    whether zero is a framebuffer is entitled to be told no.
    """
    if cmd.name == "glIsFramebuffer" or cmd.name == "glBindFramebuffer":
        return []
    wanted = {"framebuffer", "srcFramebuffer", "dstFramebuffer",
              "readFramebuffer", "drawFramebuffer"}
    return [p.name for p in cmd.params
            if p.name in wanted and p.type.pointer_depth == 0]


def _reads_buffers(cmd) -> bool:
    """Commands that read buffer contents the GPU has to see first.

    Draws and dispatches, which is where a persistently mapped buffer's
    contents must have arrived. Everything else can wait for the unmap
    it is going to get.
    """
    name = cmd.name
    return (name.startswith("glDraw") or name.startswith("glMultiDraw") or
            name.startswith("glDispatchCompute"))


class GenerationError(Exception):
    pass


# The suffix opengl32 gives the colour entries of its RGBA table; the command
# is the one without it.
RGBA_SUFFIX = "_InRGBA"


def read_proc_table(text: str) -> list[str]:
    """The command names of opengl32's GLCLTPROCTABLE, in slot order.

    The file is `<index> <name>` per line from 0 without a gap; `#` starts a
    comment line. Anything else is refused: the order is Microsoft's ABI, and
    a slot out of place calls the wrong function with the wrong arguments."""
    names: list[str] = []
    for number, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        parts = stripped.split()
        if len(parts) != 2 or not parts[0].isdigit():
            raise GenerationError(f"proc table line {number}: want '<index> <name>': {line!r}")
        if int(parts[0]) != len(names):
            raise GenerationError(f"proc table line {number}: slot {parts[0]} where {len(names)} was due")
        name = parts[1]
        if name.endswith(RGBA_SUFFIX):
            name = name[:-len(RGBA_SUFFIX)]
        names.append(name)
    if not names:
        raise GenerationError("proc table: no slots")
    return names


def _icd_entry_declaration(cmd: Command) -> str:
    return _icd_signature(cmd).replace(f" APIENTRY {cmd.name}(", f" APIENTRY Wrap_{cmd.name}(", 1)


def _table_slot(reg: Registry, index: int, name: str) -> str:
    if name in reg.driver_entries:
        return f"reinterpret_cast<GlProc>(&Wrap_{name})"
    cmd = reg.commands.get(name)
    if cmd is None:
        raise GenerationError(
            f"proc table slot {index}: {name} is neither generated nor a driver entry")
    return f"reinterpret_cast<GlProc>(&Wrap_{name})"


def emit_icd(reg: Registry, out_dir: Path, proc_table: list[str]) -> None:
    out = [HEADER, '#include "gl_icd_entries.h"\n',
           "#include <stdint.h>\n",
           '#include "core/trace/function_scope.hpp"',
           '#include "proto/gl_icd_support.hpp"',
           '#include "gl_dispatch_client.h"',
           '#include "gl_helpers.h"\n',
           "// Every entry point whose whole job is to forward.",
           "namespace av::glicd::generated {", ""]
    forwarded: list[str] = []
    handwritten: list[str] = []
    for cmd in reg.commands.values():
        if cmd.api != "gl":
            continue
        if _icd_is_handwritten(cmd):
            handwritten.append(cmd.name)
            continue
        forwarded.append(cmd.name)
        out.append(f"{_icd_entry_declaration(cmd)} {{")
        # One trace scope per API call (docs/trace.md).
        out.append(f'    AV_SCOPE("{cmd.name}");')
        if cmd.tier == "compat":
            # A compatibility command exists only where the host granted a
            # compatibility context: on a core one it is the GL_INVALID_OPERATION
            # a core context records for an entry point its profile does not have
            # (feature 051, research.md §3).
            zero = {"void": "return;", "sync": "return nullptr;"}.get(
                _ret_kind(cmd), "return 0;")
            out.append("    if (!::av::glicd::legacy_calls_live()) {")
            out.append("        ::av::glicd::refuse_legacy();")
            out.append(f"        {zero}")
            out.append("    }")
        args = ", ".join(["*av_t"] + _icd_forward_args(cmd))
        out.append("    auto* av_t = ::av::glicd::wire();")
        for rewritten in _framebuffer_names(cmd):
            # Zero names the window, and the window is a framebuffer
            # object here. See av::glicd::framebuffer_name.
            out.append(f"    {rewritten} = "
                       f"::av::glicd::framebuffer_name({rewritten});")
        if _reads_buffers(cmd):
            # A persistent mapping is memory the application writes into
            # and never unmaps, so nothing tells this driver the bytes
            # changed. What the specification promises is that the write
            # is visible to the next draw — so the next draw is where it
            # is sent. See av::glicd::flush_persistent_mappings.
            out.append("    ::av::glicd::flush_persistent_mappings();")
        if cmd.name in BIND_SHADOW_FORGETS:
            out.append("    ::av::glicd::forget_bindings();")
        kind = _ret_kind(cmd)
        prologue = _icd_prologue(cmd)
        if kind == "void":
            out.append("    if (av_t == nullptr) return;")
            if cmd.name in BIND_SHADOW:
                out.append(f"    if ({BIND_SHADOW[cmd.name]}) return;")
            if cmd.name in ANSWERED_BY_GUEST_NAMES:
                out.append(f"    if ({ANSWERED_BY_GUEST_NAMES[cmd.name]}) return;")
            out += prologue
            out += _icd_buffer_writes(cmd)
            out.append(f"    ::av::glproto::av_{cmd.name}({args});")
        elif kind == "sync":
            out.append("    if (av_t == nullptr) return nullptr;")
            out += prologue
            out += _icd_buffer_writes(cmd)
            out.append(f"    return reinterpret_cast<GLsync>("
                       f"static_cast<uintptr_t>("
                       f"::av::glproto::av_{cmd.name}({args})));")
        else:
            out.append(f"    if (av_t == nullptr) return 0;")
            out += prologue
            out += _icd_buffer_writes(cmd)
            out.append(f"    return ::av::glproto::av_{cmd.name}({args});")
        out.append("}")
        out.append("")
    out += ["}  // namespace av::glicd::generated", ""]
    write(out_dir / "gl_icd_entries.cpp", "\n".join(out))

    # Internal names only: opengl32 reaches an ICD through the table and
    # DrvGetProcAddress, and a GL symbol of the ICD's own could only collide
    # with the one the application links.
    header = [HEADER, "#pragma once\n",
              '#include "gl_types.h"\n',
              "// Declared for the driver's own use; defined either by the",
              "// generated file beside this one or by the driver itself.",
              "namespace av::glicd::generated {", ""]
    for cmd in reg.commands.values():
        if cmd.api != "gl":
            continue
        header.append(f"{_icd_entry_declaration(cmd)};")
    header += ["", "// The driver's own, which the wire does not carry."]
    for name, (returns, params) in sorted(reg.driver_entries.items()):
        header.append(f"{returns} APIENTRY Wrap_{name}({params or 'void'});")
    header += ["", "}  // namespace av::glicd::generated", ""]
    write(out_dir / "gl_icd_entries.h", "\n".join(header))

    in_table = set(proc_table)
    table = [HEADER, "#pragma once\n",
             "#include <stdint.h>\n",
             '#include "gl_icd_entries.h"\n',
             "namespace av::glicd {\n",
             "using GlProc = void (APIENTRY *)();\n",
             "}  // namespace av::glicd\n",
             "namespace av::glicd::generated {\n",
             "using ::av::glicd::GlProc;\n",
             "// opengl32's GLCLTPROCTABLE: DrvSetContext answers it, and",
             "// __wglSetProcTable takes it only at exactly this many entries.",
             f"inline constexpr uint32_t kProcTableSlots = {len(proc_table)}u;\n",
             "struct GlProcTable {",
             "    int32_t entries;",
             "    GlProc slots[kProcTableSlots];",
             "};\n",
             "inline void fill_proc_table(GlProcTable& table) noexcept {",
             "    table.entries = static_cast<int32_t>(kProcTableSlots);"]
    for index, name in enumerate(proc_table):
        table.append(f"    table.slots[{index}] = {_table_slot(reg, index, name)};")
    table += ["}", "",
              "// Every other entry, which DrvGetProcAddress answers by name.",
              "struct NamedEntry {",
              "    const char* name;",
              "    GlProc address;",
              "};",
              "",
              "inline const NamedEntry kNamedEntries[] = {"]
    named = 0
    for cmd in reg.commands.values():
        if cmd.api != "gl" or cmd.name in in_table:
            continue
        table.append(f'    {{"{cmd.name}", reinterpret_cast<GlProc>(&Wrap_{cmd.name})}},')
        named += 1
    for name in sorted(reg.driver_entries):
        if name in in_table:
            continue
        table.append(f'    {{"{name}", reinterpret_cast<GlProc>(&Wrap_{name})}},')
        named += 1
    # The same entry points under the names an extension gave them.
    # A loader asking for glDebugMessageInsertARB is asking for
    # glDebugMessageInsert; answering nothing leaves it a null pointer
    # to call.
    for name, target in sorted(reg.aliases.items()):
        table.append(f'    {{"{name}", reinterpret_cast<GlProc>(&Wrap_{target})}},')
        named += 1
    table += ["};", "",
              f"inline constexpr uint32_t kNamedEntryCount = {named}u;",
              "",
              "}  // namespace av::glicd::generated", ""]
    write(out_dir / "gl_icd_table.h", "\n".join(table))

    print(f"  icd: {len(forwarded)} entry points forwarded, "
          f"{len(handwritten)} left to the driver, "
          f"{len(proc_table)} table slots, {named} by name")


def emit_all(reg: Registry, out_dir: Path, enum_values: dict[str, int],
             proc_table: list[str]) -> None:
    _ENUM_VALUES.update(enum_values)
    out_dir.mkdir(parents=True, exist_ok=True)
    emit_types(reg, out_dir)
    emit_ids(reg, out_dir)
    emit_names(reg, out_dir)
    emit_sizes(reg, out_dir)
    emit_client(reg, out_dir)
    emit_server(reg, out_dir)
    emit_real_dispatch(reg, out_dir)
    emit_tier(reg, out_dir)
    emit_icd(reg, out_dir, proc_table)
