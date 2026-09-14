"""C++ for the OpenCL half of the wire.

Seven files, all from the same lowered registry:

  cl_protocol_ids.h    the id constants
  cl_command_names.*   an id to a name, for the log
  cl_dispatch_client.* one wrapper per command, encoding and sending
  cl_dispatch_server.* the interface the server implements, and the
                       decode-and-call table
  cl_default_dispatch.h that interface answered with no OpenCL, for tests
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
HANDLE_T = "uint64_t"

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


# ---------------------------------------------------------------------
# What the server refuses before the host's OpenCL sees it
#
# A handle crosses as a number, and on the host the ICD dereferences
# whatever it is handed. So every number a guest sends where an object
# belongs is looked up, by kind, in what this server made (feature
# 044), and a number that is not one is a command the server refuses.
# The same goes for a count the ICD reads past what crossed, and for the
# few values that are host pointers by definition.
# ---------------------------------------------------------------------

def _kind(base: str) -> str:
    """The HandleKind enumerator for an OpenCL object type."""
    name = base[len("cl_"):]
    return name[:-len("_id")] if name.endswith("_id") else name


def _kinds(reg: Registry) -> list[str]:
    """Every object type a carried command names, returns or holds."""
    used = {p.element or p.type.base for cmd in reg.commands.values() for p in cmd.params
            if p.role in (Role.HANDLE, Role.HANDLE_ARRAY, Role.HANDLE_OUT,
                          Role.HANDLE_ARRAY_OUT)}
    used |= {cmd.return_type.base for cmd in reg.commands.values() if cmd.returns_handle}
    return sorted(used)


# Handles inside a struct the wire carries as bytes. cl.xml puts
# cl_image_desc's buffer in an anonymous union and names no field, so it
# is written down here.
STRUCT_HANDLES = {
    "cl_image_desc": (("buffer", "mem"),),
}

# Context properties are an allow-list because most of the rest are a
# host pointer by definition - a GL context, an EGL or X display, a D3D
# device - which no guest can know. CL_CONTEXT_PLATFORM names an object
# and is looked up like any other handle.
CONTEXT_PROPERTY_SCALARS = ("CL_CONTEXT_INTEROP_USER_SYNC",)

# Memory properties whose value is a list of devices rather than a scalar
# (cl_khr_external_memory's CL_MEM_DEVICE_HANDLE_LIST_KHR). cl_ext.h is
# not included here, so it is spelled as its value.
MEM_PROPERTY_REFUSED = (("0x2051", "CL_MEM_DEVICE_HANDLE_LIST_KHR"),)

# clSetKernelExecInfo names that pass an array of SVM or USM pointers.
# This wire carries no allocation of either, so any such pointer is one
# the guest invented.
EXEC_INFO_POINTERS = (("CL_KERNEL_EXEC_INFO_SVM_PTRS", ""),
                      ("0x40B8", "CL_KERNEL_EXEC_INFO_SVM_PTRS_ARM"),
                      ("0x4203", "CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL"))

# The implicit transfer sizes the server works out itself, from the
# arguments and, for an image, the image's own element size. An image
# created from a host pointer is not among them: its element size is the
# format's, before any image exists to ask.
TRANSFER_PITCHES = {
    "clEnqueueReadImage": ("image", "region", "row_pitch", "slice_pitch"),
    "clEnqueueWriteImage": ("image", "region", "input_row_pitch", "input_slice_pitch"),
    "clEnqueueReadBufferRect": ("host_origin", "region", "host_row_pitch", "host_slice_pitch"),
    "clEnqueueWriteBufferRect": ("host_origin", "region", "host_row_pitch", "host_slice_pitch"),
}

# The four float, int or uint components clEnqueueFillImage reads.
FILL_COLOR_BYTES = 16


def _decode_property_keys(p: Param, out: list[str]) -> None:
    """Looks up the handles a property list holds, refusing the keys whose
    value is a host pointer."""
    if p.type.base == "cl_context_properties":
        allowed = " && ".join(f"wire_items[i] != {key}" for key in CONTEXT_PROPERTY_SCALARS)
        out.append("            for (uint64_t i = 0; wire_items[i] != 0; i += 2) {")
        out.append("                if (wire_items[i] == CL_CONTEXT_PLATFORM) {")
        out.append(f"                    const {HANDLE_T} wire_handle = static_cast<{HANDLE_T}>(wire_items[i + 1]);")
        out.append("                    void* wire_platform = s.handle_for(HandleKind::platform, wire_handle);")
        out.append("                    if (wire_platform == nullptr) return false;")
        out.append("                    wire_items[i + 1] = static_cast<cl_context_properties>("
                   "reinterpret_cast<intptr_t>(wire_platform));")
        out.append(f"                }} else if ({allowed}) {{")
        out.append("                    return false;")
        out.append("                }")
        out.append("            }")
    elif p.type.base == "cl_mem_properties":
        out.append("            for (uint64_t i = 0; wire_items[i] != 0; i += 2) {")
        for value, name in MEM_PROPERTY_REFUSED:
            out.append(f"                if (wire_items[i] == {value}) return false;  // {name}")
        out.append("            }")


def _count_of(cmd: Command, len_param: str | None) -> str | None:
    """How many the ICD reads, as an expression, or None when the profile
    leaves it to a helper."""
    if len_param is None or len_param.startswith("@"):
        return None
    if len_param.isdigit():
        return f"{len_param}u"
    names = {q.name for q in cmd.params if q.role == Role.SCALAR}
    if len_param not in names:
        raise ValueError(f"{cmd.name}: {len_param} counts an array but is not a scalar")
    return f"static_cast<uint64_t>({len_param})"


def _refusals(cmd: Command) -> list[str]:
    """Checks between decoding a command and calling it: whatever the ICD
    would read or write past what crossed, and the flags and names that
    make it keep or follow a pointer."""
    out: list[str] = []
    for p in cmd.params:
        n = p.name
        count = _count_of(cmd, p.len_param)
        if p.role in (Role.HANDLE_ARRAY, Role.SCALAR_ARRAY, Role.STRING_ARRAY, Role.BYTES_IN) \
                and count is not None:
            out.append(f"    if ({n} != nullptr && {n}_count < {count}) return false;")
        elif p.role == Role.BYTES_OUT and count is not None:
            out.append(f"    if ({n}_wanted && {count} > static_cast<uint64_t>({n}_capacity)) return false;")
        elif p.role == Role.BYTES_OUT and p.len_param == "@image_format_bytes":
            out.append(f"    if ({n}_wanted && static_cast<uint64_t>(num_entries) * sizeof(cl_image_format) >"
                       f" static_cast<uint64_t>({n}_capacity)) return false;")
        elif p.role in (Role.BYTES_IN, Role.BYTES_OUT) and cmd.name in TRANSFER_PITCHES:
            first, region, row, slice_ = TRANSFER_PITCHES[cmd.name]
            helper = "image_transfer_bytes(s, arena, " if first == "image" else "rect_bytes("
            present = f"{n} != nullptr" if p.role == Role.BYTES_IN else f"{n}_wanted"
            room = f"{n}_count" if p.role == Role.BYTES_IN else f"static_cast<uint64_t>({n}_capacity)"
            out.append("    {")
            out.append("        uint64_t wire_needed = 0;")
            out.append(f"        if ({present} && (!{helper}{first}, {region}, {row}, {slice_}, wire_needed) ||")
            out.append(f"                         {room} < wire_needed)) return false;")
            out.append("    }")
        elif p.role == Role.BYTES_IN and p.len_param == "@fill_bytes":
            out.append(f"    if ({n} != nullptr && {n}_count < {FILL_COLOR_BYTES}u) return false;")
        elif p.role == Role.BYTES_IN:
            # An image made from a host pointer: its size is the format's
            # element size over the description's extent, which this server
            # does not work out yet, so the ICD reads what the guest sized.
            out.append(f"    (void){n}_count;")
    if any(q.role == Role.BYTES_IN for q in cmd.params) and \
            any(q.name == "flags" and q.type.base == "cl_mem_flags" for q in cmd.params):
        # The bytes crossed into a block the next command reuses; an ICD
        # told to use them in place would go on reading and writing it.
        out.append("    if ((flags & CL_MEM_USE_HOST_PTR) != 0) return false;")
    if cmd.name == "clSetKernelExecInfo":
        for value, name in EXEC_INFO_POINTERS:
            spelled = f"  // {name}" if name else ""
            out.append(f"    if (param_name == {value}) return false;{spelled}")
    return out


# Emitted once, above the decoders that call them.
SIZE_HELPERS = [
    "// How far into a host pointer a transfer reaches: `offset`, then whole",
    "// rows and slices at the pitches given, and the last row only as far as",
    "// the region does. 0 for an empty region, which the ICD refuses without",
    "// touching anything; false for a size that overflows.",
    "static bool pitched_reach(const size_t* region, uint64_t element, uint64_t row,",
    "                          uint64_t slice, uint64_t offset, uint64_t& bytes) {",
    "    bytes = 0;",
    "    if (region[0] == 0 || region[1] == 0 || region[2] == 0) return true;",
    "    uint64_t rows = 0;",
    "    uint64_t slices = 0;",
    "    uint64_t last_row = 0;",
    "    return !__builtin_mul_overflow(row, static_cast<uint64_t>(region[1] - 1), &rows) &&",
    "           !__builtin_mul_overflow(slice, static_cast<uint64_t>(region[2] - 1), &slices) &&",
    "           !__builtin_mul_overflow(static_cast<uint64_t>(region[0]), element, &last_row) &&",
    "           !__builtin_add_overflow(offset, rows, &bytes) &&",
    "           !__builtin_add_overflow(bytes, slices, &bytes) &&",
    "           !__builtin_add_overflow(bytes, last_row, &bytes);",
    "}",
    "",
    "// A buffer rectangle: bytes, from the host origin, at the pitches the",
    "// specification defaults a 0 to.",
    "static bool rect_bytes(const size_t* origin, const size_t* region, size_t row_pitch,",
    "                       size_t slice_pitch, uint64_t& bytes) {",
    "    if (origin == nullptr || region == nullptr) return false;",
    "    const uint64_t row = row_pitch != 0 ? row_pitch : region[0];",
    "    uint64_t slice = slice_pitch;",
    "    if (slice == 0 && __builtin_mul_overflow(static_cast<uint64_t>(region[0]),",
    "                                             static_cast<uint64_t>(region[1]), &slice)) return false;",
    "    uint64_t across = 0;",
    "    uint64_t down = 0;",
    "    uint64_t offset = 0;",
    "    if (__builtin_mul_overflow(static_cast<uint64_t>(origin[1]), row, &across) ||",
    "        __builtin_mul_overflow(static_cast<uint64_t>(origin[2]), slice, &down) ||",
    "        __builtin_add_overflow(across, down, &offset) ||",
    "        __builtin_add_overflow(offset, static_cast<uint64_t>(origin[0]), &offset)) return false;",
    "    return pitched_reach(region, 1, row, slice, offset, bytes);",
    "}",
    "",
    "// An image transfer: elements of the image's own format, which the ICD",
    "// is asked for rather than the guest - into the arena, like every other",
    "// answer the ICD writes.",
    "static bool image_transfer_bytes(ServerDispatch& s, ::av::WireArena& arena, cl_mem image,",
    "                                 const size_t* region, size_t row_pitch, size_t slice_pitch,",
    "                                 uint64_t& bytes) {",
    "    if (region == nullptr) return false;",
    "    auto* element = arena.allocate<size_t>(1);",
    "    if (element == nullptr ||",
    "        s.on_clGetImageInfo(image, CL_IMAGE_ELEMENT_SIZE, sizeof *element, element, nullptr) != CL_SUCCESS ||",
    "        *element == 0) return false;",
    "    uint64_t row = row_pitch;",
    "    if (row == 0 && __builtin_mul_overflow(static_cast<uint64_t>(region[0]),",
    "                                           static_cast<uint64_t>(*element), &row)) return false;",
    "    uint64_t slice = slice_pitch;",
    "    if (slice == 0 && __builtin_mul_overflow(row, static_cast<uint64_t>(region[1]), &slice)) return false;",
    "    return pitched_reach(region, *element, row, slice, 0, bytes);",
    "}",
    "",
]


def _succeeded(cmd: Command) -> str:
    """Whether the call worked, in the decoder's scope after it."""
    if cmd.returns_handle:
        return "ret != nullptr"
    if cmd.return_type.base == "cl_int" and cmd.return_type.pointer_depth == 0:
        return "ret == CL_SUCCESS"
    return "true"


def _lifecycle(cmd: Command) -> tuple[str, Param] | None:
    """("retained" or "released", the object) for a reference-counting call."""
    handles = [q for q in cmd.params if q.role == Role.HANDLE]
    if len(handles) != 1 or cmd.return_type.base != "cl_int":
        return None
    if cmd.name.startswith("clRetain"):
        return "retained", handles[0]
    if cmd.name.startswith("clRelease"):
        return "released", handles[0]
    return None


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
        decl.append(f"size_t {p.name}_bytes")
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
    params = ["::av::ClientTransport& t"]
    for p in cmd.params:
        if p.role == Role.IGNORED:
            # Not on the wire, but a string array's lengths are read by
            # the encoder, so the caller still passes them.
            if _lengths_owner(cmd, p) is not None:
                params.append(f"const size_t* {p.name}")
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
        out.append(f"    b.write_varint(static_cast<uint64_t>({n}));")
    elif p.role == Role.HANDLE:
        out.append(f"    b.write_pod<{HANDLE_T}>({n});")
    elif p.role == Role.HANDLE_ARRAY:
        count = p.len_param
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append(f"        b.write_varint(static_cast<uint64_t>({count}));")
        out.append(f"        for (uint64_t i = 0; i < static_cast<uint64_t>({count}); ++i) {{")
        out.append(f"            b.write_pod<{HANDLE_T}>({n}[i]);")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.SCALAR_ARRAY:
        count = p.len_param
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append(f"        b.write_varint(static_cast<uint64_t>({count}));")
        out.append(f"        for (uint64_t i = 0; i < static_cast<uint64_t>({count}); ++i) {{")
        out.append(f"            b.write_varint(static_cast<uint64_t>({n}[i]));")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.STRING:
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append(f"        const size_t wire_len = __builtin_strlen({n});")
        out.append("        b.write_varint(wire_len);")
        out.append(f"        b.write({n}, wire_len);")
        out.append("    }")
    elif p.role == Role.STRING_ARRAY:
        count = p.len_param
        lengths = p.lengths_param
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append(f"        b.write_varint(static_cast<uint64_t>({count}));")
        out.append(f"        for (uint64_t i = 0; i < static_cast<uint64_t>({count}); ++i) {{")
        if lengths:
            out.append(f"            const size_t wire_len = ({lengths} != nullptr && {lengths}[i] != 0)")
            out.append(f"                ? {lengths}[i]")
            out.append(f"                : ({n}[i] != nullptr ? __builtin_strlen(reinterpret_cast<const char*>({n}[i])) : 0);")
        else:
            out.append(f"            const size_t wire_len = {n}[i] != nullptr ? __builtin_strlen({n}[i]) : 0;")
        out.append("            b.write_varint(wire_len);")
        out.append(f"            if (wire_len != 0) b.write({n}[i], wire_len);")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.BYTES_IN:
        size = f"{n}_bytes" if (p.len_param or "").startswith("@") else p.len_param
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append(f"        b.write_varint(static_cast<uint64_t>({size}));")
        out.append(f"        b.write({n}, static_cast<size_t>({size}));")
        out.append("    }")
    elif p.role == Role.STRUCT_IN:
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) b.write({n}, sizeof(*{n}));")
    elif p.role == Role.PROPERTIES:
        # Zero-terminated key/value pairs. Counted here so the far side
        # does not have to trust a terminator that may not be there.
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append("        uint64_t wire_count = 0;")
        out.append(f"        while ({n}[wire_count] != 0) wire_count += 2;")
        out.append("        b.write_varint(wire_count);")
        out.append("        for (uint64_t i = 0; i < wire_count; ++i) {")
        out.append(f"            b.write_pod<{HANDLE_T}>({n}[i]);")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.PROPERTY_LIST:
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) {{")
        out.append("        uint64_t wire_count = 0;")
        out.append(f"        while ({n}[wire_count] != 0) ++wire_count;")
        out.append("        b.write_varint(wire_count);")
        out.append("        for (uint64_t i = 0; i < wire_count; ++i) {")
        out.append(f"            b.write_pod<{HANDLE_T}>(static_cast<{HANDLE_T}>({n}[i]));")
        out.append("        }")
        out.append("    }")
    elif p.role in (Role.HANDLE_OUT, Role.SCALAR_OUT, Role.STRUCT_OUT,
                    Role.HANDLE_ARRAY_OUT, Role.INFO_VALUE):
        # The byte says whether the caller wants it; the server skips
        # producing what nobody asked for.
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
    elif p.role == Role.BYTES_OUT:
        size = f"{n}_bytes" if (p.len_param or "").startswith("@") else p.len_param
        out.append(f"    b.write_pod<uint8_t>({n} != nullptr ? 1 : 0);")
        out.append(f"    if ({n} != nullptr) b.write_varint(static_cast<uint64_t>({size}));")
    else:
        raise AssertionError(f"no encoder for {p.role}")


def _decode_reply(reg: Registry, cmd: Command, out: list[str]) -> None:
    """Client side: pull the return value and the out parameters back."""
    if cmd.returns_handle:
        out.append("    " + HANDLE_T + " ret = 0;")
        out.append("    if (!r.read_pod(ret)) return 0;")
    elif not cmd.returns_void:
        out.append(f"    {cmd.return_type.cpp()} ret{{}};")
        out.append("    int64_t ret_wire = 0;")
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
            out.append("        uint64_t wire_value = 0;")
            out.append(f"        if (!r.read_varint(wire_value)) {fail}")
            out.append(f"        *{n} = static_cast<{p.type.base}>(wire_value);")
        elif p.role == Role.STRUCT_OUT:
            out.append(f"        if (!r.read({n}, sizeof(*{n}))) {fail}")
        elif p.role == Role.HANDLE_ARRAY_OUT:
            out.append("        uint64_t wire_count = 0;")
            out.append(f"        if (!r.read_varint(wire_count)) {fail}")
            out.append("        for (uint64_t i = 0; i < wire_count; ++i) {")
            out.append(f"            if (!r.read_pod({n}[i])) {fail}")
            out.append("        }")
        elif p.role in (Role.BYTES_OUT, Role.INFO_VALUE):
            out.append("        uint64_t wire_count = 0;")
            out.append(f"        if (!r.read_varint(wire_count)) {fail}")
            out.append(f"        if (wire_count != 0 && !r.read({n}, static_cast<size_t>(wire_count))) {fail}")
        else:
            raise AssertionError(f"no reply decoder for {p.role}")
        out.append("    }")


def emit_client(reg: Registry, out_dir: Path) -> None:
    h = [HEADER, "#pragma once\n",
         "#include <stddef.h>", "#include <stdint.h>", ""] + CL_PREAMBLE + ["",
         '#include "proto/client_transport.hpp"', "",
         "namespace av::clproto {", ""]
    c = [HEADER, '#include "cl_dispatch_client.h"', "",
         '#include "proto/wire/wire_arena.hpp"',
         '#include "proto/wire/wire_reader.hpp"',
         '#include "proto/wire/wire_writer.hpp"',
         '#include "cl_protocol_ids.h"', "",
         "namespace av::clproto {", ""]

    for cmd in reg.commands.values():
        sig = _client_signature(reg, cmd)
        h.append(f"{sig};")
        c.append(f"{sig} {{")
        c.append("    ::av::WireWriter b = t.request_writer();")
        for p in cmd.params:
            _encode_param(reg, cmd, p, c)
        if cmd.returns_handle:
            fail = "        return 0;"
        elif cmd.returns_void:
            fail = "        return;"
        else:
            fail = "        return CL_OUT_OF_RESOURCES;"
        if cmd.is_async:
            # A writer that overflowed its slot is a failed call, not a short
            # command on the wire.
            c.append(f"    if (t.send_async(kCmd_{cmd.name}, b) != ::av::TransportResult::ok) {{")
            c.append(fail)
            c.append("    }")
            if cmd.returns_handle:
                c.append("    return 0;")
            elif not cmd.returns_void:
                c.append("    return CL_SUCCESS;")
        else:
            # send_sync points r at the answer, valid only until the next call.
            c.append("    ::av::WireReader r{nullptr, 0u};")
            c.append(f"    if (t.send_sync(kCmd_{cmd.name}, b, r) !=")
            c.append("        ::av::TransportResult::ok) {")
            c.append(fail)
            c.append("    }")
            c.append("    [[maybe_unused]] ::av::WireArena& arena = t.decode_arena();")
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
        out.append("        uint64_t wire_value = 0;")
        out.append("        if (!r.read_varint(wire_value)) return false;")
        out.append(f"        {n} = static_cast<{p.type.base}>(wire_value);")
        out.append("    }")
    elif p.role == Role.HANDLE:
        # Null is the API's own and the ICD answers it with an error code;
        # anything else must be an object this server made, of this kind.
        out.append(f"    {{ {HANDLE_T} wire_handle = 0;")
        out.append("      if (!r.read_pod(wire_handle)) return false;")
        out.append("      if (wire_handle != 0) {")
        out.append(f"          {n} = static_cast<{p.type.base}>("
                   f"s.handle_for(HandleKind::{_kind(p.type.base)}, wire_handle));")
        out.append(f"          if ({n} == nullptr) return false;")
        out.append("      } }")
    elif p.role in (Role.HANDLE_ARRAY, Role.SCALAR_ARRAY):
        elem = p.element if p.role == Role.HANDLE_ARRAY else p.type.base
        out.append("    {")
        out.append("        uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append("            uint64_t wire_count = 0;")
        out.append("            if (!r.read_varint(wire_count)) return false;")
        out.append(f"            auto* wire_items = arena.allocate<{elem}>(static_cast<size_t>(wire_count) + 1);")
        out.append("            if (wire_items == nullptr) return false;")
        out.append("            for (uint64_t i = 0; i < wire_count; ++i) {")
        if p.role == Role.HANDLE_ARRAY:
            # No null inside a list: the ICD dispatches on every element.
            out.append(f"                {HANDLE_T} wire_handle = 0;")
            out.append("                if (!r.read_pod(wire_handle)) return false;")
            out.append(f"                wire_items[i] = static_cast<{elem}>("
                       f"s.handle_for(HandleKind::{_kind(elem)}, wire_handle));")
            out.append("                if (wire_items[i] == nullptr) return false;")
        else:
            out.append("                uint64_t wire_value = 0;")
            out.append("                if (!r.read_varint(wire_value)) return false;")
            out.append(f"                wire_items[i] = static_cast<{elem}>(wire_value);")
        out.append("            }")
        out.append(f"            {n} = wire_items;")
        out.append(f"            {n}_count = wire_count;")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.STRING:
        out.append("    {")
        out.append("        uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append("            uint64_t wire_len = 0;")
        out.append("            if (!r.read_varint(wire_len)) return false;")
        out.append("            char* wire_text = arena.allocate_chars(static_cast<size_t>(wire_len) + 1);")
        out.append("            if (wire_text == nullptr) return false;")
        out.append("            if (wire_len != 0 && !r.read(wire_text, static_cast<size_t>(wire_len))) return false;")
        out.append("            wire_text[wire_len] = '\\0';")
        out.append(f"            {n} = wire_text;")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.STRING_ARRAY:
        out.append("    {")
        out.append("        uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append("            uint64_t wire_count = 0;")
        out.append("            if (!r.read_varint(wire_count)) return false;")
        out.append(f"            auto** wire_items = arena.allocate<const {p.type.base}*>("
                   "static_cast<size_t>(wire_count) + 1);")
        out.append("            if (wire_items == nullptr) return false;")
        keeps_lengths = _lengths_consumer(cmd, p) is not None
        if keeps_lengths:
            out.append("            auto* wire_lengths = arena.allocate<size_t>("
                       "static_cast<size_t>(wire_count) + 1);")
            out.append("            if (wire_lengths == nullptr) return false;")
        out.append("            for (uint64_t i = 0; i < wire_count; ++i) {")
        out.append("                uint64_t wire_len = 0;")
        out.append("                if (!r.read_varint(wire_len)) return false;")
        out.append("                char* wire_text = arena.allocate_chars(static_cast<size_t>(wire_len) + 1);")
        out.append("                if (wire_text == nullptr) return false;")
        out.append("                if (wire_len != 0 && !r.read(wire_text, static_cast<size_t>(wire_len))) return false;")
        out.append("                wire_text[wire_len] = '\\0';")
        out.append(f"                wire_items[i] = reinterpret_cast<const {p.type.base}*>(wire_text);")
        if keeps_lengths:
            out.append("                wire_lengths[i] = static_cast<size_t>(wire_len);")
        out.append("            }")
        out.append(f"            {n} = wire_items;")
        out.append(f"            {n}_count = wire_count;")
        if keeps_lengths:
            out.append(f"            {n}_lengths = wire_lengths;")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.BYTES_IN:
        out.append("    {")
        out.append("        uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append("            uint64_t wire_count = 0;")
        out.append("            if (!r.read_varint(wire_count)) return false;")
        # The reader hands out no view of its own buffer, so the bytes are
        # copied into the caller's block like everything else pointed at.
        out.append("            auto* wire_bytes = arena.allocate<uint8_t>("
                   "static_cast<size_t>(wire_count) + 1);")
        out.append("            if (wire_bytes == nullptr) return false;")
        out.append("            if (wire_count != 0 && !r.read(wire_bytes, "
                   "static_cast<size_t>(wire_count))) return false;")
        out.append(f"            {n} = reinterpret_cast<{p.type.cpp()}>(wire_bytes);")
        out.append(f"            {n}_count = wire_count;")
        out.append("        }")
        out.append("    }")
    elif p.role == Role.STRUCT_IN:
        out.append("    {")
        out.append("        uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append(f"            auto* wire_item = arena.allocate<{p.element}>(1);")
        out.append("            if (wire_item == nullptr) return false;")
        out.append("            if (!r.read(wire_item, sizeof(*wire_item))) return false;")
        for field, kind in STRUCT_HANDLES.get(p.element, ()):
            out.append(f"            if (wire_item->{field} != nullptr) {{")
            out.append(f"                const {HANDLE_T} wire_handle = static_cast<{HANDLE_T}>("
                       f"reinterpret_cast<uintptr_t>(wire_item->{field}));")
            out.append(f"                wire_item->{field} = static_cast<decltype(wire_item->{field})>("
                       f"s.handle_for(HandleKind::{kind}, wire_handle));")
            out.append(f"                if (wire_item->{field} == nullptr) return false;")
            out.append("            }")
        out.append(f"            {n} = wire_item;")
        out.append("        }")
        out.append("    }")
    elif p.role in (Role.PROPERTIES, Role.PROPERTY_LIST):
        out.append("    {")
        out.append("        uint8_t wire_present = 0;")
        out.append("        if (!r.read_pod(wire_present)) return false;")
        out.append("        if (wire_present != 0) {")
        out.append("            uint64_t wire_count = 0;")
        out.append("            if (!r.read_varint(wire_count)) return false;")
        # Two terminators: a list whose last key has no value reads the
        # first as that value and still stops at the second, here and in
        # the ICD, which walks it the same way. The arena refuses a count
        # the addition wraps.
        out.append(f"            auto* wire_items = arena.allocate<{p.type.base}>(static_cast<size_t>(wire_count) + 2);")
        out.append("            if (wire_items == nullptr) return false;")
        out.append("            for (uint64_t i = 0; i < wire_count; ++i) {")
        out.append(f"                {HANDLE_T} wire_value = 0;")
        out.append("                if (!r.read_pod(wire_value)) return false;")
        out.append(f"                wire_items[i] = static_cast<{p.type.base}>(wire_value);")
        out.append("            }")
        out.append("            wire_items[wire_count] = 0;")
        out.append("            wire_items[wire_count + 1] = 0;")
        _decode_property_keys(p, out)
        out.append(f"            {n} = wire_items;")
        out.append("        }")
        out.append("    }")
    elif p.role in (Role.HANDLE_OUT, Role.SCALAR_OUT, Role.STRUCT_OUT,
                    Role.HANDLE_ARRAY_OUT, Role.INFO_VALUE, Role.BYTES_OUT):
        out.append("    {")
        out.append("        uint8_t wire_wanted = 0;")
        out.append("        if (!r.read_pod(wire_wanted)) return false;")
        out.append(f"        {n}_wanted = wire_wanted != 0;")
        if p.role == Role.BYTES_OUT:
            out.append(f"        if ({n}_wanted) {{")
            out.append("            uint64_t cap = 0;")
            out.append("            if (!r.read_varint(cap)) return false;")
            out.append(f"            {n}_capacity = static_cast<size_t>(cap);")
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
                result.append((p, f"const size_t* {p.name}"))
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
         "#include <stddef.h>", "#include <stdint.h>", ""] + CL_PREAMBLE + ["",
         '#include "proto/wire/wire_arena.hpp"',
         '#include "proto/wire/wire_reader.hpp"',
         '#include "proto/wire/wire_writer.hpp"', "",
         "namespace av::clproto {", "",
         "// The objects a guest names by number. Which numbers are objects is",
         "// the server's to say: see ServerDispatch::handle_for.",
         "enum class HandleKind : uint8_t {"]
    h += [f"    {_kind(base)}," for base in _kinds(reg)]
    h += ["};", "",
          "// Implemented by the server; the table below calls into it.",
          "class ServerDispatch {", "public:",
          "    virtual ~ServerDispatch() = default;", "",
          "    // The object of this kind the server made under this number, or",
          "    // null for a number it did not: the decoder refuses the command",
          "    // rather than hand the host's OpenCL a pointer a guest invented.",
          "    virtual void* handle_for(HandleKind kind, uint64_t wire) const = 0;",
          "    // An object a successful call is about to give the guest a number",
          "    // for; from here on that number is the object.",
          "    virtual void made(HandleKind kind, void* object) = 0;",
          "    // A successful retain or release of one. After its last release",
          "    // the number names nothing.",
          "    virtual void retained(HandleKind kind, void* object) = 0;",
          "    virtual void released(HandleKind kind, void* object) = 0;", ""]
    c = [HEADER, '#include "cl_dispatch_server.h"', "",
         "#include <stddef.h>", "#include <stdint.h>", "",
         '#include "cl_protocol_ids.h"', "",
         "namespace av::clproto {", ""] + SIZE_HELPERS

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
                    c.append(f"    const size_t* {p.name} = nullptr;")
                continue
            if p.role == Role.SCALAR:
                c.append(f"    {p.type.base} {p.name}{{}};")
            elif p.role == Role.HANDLE:
                c.append(f"    {p.type.base} {p.name} = nullptr;")
            elif p.role == Role.HANDLE_ARRAY:
                c.append(f"    const {p.element}* {p.name} = nullptr;")
                c.append(f"    uint64_t {p.name}_count = 0;")
            elif p.role == Role.SCALAR_ARRAY:
                c.append(f"    const {p.type.base}* {p.name} = nullptr;")
                c.append(f"    uint64_t {p.name}_count = 0;")
            elif p.role == Role.STRING:
                c.append(f"    const char* {p.name} = nullptr;")
            elif p.role == Role.STRING_ARRAY:
                c.append(f"    const {p.type.base}** {p.name} = nullptr;")
                c.append(f"    uint64_t {p.name}_count = 0;")
                if _lengths_consumer(cmd, p) is not None:
                    c.append(f"    const size_t* {p.name}_lengths = nullptr;")
            elif p.role == Role.BYTES_IN:
                c.append(f"    {p.type.cpp()} {p.name} = nullptr;")
                c.append(f"    uint64_t {p.name}_count = 0;")
            elif p.role == Role.STRUCT_IN:
                c.append(f"    const {p.element}* {p.name} = nullptr;")
            elif p.role in (Role.PROPERTIES, Role.PROPERTY_LIST):
                c.append(f"    const {p.type.base}* {p.name} = nullptr;")
            else:
                c.append(f"    bool {p.name}_wanted = false;")
                if p.role == Role.BYTES_OUT:
                    c.append(f"    size_t {p.name}_capacity = 0;")

        for p in cmd.params:
            _decode_param(reg, cmd, p, c)
        c.extend(_refusals(cmd))

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
                c.append(f"        {n} = arena.allocate<{p.element}>(static_cast<size_t>({cap}) + 1);")
                c.append(f"        if ({n} == nullptr) return false;")
                c.append("    }")
            elif p.role in (Role.INFO_VALUE, Role.BYTES_OUT):
                cap = _capacity_expr(cmd, p)
                ptr_t = "void*" if p.type.base == "void" else f"{p.type.base}*"
                c.append(f"    {ptr_t} {n} = nullptr;")
                c.append(f"    if ({n}_wanted) {{")
                c.append(f"        {n} = reinterpret_cast<{ptr_t}>("
                         f"arena.allocate<uint8_t>(static_cast<size_t>({cap}) + 1));")
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
        succeeded = _succeeded(cmd)
        lifecycle = _lifecycle(cmd)
        if lifecycle is not None:
            hook, obj = lifecycle
            c.append(f"    if ({succeeded}) s.{hook}(HandleKind::{_kind(obj.type.base)}, {obj.name});")

        # Reply: the return value, then what was asked for.
        if cmd.returns_handle:
            c.append(f"    if ({succeeded}) s.made(HandleKind::{_kind(cmd.return_type.base)}, ret);")
            c.append("    reply.write_pod<" + HANDLE_T +
                     ">(static_cast<" + HANDLE_T + ">(reinterpret_cast<uintptr_t>(ret)));")
        elif not cmd.returns_void:
            c.append("    reply.write_varint(static_cast<uint64_t>("
                     "static_cast<int64_t>(ret)));")

        for p in cmd.out_params:
            n = p.name
            c.append(f"    if ({n}_wanted) {{")
            if p.role == Role.HANDLE_OUT:
                c.append(f"        if (({succeeded}) && {n}_storage != nullptr) "
                         f"s.made(HandleKind::{_kind(p.element)}, {n}_storage);")
                c.append(f"        reply.write_pod<{HANDLE_T}>(static_cast<{HANDLE_T}>("
                         f"reinterpret_cast<uintptr_t>({n}_storage)));")
            elif p.role == Role.SCALAR_OUT:
                c.append(f"        reply.write_varint(static_cast<uint64_t>({n}_storage));")
            elif p.role == Role.STRUCT_OUT:
                c.append(f"        reply.write(&{n}_storage, sizeof({n}_storage));")
            elif p.role == Role.HANDLE_ARRAY_OUT:
                # However many the call actually produced, never more
                # than the caller's room for them.
                count = p.size_ret_param
                cap = _capacity_expr(cmd, p)
                if count:
                    c.append(f"        const uint64_t wire_produced = {count} != nullptr")
                    c.append(f"            ? static_cast<uint64_t>(*{count})")
                    c.append(f"            : static_cast<uint64_t>({cap});")
                else:
                    c.append(f"        const uint64_t wire_produced = static_cast<uint64_t>({cap});")
                c.append(f"        const uint64_t wire_sent = wire_produced < static_cast<uint64_t>({cap})")
                c.append(f"            ? wire_produced : static_cast<uint64_t>({cap});")
                c.append("        reply.write_varint(wire_sent);")
                c.append("        for (uint64_t i = 0; i < wire_sent; ++i) {")
                c.append(f"            if (({succeeded}) && {n}[i] != nullptr) "
                         f"s.made(HandleKind::{_kind(p.element)}, {n}[i]);")
                c.append(f"            reply.write_pod<{HANDLE_T}>(static_cast<{HANDLE_T}>("
                         f"reinterpret_cast<uintptr_t>({n}[i])));")
                c.append("        }")
            elif p.role == Role.INFO_VALUE:
                size_ret = p.size_ret_param
                cap = _capacity_expr(cmd, p)
                if size_ret:
                    c.append(f"        uint64_t wire_written = {size_ret} != nullptr")
                    c.append(f"            ? static_cast<uint64_t>(*{size_ret})")
                    c.append(f"            : static_cast<uint64_t>({cap});")
                else:
                    c.append(f"        uint64_t wire_written = static_cast<uint64_t>({cap});")
                c.append(f"        if (wire_written > static_cast<uint64_t>({cap}))")
                c.append(f"            wire_written = static_cast<uint64_t>({cap});")
                c.append("        reply.write_varint(wire_written);")
                c.append(f"        if (wire_written != 0) reply.write({n}, static_cast<size_t>(wire_written));")
            elif p.role == Role.BYTES_OUT:
                cap = _capacity_expr(cmd, p)
                c.append(f"        reply.write_varint(static_cast<uint64_t>({cap}));")
                c.append(f"        if ({cap} != 0) reply.write({n}, static_cast<size_t>({cap}));")
            c.append("    }")

        c.append("    return true;")
        c.append("}")
        c.append("")

    c.append("bool dispatch_command(ServerDispatch& s, uint32_t cmd_id,")
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
          "[[nodiscard]] bool dispatch_command(ServerDispatch& s, uint32_t cmd_id,",
          "                                    ::av::WireReader& r, ::av::WireWriter& reply,",
          "                                    ::av::WireArena& arena);", "",
          "}  // namespace av::clproto", ""]
    c += ["}  // namespace av::clproto", ""]
    write(out_dir / "cl_dispatch_server.h", "\n".join(h))
    write(out_dir / "cl_dispatch_server.cpp", "\n".join(c))



# Vendor and Khronos extension suffixes. A command spelled with one of these is
# resolved through the loader at first use rather than linked, because which
# extension symbols a loader exports is a property of the machine.
_EXTENSION_SUFFIXES = ("KHR", "EXT", "INTEL", "AMD", "APPLE", "ARM", "NV", "QCOM", "IMG")


def _is_extension(name: str) -> bool:
    return name.endswith(_EXTENSION_SUFFIXES)


def _absent_result(cmd) -> str:
    """What a command answers on a loader that does not have it."""
    base = cmd.return_type.base if hasattr(cmd, "return_type") else "cl_int"
    if base == "cl_int":
        return "CL_INVALID_OPERATION"
    return "nullptr"


# How many objects a DefaultDispatch enumeration answers with: more than
# one, so a decoder that only looks at the first is caught.
DEFAULT_ENUMERATED = 2


def _default_body(reg: Registry, cmd: Command) -> list[str]:
    """One DefaultDispatch entry point: report what it was given, then
    succeed."""
    body = [f"        (void){p.name};" for p, _ in _server_params(reg, cmd)]
    body.append(f"        called(kCmd_{cmd.name});")
    for p in cmd.params:
        n = p.name
        count = _count_of(cmd, p.len_param)
        span = f"static_cast<size_t>({count})" if count else None
        if p.role == Role.HANDLE:
            body.append(f"        handed(HandleKind::{_kind(p.type.base)}, {n});")
        elif p.role == Role.HANDLE_ARRAY and span:
            body.append(f"        for (size_t i = 0; {n} != nullptr && i < {span}; ++i) "
                        f"handed(HandleKind::{_kind(p.element)}, {n}[i]);")
        elif p.role in (Role.SCALAR_ARRAY, Role.STRING_ARRAY) and span:
            body.append(f"        if ({n} != nullptr) touched({n}, {span} * sizeof(*{n}));")
        elif p.role in (Role.BYTES_IN, Role.BYTES_OUT) and span:
            body.append(f"        if ({n} != nullptr) touched({n}, {span});")
        elif p.role == Role.INFO_VALUE and p.size_param:
            body.append(f"        if ({n} != nullptr) touched({n}, static_cast<size_t>({p.size_param}));")
        elif p.role == Role.STRUCT_IN:
            for field, kind in STRUCT_HANDLES.get(p.element, ()):
                body.append(f"        if ({n} != nullptr) handed(HandleKind::{kind}, {n}->{field});")
        elif p.role == Role.PROPERTIES and p.type.base == "cl_context_properties":
            body.append(f"        for (size_t i = 0; {n} != nullptr && {n}[i] != 0; i += 2) {{")
            body.append(f"            if ({n}[i] == CL_CONTEXT_PLATFORM) "
                        f"handed(HandleKind::platform, reinterpret_cast<void*>({n}[i + 1]));")
            body.append("        }")
        elif p.role == Role.HANDLE_OUT:
            body.append(f"        if ({n} != nullptr) *{n} = static_cast<{p.element}>("
                        f"produce(HandleKind::{_kind(p.element)}));")
        elif p.role == Role.HANDLE_ARRAY_OUT:
            body.append(f"        for (size_t i = 0; {n} != nullptr && i < static_cast<size_t>({p.len_param}) && "
                        f"i < {DEFAULT_ENUMERATED}u; ++i) {{")
            body.append(f"            {n}[i] = static_cast<{p.element}>(produce(HandleKind::{_kind(p.element)}));")
            body.append("        }")
            if p.size_ret_param:
                body.append(f"        if ({p.size_ret_param} != nullptr) *{p.size_ret_param} = {DEFAULT_ENUMERATED}u;")
        elif p.role == Role.SCALAR_OUT and n == "errcode_ret":
            body.append(f"        if ({n} != nullptr) *{n} = CL_SUCCESS;")
    if cmd.returns_handle:
        body.append(f"        return static_cast<{cmd.return_type.base}>("
                    f"produce(HandleKind::{_kind(cmd.return_type.base)}));")
    elif cmd.return_type.base == "cl_int":
        body.append("        return CL_SUCCESS;")
    elif not cmd.returns_void:
        body.append("        return {};")
    return body


def emit_default_dispatch(reg: Registry, out_dir: Path) -> None:
    """The interface answered with no OpenCL at all, for testing the decoder."""
    out = [HEADER, "#pragma once\n", "#include <stddef.h>", ""] + CL_PREAMBLE + ["",
           '#include "cl_dispatch_server.h"',
           '#include "cl_protocol_ids.h"', "",
           "namespace av::clproto {", "",
           "// Every command answered with no OpenCL behind it, for judging the",
           "// decoder rather than a driver. Each call says which it is to",
           "// `called`, hands every object it was given to `handed` and every",
           "// span it would read or write to `touched` - that is where a fake",
           "// holds the decoder to what it resolved and bounded - then",
           "// succeeds, answering whatever objects it makes with `produce`.",
           "class DefaultDispatch : public ServerDispatch {", "public:"]
    for cmd in reg.commands.values():
        out.append(f"    {_server_signature2(reg, cmd)} override {{")
        out.extend(_default_body(reg, cmd))
        out.append("    }")
    out += ["", "protected:",
            "    virtual void called(uint32_t command_id) = 0;",
            "    virtual void handed(HandleKind kind, void* object) = 0;",
            "    virtual void touched(const void* bytes, size_t length) = 0;",
            "    virtual void* produce(HandleKind kind) = 0;",
            "};", "", "}  // namespace av::clproto", ""]
    write(out_dir / "cl_default_dispatch.h", "\n".join(out))


def emit_real_dispatch(reg: Registry, out_dir: Path) -> None:
    """The interface implemented by calling the host's own OpenCL."""
    out = [HEADER, "#pragma once\n"] + CL_PREAMBLE + ["",
           '#include "cl_dispatch_server.h"', "",
           "namespace av::clproto {", "",
           "// Every command forwarded to whatever OpenCL this process",
           "// loaded. Which numbers are its objects is not this class's to",
           "// say: the server that derives from it answers handle_for.",
           "//",
           "// An extension command is resolved rather than linked. The ICD",
           "// loader on a machine is whichever one is first on the library",
           "// path, and they do not all export the same extension symbols -",
           "// linking clGetGLContextInfoKHR made the whole executor library",
           "// unloadable on a host whose loader had no GL sharing (042).",
           "class RealDispatch : public ServerDispatch {", "public:"]
    members: list[str] = []
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
        if _is_extension(cmd.name):
            pointer = f"pfn_{cmd.name}_"
            members.append(f"    {cmd.name}_fn {pointer} = nullptr;")
            out.append(f"    using {cmd.name}_fn = decltype(&::{cmd.name});")
            out.append(f"    {sig} override {{")
            out.append(f"        if ({pointer} == nullptr) {{")
            out.append(f"            {pointer} = reinterpret_cast<{cmd.name}_fn>(")
            out.append("                ::clGetExtensionFunctionAddress("
                       f'"{cmd.name}"));')
            out.append("        }")
            out.append(f"        if ({pointer} == nullptr) {{")
            if cmd.returns_void:
                out.append("            return;")
            else:
                out.append(f"            return {_absent_result(cmd)};")
            out.append("        }")
            call = f"{pointer}({', '.join(args)})"
            if cmd.returns_void:
                out.append(f"        {call};")
            else:
                out.append(f"        return {call};")
            out.append("    }")
            continue
        call = f"::{cmd.name}({', '.join(args)})"
        out.append(f"    {sig} override {{")
        if cmd.returns_void:
            out.append(f"        {call};")
        else:
            out.append(f"        return {call};")
        out.append("    }")
    out += ["", "private:"] + members
    out += ["};", "", "}  // namespace av::clproto", ""]
    write(out_dir / "cl_real_dispatch.h", "\n".join(out))


def emit_ids(reg: Registry, out_dir: Path) -> None:
    out = [HEADER, "#pragma once\n", "#include <stdint.h>", "",
           "namespace av::clproto {", ""]
    for cmd in reg.commands.values():
        out.append(f"inline constexpr uint32_t kCmd_{cmd.name} = "
                   f"0x{cmd.cmd_id:08X}U;")
    out += ["", "}  // namespace av::clproto", ""]
    write(out_dir / "cl_protocol_ids.h", "\n".join(out))


def emit_names(reg: Registry, out_dir: Path) -> None:
    h = [HEADER, "#pragma once\n", "#include <stdint.h>", "",
         "namespace av::clproto {", "",
         "// A static name for a wire id; \"unknown\" for anything else,",
         "// never null.",
         "[[nodiscard]] const char* cmd_id_to_string(uint32_t cmd_id) noexcept;",
         "", "}  // namespace av::clproto", ""]
    c = [HEADER, '#include "cl_command_names.h"', "",
         '#include "cl_protocol_ids.h"', "",
         "namespace av::clproto {", "",
         "const char* cmd_id_to_string(uint32_t cmd_id) noexcept {",
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
    emit_default_dispatch(reg, out_dir)
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
           '#include "proto/icd_stub_report.hpp"', "",
           "namespace av::clproto {", "",
           "// Every one of these answers CL_INVALID_OPERATION, and says so",
           "// once: an application that asks for something this driver does",
           "// not carry gets a clean refusal, and whoever is reading the log",
           "// gets the name of what it asked for. See",
           "// proto/icd_stub_report.hpp.",
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
        out.append(f'    ::av::icd::stub_called("cl", "{name}", '
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
